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
  auto put_op = epics_service_manager->putChannelData(p_ptr->pv_name, p_ptr->value);
  if (!put_op) {
    // fire error
    manageReply(-1, "PV name malformed", p_ptr);
  } else {
    processing_pool->push_task(
        &PutCommandWorker::checkPutCompletion,
        this,
        std::make_shared<PutOpInfo>(p_ptr, std::move(put_op)));
  }
}

void
PutCommandWorker::manageReply(const std::int8_t error_code, const std::string& error_message, ConstPutCommandShrdPtr cmd) {
  logger->logMessage(STRING_FORMAT("%1% [pv:%2% value:%3%]", error_message % cmd->pv_name % cmd->value), LogLevel::ERROR);
  if (cmd->reply_topic.empty() || cmd->reply_id.empty()) {
    return;
  } else {
    auto serialized_message = serialize(PutCommandReply{error_code, cmd->reply_id, error_message}, cmd->serialization);
    if (!serialized_message) {
      logger->logMessage("Invalid serialized message", LogLevel::FATAL);
    } else {
      publisher->pushMessage(MakeReplyPushableMessageUPtr(cmd->reply_topic, "put-operation", cmd->pv_name, serialized_message),
                             {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
    }
  }
}

void
PutCommandWorker::checkPutCompletion(PutOpInfoShrdPtr put_info) {
  // check for timeout
  if (put_info->isTimeout()) {
    manageReply(-3, "Timeout operation", put_info->cmd);
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
        manageReply(-2, put_info->op->getState().message, put_info->cmd);
        break;
      }
      case pvac::PutEvent::Cancel: {
        manageReply(-2, "Put operation hs been cancelled", put_info->cmd);
        break;
      }
      case pvac::PutEvent::Success: {
        metric.incrementCounter(IEpicsMetricCounterType::Put);
        manageReply(0, "Successfull operation", put_info->cmd);
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