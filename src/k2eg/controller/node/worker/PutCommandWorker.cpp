#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/PutCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>
#include <memory>

#include "k2eg/controller/command/cmd/Command.h"
#include "k2eg/controller/command/cmd/PutCommand.h"
#include "k2eg/service/epics/EpicsPutOperation.h"

using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::epics_impl;

using namespace k2eg::common;

#pragma region PutCommandWorker
PutCommandWorker::PutCommandWorker(EpicsServiceManagerShrdPtr epics_service_manager)
    : processing_pool(std::make_shared<BS::thread_pool>()), logger(ServiceResolver<ILogger>::resolve()), epics_service_manager(epics_service_manager) {}

void
PutCommandWorker::processCommand(ConstCommandShrdPtr command) {
  if (command->type != CommandType::get) return;
  ConstPutCommandShrdPtr p_ptr = static_pointer_cast<const PutCommand>(command);
  logger->logMessage(STRING_FORMAT("Perform put command for %1%", p_ptr->channel_name), LogLevel::DEBUG);
  auto put_op = epics_service_manager->putChannelData(p_ptr->channel_name, "value", p_ptr->value);
  processing_pool->push_task(
      &PutCommandWorker::checkPutCompletion, 
      this,
      std::make_shared<PutOpInfo>(p_ptr->channel_name, p_ptr->value, std::move(put_op))
      );
}

void
PutCommandWorker::checkPutCompletion(PutOpInfoShrdPtr put_info) {
  if (!put_info->op->isDone()) {
    // re-enque the op class
    processing_pool->push_task(&PutCommandWorker::checkPutCompletion, this, put_info);
  } else {
    switch (put_info->op->getState().event) {
      case pvac::PutEvent::Fail:
        logger->logMessage(STRING_FORMAT("Failed put command for %1% and value %2%", put_info->channel_name % put_info->value), LogLevel::INFO);
        break;
      case pvac::PutEvent::Cancel:
        logger->logMessage(STRING_FORMAT("Cancelled put command for %1% and value %2%", put_info->channel_name % put_info->value), LogLevel::INFO);
        break;
      case pvac::PutEvent::Success:
        logger->logMessage(STRING_FORMAT("Success put command for %1% and value %2%", put_info->channel_name % put_info->value), LogLevel::INFO);
        break;
    }
  }
}
#pragma endregion PutCommandWorker