#include <k2eg/common/utility.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/metric/IMetricService.h>

#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/GetCommandWorker.h>

#include <client.h>

#include <chrono>
#include <thread>

using namespace k2eg::common;

using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::log;

using namespace k2eg::service::epics_impl;

using namespace k2eg::service::pubsub;

using namespace k2eg::service::metric;

#pragma region GetCommandWorker

GetCommandWorker::GetCommandWorker(EpicsServiceManagerShrdPtr epics_service_manager)
    : logger(ServiceResolver<ILogger>::resolve())
    , publisher(ServiceResolver<IPublisher>::resolve())
    , metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric())
    , epics_service_manager(epics_service_manager)
{
    publisher->setCallBackForReqType("get-reply-message", std::bind(&GetCommandWorker::publishEvtCB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

GetCommandWorker::~GetCommandWorker() {}

void GetCommandWorker::publishEvtCB(pubsub::EventType type, PublishMessage* const msg, const std::string& error_message)
{
    switch (type)
    {
    case OnDelivery: break;
    case OnSent: break;
    case OnError:
        {
            logger->logMessage(STRING_FORMAT("[GetCommandWorker::publishEvtCB] %1%", error_message), LogLevel::ERROR);
            break;
        }
    }
}

void GetCommandWorker::processCommand(std::shared_ptr<BS::light_thread_pool> command_pool, ConstCommandShrdPtr command)
{
    if (command->type != CommandType::get)
    {
        return;
    }

    ConstGetCommandShrdPtr cmd_ptr = static_pointer_cast<const GetCommand>(command);
    logger->logMessage(STRING_FORMAT("Perform get command for %1% on topic %2% with sertype: %3%",
                                     cmd_ptr->pv_name % cmd_ptr->reply_topic % serialization_to_string(cmd_ptr->serialization)),
                       LogLevel::DEBUG);
    auto get_op = epics_service_manager->getChannelData(cmd_ptr->pv_name);
    if (!get_op)
    {
        manageFaultyReply(-1, "PV name malformed", cmd_ptr);
    }
    else
    {
        auto g_ptr = std::make_shared<GetOpInfo>(cmd_ptr, std::move(get_op));
        command_pool->detach_task(
            [this, command_pool, g_ptr]()
            {
                this->checkGetCompletion(command_pool, g_ptr);
            });
    }
}

void GetCommandWorker::manageFaultyReply(const std::int8_t error_code, const std::string& error_message, ConstGetCommandShrdPtr cmd)
{
    logger->logMessage(STRING_FORMAT("%1% [pv:%2%]", error_message % cmd->pv_name), LogLevel::ERROR);
    if (cmd->reply_topic.empty())
    {
        return;
    }
    else
    {
        auto serialized_message = serialize(GetFaultyCommandReply{error_code, cmd->reply_id, error_message}, cmd->serialization);
        if (!serialized_message)
        {
            logger->logMessage("Invalid serialized message", LogLevel::FATAL);
        }
        else
        {
            publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "put-operation", cmd->pv_name, serialized_message), {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
        }
    }
}

void GetCommandWorker::checkGetCompletion(std::shared_ptr<BS::light_thread_pool> command_pool, GetOpInfoShrdPtr get_info)
{
    // check for timeout
    if (get_info->isTimeout())
    {
        manageFaultyReply(-3, "Timeout operation", get_info->cmd);
        return;
    }
    // give some time of relaxing
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    if (!get_info->op->isDone())
    {
        // re-enque the op class
        // command_pool->push_task(&GetCommandWorker::checkGetCompletion, this, command_pool, get_info);
        command_pool->detach_task(
            [this, command_pool, get_info]()
            {
                this->checkGetCompletion(command_pool, get_info);
            });
    }
    else
    {
        switch (get_info->op->getState().event)
        {
        case pvac::GetEvent::Fail:
            {
                manageFaultyReply(-2, get_info->op->getState().message, get_info->cmd);
                break;
            }
        case pvac::GetEvent::Cancel:
            {
                manageFaultyReply(-2, "Operaton cancelled", get_info->cmd);
                break;
            }
        case pvac::GetEvent::Success:
            {
                // update metric
                metric.incrementCounter(IEpicsMetricCounterType::Get);
                logger->logMessage(STRING_FORMAT("Success get command for %1%", get_info->cmd->pv_name), LogLevel::INFO);
                auto channel_data = get_info->op->getChannelData();
                if (!channel_data)
                {
                    logger->logMessage(STRING_FORMAT("No data received for %1%", get_info->cmd->pv_name), LogLevel::ERROR);
                    break;
                }
                auto serialized_message =
                    serialize(GetCommandReply{0, get_info->cmd->reply_id, get_info->op->getChannelData()}, get_info->cmd->serialization);
                if (!serialized_message)
                {
                    logger->logMessage("Invalid serialized message", LogLevel::FATAL);
                    break;
                }
                publisher->pushMessage(
                    MakeReplyPushableMessageUPtr(get_info->cmd->reply_topic, "get-reply-message", get_info->cmd->pv_name, serialized_message),
                    {{"k2eg-ser-type", serialization_to_string(get_info->cmd->serialization)}});
                break;
            }
        }
    }
    // give some time of relaxing
    std::this_thread::sleep_for(std::chrono::microseconds(500));
}

#pragma endregion GetCommandWorker