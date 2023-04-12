#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/MonitorCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>

#include <cassert>
#include <functional>

using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::log;

using namespace k2eg::service::epics_impl;

using namespace k2eg::service::pubsub;

#pragma region MonitorMessage
MonitorMessage::MonitorMessage(const std::string& queue, ConstMonitorEventShrdPtr monitor_event, ConstSerializedMessageShrdPtr message)
    : request_type("monitor")
    , monitor_event(monitor_event)
    , queue(queue)
    , message(message) {}
char* MonitorMessage::getBufferPtr() { return const_cast<char*>(message->data()); }
const size_t MonitorMessage::getBufferSize() { return message->size(); }
const std::string& MonitorMessage::getQueue() { return queue; }
const std::string& MonitorMessage::getDistributionKey() { return monitor_event->channel_data.channel_name; }
const std::string& MonitorMessage::getReqType() { return request_type; }
#pragma endregion MonitorMessage

#pragma region MonitorCommandWorker
MonitorCommandWorker::MonitorCommandWorker(EpicsServiceManagerShrdPtr epics_service_manager)
    : logger(ServiceResolver<ILogger>::resolve())
    , publisher(ServiceResolver<IPublisher>::resolve())
    , epics_service_manager(epics_service_manager) {
    handler_token = epics_service_manager->addHandler(std::bind(&MonitorCommandWorker::epicsMonitorEvent, this, std::placeholders::_1));
}

void MonitorCommandWorker::processCommand(ConstCommandShrdPtr command) {
    if (command->type != CommandType::monitor) return;
    bool activate = false;
    auto a_ptr = static_pointer_cast<const MonitorCommand>(command);
    // lock the vector for write
    {
        logger->logMessage(STRING_FORMAT("%1% monitor on '%2%' for topic '%3%'", (a_ptr->activate ? "Activate" : "Deactivate") % a_ptr->channel_name % a_ptr->destination_topic));

        auto& vec_ref = channel_topics_map[a_ptr->channel_name];
        if (a_ptr->activate) {
            // add topic to channel
            // check if the topic is already present[fault tollerant check]
            if (std::find_if(std::begin(vec_ref), std::end(vec_ref), [&a_ptr](auto& info_topic) { return info_topic->dest_topic.compare(a_ptr->destination_topic) == 0; }) == std::end(vec_ref)) {
                std::unique_lock lock(channel_map_mtx);
                channel_topics_map[a_ptr->channel_name].push_back(MakeChannelTopicMonitorInfoShrdPtr(ChannelTopicMonitorInfo{.dest_topic = a_ptr->destination_topic, .ser_type = a_ptr->serialization})

                );
            } else {
                logger->logMessage(STRING_FORMAT("Monitor for '%1%' for topic '%2%' already activated", a_ptr->channel_name % a_ptr->destination_topic));
            }
        } else {
            // remove topic to channel
            auto itr = std::find_if(std::begin(vec_ref), std::end(vec_ref), [&a_ptr](auto& info_topic) { return info_topic->dest_topic.compare(a_ptr->destination_topic) == 0; });
            if (itr != std::end(vec_ref)) {
                std::unique_lock lock(channel_map_mtx);
                vec_ref.erase(itr);
            } else {
                logger->logMessage(STRING_FORMAT("No active monitor on '%1%' for topic '%2%'", a_ptr->channel_name % a_ptr->destination_topic));
            }
        }
        activate = vec_ref.size();
    }
    // if the vec_ref has size > 0 mean that someone is still needed the channel data in monitor way
    epics_service_manager->monitorChannel(a_ptr->channel_name, activate, a_ptr->protocol);
}

void MonitorCommandWorker::epicsMonitorEvent(const MonitorEventVecShrdPtr& event_data) {
#ifdef __DEBUG__
    logger->logMessage(STRING_FORMAT("Received epics monitor %1% events", event_data->size()), LogLevel::TRACE);
#endif
    std::shared_lock slock(channel_map_mtx);
    // cache the varius serilized message for each serializaiton type
    std::map<MessageSerType, ConstSerializedMessageShrdPtr> local_serialization_cache;
    for (auto& event: *event_data) {
        // publisher
        if (event->type != MonitorType::Data) continue;
        for (auto& info_topic: channel_topics_map[event->channel_data.channel_name]) {
            logger->logMessage(STRING_FORMAT("Publish channel %1% on topic %2%", event->channel_data.channel_name % info_topic->dest_topic), LogLevel::TRACE);
            if (!local_serialization_cache.contains(info_topic->ser_type)) {
                // cache new serialized message
                local_serialization_cache[info_topic->ser_type] = serialize(event->channel_data, static_cast<SerializationType>(info_topic->ser_type));
            }

            publisher->pushMessage(
                MakeMonitorMessageUPtr(
                    info_topic->dest_topic, event, 
                    local_serialization_cache[info_topic->ser_type]
                    )
           );
        }
    }
    publisher->flush(100);
}
#pragma endregion MonitorMessage