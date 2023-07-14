#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/GetCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>

#include <chrono>
#include <thread>

#include "client.h"
#include "k2eg/controller/node/worker/CommandWorker.h"
#include "k2eg/service/epics/EpicsData.h"
#include "k2eg/service/metric/IMetricService.h"

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
    : processing_pool(std::make_shared<BS::thread_pool>()),
      logger(ServiceResolver<ILogger>::resolve()),
      publisher(ServiceResolver<IPublisher>::resolve()),
      metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric()),
      epics_service_manager(epics_service_manager) {}

GetCommandWorker::~GetCommandWorker() { processing_pool->wait_for_tasks(); }

void
GetCommandWorker::processCommand(ConstCommandShrdPtr command) {
  if (command->type != CommandType::get) { return; }

  ConstGetCommandShrdPtr g_ptr = static_pointer_cast<const GetCommand>(command);
  logger->logMessage(STRING_FORMAT("Perform get command for %1% with protocol %2% on topic %3% with sertype: %4%",
                                   g_ptr->pv_name % g_ptr->protocol % g_ptr->destination_topic % serialization_to_string(g_ptr->serialization)),
                     LogLevel::DEBUG);
  auto channel_data = epics_service_manager->getChannelData(g_ptr->pv_name, g_ptr->protocol);
  processing_pool->push_task(
      &GetCommandWorker::checkGetCompletion,
      this,
      std::make_shared<GetOpInfo>(g_ptr->pv_name, g_ptr->destination_topic, g_ptr->serialization, g_ptr->reply_id, std::move(channel_data)));
}

void
GetCommandWorker::checkGetCompletion(GetOpInfoShrdPtr get_info) {
  // check for timeout
  if (get_info->isTimeout()) {
    logger->logMessage(STRING_FORMAT("Timeout get command for %1%", get_info->pv_name), LogLevel::ERROR);
    auto serialized_message = serialize(GetFaultyCommandReply{-3, get_info->reply_id, "Timeout operation"}, get_info->serialization);
    if (!serialized_message) {
      logger->logMessage("Invalid serialized message", LogLevel::FATAL);
    } else {
      publisher->pushMessage(MakeReplyPushableMessageUPtr(get_info->destination_topic, "get-operation", get_info->pv_name, serialized_message),
                              {{"k2eg-ser-type", serialization_to_string(get_info->serialization)}});
      logger->logMessage(STRING_FORMAT("Sent get faulty reply for reply_id %1%", get_info->reply_id), LogLevel::DEBUG);
    }
    return;
  }
  // give some time of relaxing
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  if (!get_info->op->isDone()) {
    // re-enque the op class
    processing_pool->push_task(&GetCommandWorker::checkGetCompletion, this, get_info);
  } else {
    switch (get_info->op->getState().event) {
      case pvac::GetEvent::Fail: {
        logger->logMessage(STRING_FORMAT("Failed get command for %1% with message %2%", get_info->pv_name % get_info->op->getState().message), LogLevel::ERROR);
        auto serialized_message = serialize(GetFaultyCommandReply{-1, get_info->reply_id, get_info->op->getState().message}, get_info->serialization);
        if (!serialized_message) {
          logger->logMessage("Invalid serialized message", LogLevel::FATAL);
        } else {
          publisher->pushMessage(MakeReplyPushableMessageUPtr(get_info->destination_topic, "get-operation", get_info->pv_name, serialized_message),
                                 {{"k2eg-ser-type", serialization_to_string(get_info->serialization)}});
          logger->logMessage(STRING_FORMAT("Sent get faulty reply for reply_id %1%", get_info->reply_id), LogLevel::DEBUG);
        }
        break;
      }
      case pvac::GetEvent::Cancel: {
        logger->logMessage(STRING_FORMAT("Cancelled get command for %1% with message %2%", get_info->pv_name % get_info->op->getState().message),
                           LogLevel::ERROR);
        auto serialized_message = serialize(GetFaultyCommandReply{-2, get_info->reply_id, get_info->op->getState().message}, get_info->serialization);
        if (!serialized_message) {
          logger->logMessage("Invalid serialized message", LogLevel::FATAL);
        } else {
          publisher->pushMessage(MakeReplyPushableMessageUPtr(get_info->destination_topic, "get-operation", get_info->pv_name, serialized_message),
                                 {{"k2eg-ser-type", serialization_to_string(get_info->serialization)}});
          logger->logMessage(STRING_FORMAT("Sent get faulty reply for reply_id %1%", get_info->reply_id), LogLevel::DEBUG);
        }
        break;
      }
      case pvac::GetEvent::Success: {
        // update metric
        metric.incrementCounter(IEpicsMetricCounterType::Get);
        logger->logMessage(STRING_FORMAT("Success get command for %1%", get_info->pv_name), LogLevel::INFO);
        auto channel_data = get_info->op->getChannelData();
        if (!channel_data) {
          logger->logMessage(STRING_FORMAT("No data received for %1%", get_info->pv_name), LogLevel::ERROR);
          break;
        }
        auto serialized_message = serialize(GetCommandReply{0, get_info->reply_id, get_info->op->getChannelData()}, get_info->serialization);
        if (!serialized_message) {
          logger->logMessage("Invalid serialized message", LogLevel::FATAL);
          break;
        }
        publisher->pushMessage(MakeReplyPushableMessageUPtr(get_info->destination_topic, "get-operation", get_info->pv_name, serialized_message),
                               {{"k2eg-ser-type", serialization_to_string(get_info->serialization)}});
        break;
      }
    }
  }
}
#pragma endregion GetCommandWorker