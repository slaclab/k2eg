#include <k2eg/common/utility.h>
#include <k2eg/controller/command/cmd/Command.h>
#include <k2eg/controller/command/cmd/PutCommand.h>
#include <k2eg/controller/node/worker/PutCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/epics/EpicsPutOperation.h>

#include <boost/json.hpp>
#include <memory>

using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::metric;
using namespace k2eg::service::epics_impl;

using namespace k2eg::common;

#pragma region PutCommandWorker
PutCommandWorker::PutCommandWorker(EpicsServiceManagerShrdPtr epics_service_manager)
    : processing_pool(std::make_shared<BS::thread_pool>()),
      logger(ServiceResolver<ILogger>::resolve()),
      publisher(ServiceResolver<IPublisher>::resolve()),
      metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric()),
      epics_service_manager(epics_service_manager) {}

PutCommandWorker::~PutCommandWorker() { processing_pool->wait_for_tasks(); }

void
PutCommandWorker::processCommand(ConstCommandShrdPtr command) {
  if (command->type != CommandType::put) return;
  ConstPutCommandShrdPtr p_ptr = static_pointer_cast<const PutCommand>(command);
  logger->logMessage(STRING_FORMAT("Perform put command for %1%", p_ptr->pv_name), LogLevel::DEBUG);
  auto put_op = epics_service_manager->putChannelData(p_ptr->pv_name, "value", p_ptr->value);
  processing_pool->push_task(
      &PutCommandWorker::checkPutCompletion,
      this,
      std::make_shared<PutOpInfo>(p_ptr->pv_name, p_ptr->destination_topic, p_ptr->value, p_ptr->reply_id, p_ptr->serialization, std::move(put_op)));
}

void
PutCommandWorker::checkPutCompletion(PutOpInfoShrdPtr put_info) {
  // check for timeout
  if (put_info->isTimeout()) {
    logger->logMessage(STRING_FORMAT("Timeout put command for %1% and value %2%", put_info->pv_name % put_info->value), LogLevel::ERROR);
    return;
  }
  // give some time of relaxing
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  if (!put_info->op->isDone()) {
    // re-enque the op class
    processing_pool->push_task(&PutCommandWorker::checkPutCompletion, this, put_info);
  } else {
    switch (put_info->op->getState().event) {
      case pvac::PutEvent::Fail: {
        logger->logMessage(STRING_FORMAT("Failed put command for %1% and message %2%", put_info->pv_name % put_info->op->getState().message), LogLevel::ERROR);
        auto serialized_message = serialize(PutCommandReply{-2, put_info->reply_id}, put_info->serialization);
        if (!serialized_message) {
          logger->logMessage("Invalid serialized message", LogLevel::FATAL);
        } else {
          publisher->pushMessage(MakeReplyPushableMessageUPtr(put_info->destination_topic, "put-operation", put_info->pv_name, serialized_message),
                                 {{"k2eg-ser-type", serialization_to_string(put_info->serialization)}});
        }
        break;
      }
      case pvac::PutEvent::Cancel: {
        logger->logMessage(STRING_FORMAT("Cancelled put command for %1% and message %2%", put_info->pv_name % put_info->op->getState().message),
                           LogLevel::ERROR);
        auto serialized_message = serialize(PutCommandReply{-1, put_info->reply_id}, put_info->serialization);
        if (!serialized_message) {
          logger->logMessage("Invalid serialized message", LogLevel::FATAL);
        } else {
          publisher->pushMessage(MakeReplyPushableMessageUPtr(put_info->destination_topic, "put-operation", put_info->pv_name, serialized_message),
                                 {{"k2eg-ser-type", serialization_to_string(put_info->serialization)}});
        }
        break;
      }
      case pvac::PutEvent::Success: {
        metric.incrementCounter(IEpicsMetricCounterType::Put);
        logger->logMessage(STRING_FORMAT("Success put command for %1%", put_info->pv_name), LogLevel::INFO);
        auto serialized_message = serialize(PutCommandReply{0, put_info->reply_id}, put_info->serialization);
        if (!serialized_message) {
          logger->logMessage("Invalid serialized message", LogLevel::FATAL);
        } else {
          publisher->pushMessage(MakeReplyPushableMessageUPtr(put_info->destination_topic, "put-operation", put_info->pv_name, serialized_message),
                                 {{"k2eg-ser-type", serialization_to_string(put_info->serialization)}});
        }
        break;
      }
    }
  }
}

k2eg::common::ConstSerializedMessageShrdPtr
PutCommandWorker::getReply(PutOpInfoShrdPtr put_info) {
  return ConstSerializedMessageShrdPtr();
}
#pragma endregion PutCommandWorker