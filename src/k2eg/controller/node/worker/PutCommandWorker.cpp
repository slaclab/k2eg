#include <k2eg/controller/node/worker/PutCommandWorker.h>

#include <k2eg/service/ServiceResolver.h>

#include <k2eg/common/utility.h>

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
    : logger(ServiceResolver<ILogger>::resolve())
    , epics_service_manager(epics_service_manager) {}

void PutCommandWorker::processCommand(ConstCommandShrdPtr command) {
    if(command->type != CommandType::get) return;
    ConstGetCommandShrdPtr g_ptr = static_pointer_cast<const GetCommand>(command);
    logger->logMessage(STRING_FORMAT("Perform put command for %1%", g_ptr->channel_name), LogLevel::DEBUG);


    return;
}
#pragma endregion PutCommandWorker