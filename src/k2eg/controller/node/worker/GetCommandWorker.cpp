#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/GetCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>

#include <chrono>
#include <thread>

#include "client.h"
#include "k2eg/service/metric/IMetricService.h"

using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::log;

using namespace k2eg::service::epics_impl;

using namespace k2eg::service::pubsub;

using namespace k2eg::service::metric;

#pragma region GetMessage
GetMessage::GetMessage(const std::string& destination_topic, ConstChannelDataUPtr channel_data, ConstSerializedMessageShrdPtr message)
    : request_type("get"), destination_topic(destination_topic), channel_data(std::move(channel_data)), message(message) {}

char*
GetMessage::getBufferPtr() {
  return const_cast<char*>(message->data());
}
const size_t
GetMessage::getBufferSize() {
  return message->size();
}
const std::string&
GetMessage::getQueue() {
  return destination_topic;
}
const std::string&
GetMessage::getDistributionKey() {
  return channel_data->pv_name;
}
const std::string&
GetMessage::getReqType() {
  return request_type;
}
#pragma endregion GetMessage

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
  // while(!channel_data->isDone()){std::this_thread::sleep_for(std::chrono::milliseconds(100));}
  processing_pool->push_task(&GetCommandWorker::checkGetCompletion,
                             this,
                             std::make_shared<GetOpInfo>(g_ptr->pv_name, g_ptr->destination_topic, g_ptr->serialization, std::move(channel_data)));
}

void
GetCommandWorker::checkGetCompletion(GetOpInfoShrdPtr get_info) {
  // check for timeout
  if (get_info->isTimeout()) {
    logger->logMessage(STRING_FORMAT("Timeout get command for %1%", get_info->pv_name), LogLevel::ERROR);
    return;
  }
  // give some time of relaxing
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  if (!get_info->op->isDone()) {
    // re-enque the op class
    processing_pool->push_task(&GetCommandWorker::checkGetCompletion, this, get_info);
  } else {
    switch (get_info->op->getState().event) {
      case pvac::GetEvent::Fail:
        logger->logMessage(STRING_FORMAT("Failed get command for %1% with message %2%", get_info->pv_name % get_info->op->getState().message), LogLevel::ERROR);
        break;
      case pvac::GetEvent::Cancel:
        logger->logMessage(STRING_FORMAT("Cancelled get command for %1% with message %2%", get_info->pv_name % get_info->op->getState().message),
                           LogLevel::ERROR);
        break;
      case pvac::GetEvent::Success:
        // update metric
        metric.incrementCounter(IEpicsMetricCounterType::Get);
        logger->logMessage(STRING_FORMAT("Success get command for %1%", get_info->pv_name), LogLevel::INFO);
        auto channel_data = get_info->op->getChannelData();
        if (!channel_data) {
          logger->logMessage(STRING_FORMAT("No data received for %1%", get_info->pv_name), LogLevel::ERROR);
          break;
        }
        auto serialized_message = serialize(*channel_data, static_cast<SerializationType>(get_info->serialization));
        if (!serialized_message) {
          logger->logMessage("Invalid serilized message", LogLevel::ERROR);
          break;
        }
        publisher->pushMessage(
          std::make_unique<GetMessage>(get_info->destination_topic, std::move(channel_data), serialized_message),
          {{"k2eg-ser-type", serialization_to_string(get_info->serialization)}});
        break;
    }
  }
}
#pragma endregion GetCommandWorker