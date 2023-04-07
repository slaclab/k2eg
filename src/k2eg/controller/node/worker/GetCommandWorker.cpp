#include <k2eg/controller/node/worker/GetCommandWorker.h>

#include <k2eg/service/ServiceResolver.h>

#include <k2eg/common/utility.h>

using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::log;

using namespace k2eg::service::epics_impl;

using namespace k2eg::service::pubsub;

#pragma region GetMessage
GetMessage::GetMessage(const std::string& destination_topic, ConstChannelDataUPtr channel_data)
    : request_type("get")
    , destination_topic(destination_topic)
    , channel_data(std::move(channel_data))
    , message(serialize(*this->channel_data, SerializationType::JSON)) {}

char* GetMessage::getBufferPtr() { return const_cast<char*>(message->data()); }
const size_t GetMessage::getBufferSize() { return message->size(); }
const std::string& GetMessage::getQueue() { return destination_topic; }
const std::string& GetMessage::getDistributionKey() { return channel_data->channel_name; }
const std::string& GetMessage::getReqType() { return request_type; }
#pragma endregion GetMessage

#pragma region GetCommandWorker
GetCommandWorker::GetCommandWorker(EpicsServiceManagerShrdPtr epics_service_manager)
    : logger(ServiceResolver<ILogger>::resolve())
    , publisher(ServiceResolver<IPublisher>::resolve())
    , epics_service_manager(epics_service_manager) {}

void GetCommandWorker::processCommand(ConstCommandShrdPtr command) {
    if(command->type != CommandType::get) return;
    ConstGetCommandShrdPtr g_ptr = static_pointer_cast<const GetCommand>(command);
    auto channel_data = epics_service_manager->getChannelData(g_ptr->channel_name);
    if(channel_data) {
        publisher->pushMessage(std::make_unique<GetMessage>(g_ptr->destination_topic, std::move(channel_data)));
    } else {
        // data not received => timeout
        logger->logMessage(STRING_FORMAT("Message not recevide for %1%", g_ptr->channel_name), LogLevel::ERROR);
    }
    return;
}
#pragma endregion GetCommandWorker