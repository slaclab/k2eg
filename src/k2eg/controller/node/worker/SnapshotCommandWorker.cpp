#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/SnapshotCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>

#include <iterator>

#include "k2eg/controller/command/cmd/SnapshotCommand.h"
#include "k2eg/service/epics/EpicsData.h"

using namespace k2eg::common;

using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;
using namespace k2eg::controller::node::worker;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::metric;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::epics_impl;

using namespace k2eg::service::epics_impl;

SnapshotCommandWorker::SnapshotCommandWorker(EpicsServiceManagerShrdPtr epics_service_manager)
    : processing_pool(std::make_shared<BS::thread_pool>(1)),
      logger(ServiceResolver<ILogger>::resolve()),
      publisher(ServiceResolver<IPublisher>::resolve()),
      metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric()),
      epics_service_manager(epics_service_manager) {
  publisher->setCallBackForReqType("get-reply-message",
                                   std::bind(&SnapshotCommandWorker::publishEvtCB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

SnapshotCommandWorker::~SnapshotCommandWorker() { processing_pool->wait_for_tasks(); }

void
SnapshotCommandWorker::publishEvtCB(pubsub::EventType type, PublishMessage* const msg, const std::string& error_message) {
  switch (type) {
    case OnDelivery: break;
    case OnSent: break;
    case OnError: {
      logger->logMessage(STRING_FORMAT("[GetCommandWorker::publishEvtCB] %1%", error_message), LogLevel::ERROR);
      break;
    }
  }
}

void
SnapshotCommandWorker::processCommand(ConstCommandShrdPtr command) {
  if (command->type != CommandType::get) { return; }

  ConstSnapshotCommandShrdPtr s_ptr = static_pointer_cast<const SnapshotCommand>(command);

  // get all monitor operation for all pv that need to be part of the snapshot
  std::vector<service::epics_impl::ConstMonitorOperationShrdPtr> v_mon_ops;
  for (const auto& pv_uri : s_ptr->pv_name_list) {
    logger->logMessage(STRING_FORMAT("Perepare snapshot monitor ops for '%1%' on topic %2% with sertype: %3%",
                                     pv_uri % s_ptr->reply_topic % serialization_to_string(s_ptr->serialization)),
                       LogLevel::DEBUG);

    auto mon_op = epics_service_manager->getMonitorOp(pv_uri);
    if (!mon_op) {
      manageFaultyReply(-1, STRING_FORMAT("PV '%1%'name malformed", pv_uri), s_ptr);
      break;
    } else {
      v_mon_ops.push_back(mon_op);
    }
  }
  // submit snapshot to processing pool with a timeout(process window) of 1000 ms
  processing_pool->push_task(&SnapshotCommandWorker::checkGetCompletion, this, std::make_shared<SnapshotOpInfo>(s_ptr, std::move(v_mon_ops), 1000));
}

void
SnapshotCommandWorker::manageFaultyReply(const std::int8_t error_code, const std::string& error_message, ConstSnapshotCommandShrdPtr cmd) {
  logger->logMessage(STRING_FORMAT("Snapshot error%1%", error_message), LogLevel::ERROR);
  if (cmd->reply_topic.empty()) {
    return;
  } else {
    auto serialized_message = serialize(SnapshotFaultyCommandReply{error_code, cmd->reply_id, error_message}, cmd->serialization);
    if (!serialized_message) {
      logger->logMessage("Invalid serialized message", LogLevel::FATAL);
    } else {
      publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "snapshot-operation", "snapshot-dist-key", serialized_message),
                             {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
    }
  }
}

void
SnapshotCommandWorker::checkGetCompletion(SnapshotOpInfoShrdPtr snapshot_info) {
  if (snapshot_info->isTimeout()) {
    // manageFaultyReply(-3, "Timeout operation", snapshot_info->cmd);
    // we comeplted the time window so check if all data is arrived or not
    std::vector<ConstChannelDataShrdPtr> snapshot_data;
    for (std::size_t i = 0; i < snapshot_info->v_mon_ops.size(); ++i) {
      auto m_op = snapshot_info->v_mon_ops[i];
      // Use 'i' as the index if needed
      MonitorEventShrdPtr evt_shrd_ptr;
      // fetch data
      m_op->poll();
      if (!m_op->hasData()) {
        // get last appendend event
        evt_shrd_ptr = m_op->getEventData()->event_data->back();
      } else {
        // force it
        m_op->forceUpdate();
        m_op->poll();
        if (!m_op->getEventData()->event_data->empty()) { evt_shrd_ptr = m_op->getEventData()->event_data->back(); }
      }

      if (evt_shrd_ptr) {
        // Pass the correct member (e.g., pv) from evt_shrd_ptr->channel_data
        publishSnapshotReply(snapshot_info->cmd, i, MakeChannelDataUPtr(evt_shrd_ptr->channel_data));
      } else {
        // TODO what to do here?
      }
      // send message to terminate the operation
      publishEndSnapshotReply(snapshot_info->cmd);  
    }
    // serialzie and forward the message
  }

  // re-enque the op class
  processing_pool->push_task(&SnapshotCommandWorker::checkGetCompletion, this, snapshot_info);

  // give some time of relaxing
  std::this_thread::sleep_for(std::chrono::microseconds(10));
}

void
SnapshotCommandWorker::publishSnapshotReply(ConstSnapshotCommandShrdPtr cmd, std::uint32_t pv_index, service::epics_impl::ConstChannelDataUPtr pv_data) {
  // Pass the correct member (e.g., pv) from evt_shrd_ptr->channel_data
  auto serialized_message = serialize(SnapshotCommandReply{0, cmd->reply_id, static_cast<std::int32_t>(pv_index), std::move(pv_data)}, cmd->serialization);
  if (!serialized_message) {
    logger->logMessage("Invalid serialized message", LogLevel::FATAL);
  } else {
    publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "snapshot-reply-message", "snapshot-dist-key", serialized_message),
                           {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
  }
}

void
SnapshotCommandWorker::publishEndSnapshotReply(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr cmd) {
  auto serialized_message = serialize(SnapshotCommandReply{0, cmd->reply_id, static_cast<std::int32_t>(-1), nullptr}, cmd->serialization);
  if (!serialized_message) {
    logger->logMessage("Invalid serialized message", LogLevel::FATAL);
  } else {
    publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "snapshot-reply-message", "snapshot-dist-key", serialized_message),
                           {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
  }
}