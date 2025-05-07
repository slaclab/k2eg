
#include <k2eg/common/utility.h>

#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/ServiceResolver.h>

#include <k2eg/controller/command/cmd/SnapshotCommand.h>
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>

using namespace k2eg::common;

using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::controller::node::worker;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::metric;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::epics_impl;

SnapshotCommandWorker::SnapshotCommandWorker(EpicsServiceManagerShrdPtr epics_service_manager)
    : logger(ServiceResolver<ILogger>::resolve())
    , publisher(ServiceResolver<IPublisher>::resolve())
    , metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric())
    , epics_service_manager(epics_service_manager)
{
    publisher->setCallBackForReqType("get-reply-message", std::bind(&SnapshotCommandWorker::publishEvtCB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

SnapshotCommandWorker::~SnapshotCommandWorker() {}

void SnapshotCommandWorker::publishEvtCB(pubsub::EventType type, PublishMessage* const msg, const std::string& error_message)
{
    switch (type)
    {
    case OnDelivery: break;
    case OnSent: break;
    case OnError:
        {
            logger->logMessage(STRING_FORMAT("[SnapshotCommandWorker::publishEvtCB] %1%", error_message), LogLevel::ERROR);
            break;
        }
    }
}

void SnapshotCommandWorker::processCommand(std::shared_ptr<BS::light_thread_pool> command_pool, ConstCommandShrdPtr command)
{
    if (command->type != CommandType::snapshot)
    {
        return;
    }

    ConstSnapshotCommandShrdPtr s_ptr = static_pointer_cast<const SnapshotCommand>(command);

    // get all monitor operation for all pv that need to be part of the snapshot
    std::vector<service::epics_impl::ConstMonitorOperationShrdPtr> v_mon_ops;
    for (const auto& pv_uri : s_ptr->pv_name_list)
    {
        logger->logMessage(STRING_FORMAT("Perepare snapshot ops for '%1%' on topic %2% with sertype: %3%",
                                         pv_uri % s_ptr->reply_topic % serialization_to_string(s_ptr->serialization)),
                           LogLevel::DEBUG);

        auto mon_op = epics_service_manager->getMonitorOp(pv_uri);
        if (!mon_op)
        {
            manageFaultyReply(-1, STRING_FORMAT("PV '%1%'name malformed", pv_uri), s_ptr);
            break;
        }
        else
        {
            v_mon_ops.push_back(mon_op);
        }
    }
    // submit snapshot to processing pool
    auto s_op_ptr = std::make_shared<SnapshotOpInfo>(s_ptr, std::move(v_mon_ops));
    command_pool->detach_task(
        [this, command_pool, s_op_ptr]()
        {
            this->checkGetCompletion(command_pool, s_op_ptr);
        });
}

void SnapshotCommandWorker::manageFaultyReply(const std::int8_t error_code, const std::string& error_message, ConstSnapshotCommandShrdPtr cmd)
{
    logger->logMessage(STRING_FORMAT("Snapshot error%1%", error_message), LogLevel::ERROR);
    if (cmd->reply_topic.empty())
    {
        return;
    }
    else
    {
        auto serialized_message = serialize(SnapshotFaultyCommandReply{error_code, cmd->reply_id, error_message}, cmd->serialization);
        if (!serialized_message)
        {
            logger->logMessage("Invalid serialized message", LogLevel::FATAL);
        }
        else
        {
            publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "snapshot-operation", "snapshot-dist-key", serialized_message), {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
        }
    }
}

void SnapshotCommandWorker::checkGetCompletion(std::shared_ptr<BS::light_thread_pool> command_pool, SnapshotOpInfoShrdPtr snapshot_info)
{
    if (!snapshot_info->isTimeout())
    {
        // Process monitors before timeout using processed_index to annotate completed ones.
        for (std::size_t i = 0; i < snapshot_info->v_mon_ops.size(); ++i)
        {
            // check if PV has been already consumed
            if (snapshot_info->processed_index.test(i))
            {
                continue;
            }
            // try to get data from poll
            auto m_op = snapshot_info->v_mon_ops[i];
            m_op->poll();
            if (m_op->hasData())
            {
                // we have data so we can send it to the client
                auto evt_shrd_ptr = m_op->getEventData()->event_data->back();
                publishSnapshotReply(snapshot_info->cmd, static_cast<std::uint32_t>(i), MakeChannelDataUPtr(evt_shrd_ptr->channel_data));
                // set the index as processed
                snapshot_info->processed_index.set(i, true);
            }
        }
        // give some time to relax
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        // check if we have done all the PVs
        if (!snapshot_info->processed_index.all())
        {
            // there still are PVs that have not been received, so resubmit the task
            // command_pool->push_task(&SnapshotCommandWorker::checkGetCompletion, this, command_pool, snapshot_info);
            command_pool->detach_task(
                [this, command_pool, snapshot_info]()
                {
                    this->checkGetCompletion(command_pool, snapshot_info);
                });
        }
        else
        {
            // in this case we have comepleted the snapshot so we can send the completion message to the client before
            // the snapshot
            publishEndSnapshotReply(snapshot_info->cmd);
        }
    }
    else
    { // At timeout, process unhandled monitors.
        for (std::size_t i = 0; i < snapshot_info->v_mon_ops.size(); ++i)
        {
            if (snapshot_info->processed_index.test(i))
            {
                continue;
            }
            // try to get data from poll forcing the update
            auto m_op = snapshot_info->v_mon_ops[i];
            m_op->forceUpdate();
            m_op->poll();
            if (m_op->hasData())
            {
                auto evt_shrd_ptr = m_op->getEventData()->event_data->back();
                publishSnapshotReply(snapshot_info->cmd, static_cast<std::uint32_t>(i), MakeChannelDataShrdPtr(evt_shrd_ptr->channel_data));
            }
            snapshot_info->processed_index.set(i, true);
        }
        // send completion message
        publishEndSnapshotReply(snapshot_info->cmd);
    }
}

void SnapshotCommandWorker::publishSnapshotReply(ConstSnapshotCommandShrdPtr cmd, std::uint32_t pv_index, service::epics_impl::ConstChannelDataShrdPtr pv_data)
{
    // Pass the correct member (e.g., pv) from evt_shrd_ptr->channel_data
    auto serialized_message = serialize(SnapshotCommandReply{0, cmd->reply_id, static_cast<std::int32_t>(pv_index), std::move(pv_data)}, cmd->serialization);
    if (!serialized_message)
    {
        logger->logMessage("Invalid serialized message", LogLevel::FATAL);
    }
    else
    {
        publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "snapshot-reply-message", "snapshot-dist-key", serialized_message), {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
    }
}

void SnapshotCommandWorker::publishEndSnapshotReply(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd)
{
    auto serialized_message = serialize(CommandReply{1, cmd->reply_id}, cmd->serialization);
    if (!serialized_message)
    {
        logger->logMessage("Invalid serialized message", LogLevel::FATAL);
    }
    else
    {
        publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "snapshot-reply-message", "snapshot-dist-key", serialized_message), {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
    }
}