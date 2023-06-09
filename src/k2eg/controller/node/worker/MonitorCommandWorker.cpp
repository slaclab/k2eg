#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/MonitorCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>

#include <cassert>
#include <functional>
#include "k2eg/controller/node/worker/CommandWorker.h"
#include "k2eg/service/epics/EpicsChannel.h"
#include "k2eg/service/metric/IMetricService.h"
#include "k2eg/service/pubsub/IPublisher.h"

using namespace k2eg::common;

using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::command;
using namespace k2eg::controller::command::cmd;

using namespace k2eg::service;
using namespace k2eg::service::log;

using namespace k2eg::service::epics_impl;

using namespace k2eg::service::pubsub;

using namespace k2eg::service::metric;

#pragma region MonitorCommandWorker
MonitorCommandWorker::MonitorCommandWorker(EpicsServiceManagerShrdPtr epics_service_manager)
    : logger(ServiceResolver<ILogger>::resolve())
    , publisher(ServiceResolver<IPublisher>::resolve())
    , metric(ServiceResolver<IMetricService>::resolve()->getEpicsMetric())
    , epics_service_manager(epics_service_manager) {
    handler_token = epics_service_manager->addHandler(std::bind(&MonitorCommandWorker::epicsMonitorEvent, this, std::placeholders::_1));
}

void MonitorCommandWorker::processCommand(ConstCommandShrdPtr command) {
    if (command->type != CommandType::monitor) return;
    bool activate = false;
    auto a_ptr = static_pointer_cast<const MonitorCommand>(command);
    // lock the map and contained vector for write
    {
        std::unique_lock lock(channel_map_mtx);
        logger->logMessage(STRING_FORMAT("%1% monitor on '%2%' for topic '%3%'", (a_ptr->activate ? "Activate" : "Deactivate") % a_ptr->pv_name % a_ptr->destination_topic));

        auto& vec_ref = channel_topics_map[a_ptr->pv_name];
        if (a_ptr->activate) {
            // add topic to channel
            // check if the topic is already present[fault tollerant check]
            if (std::find_if(std::begin(vec_ref), std::end(vec_ref), [&a_ptr](auto& info_topic) { return info_topic->dest_topic.compare(a_ptr->destination_topic) == 0; }) == std::end(vec_ref)) {
                    channel_topics_map[a_ptr->pv_name].push_back(MakeChannelTopicMonitorInfoUPtr(ChannelTopicMonitorInfo{.dest_topic = a_ptr->destination_topic, .ser_type = a_ptr->serialization})
                );
            } else {
                logger->logMessage(STRING_FORMAT("Monitor for '%1%' for topic '%2%' already activated", a_ptr->pv_name % a_ptr->destination_topic));
            }
        } else {
            // remove topic to channel
            auto itr = std::find_if(std::begin(vec_ref), std::end(vec_ref), [&a_ptr](auto& info_topic) { return info_topic->dest_topic.compare(a_ptr->destination_topic) == 0; });
            if (itr != std::end(vec_ref)) {
                vec_ref.erase(itr);
            } else {
                logger->logMessage(STRING_FORMAT("No active monitor on '%1%' for topic '%2%'", a_ptr->pv_name % a_ptr->destination_topic));
            }
        }
        activate = vec_ref.size();
    }
    // if the vec_ref has size > 0 mean that someone is still needed the channel data in monitor way
    epics_service_manager->monitorChannel(a_ptr->pv_name, activate, a_ptr->protocol);
}

void MonitorCommandWorker::epicsMonitorEvent(EpicsServiceManagerHandlerParamterType event_received) {
#ifdef __DEBUG__
    logger->logMessage(STRING_FORMAT("Received epics monitor %1% events data", event_received->event_data->size()), LogLevel::TRACE);
#endif
    //----------update metric--------
    metric.incrementCounter(IEpicsMetricCounterType::MonitorData, event_received->event_data->size());
    metric.incrementCounter(IEpicsMetricCounterType::MonitorCancel, event_received->event_cancel->size());
    metric.incrementCounter(IEpicsMetricCounterType::MonitorDisconnect, event_received->event_disconnect->size());
    metric.incrementCounter(IEpicsMetricCounterType::MonitorFail, event_received->event_fail->size());

    std::shared_lock slock(channel_map_mtx);
    // cache the various serilized message for each serializaiton type
    std::map<SerializationType, ConstSerializedMessageShrdPtr> local_serialization_cache;
    for (auto& event: *event_received->event_data) {
        // publisher
        for (auto& info_topic: channel_topics_map[event->channel_data.pv_name]) {
            logger->logMessage(STRING_FORMAT("Publish channel %1% on topic %2%", event->channel_data.pv_name % info_topic->dest_topic), LogLevel::TRACE);
            if (!local_serialization_cache.contains(info_topic->ser_type)) {
                // cache new serialized message
                local_serialization_cache[info_topic->ser_type] = serialize(event->channel_data, static_cast<SerializationType>(info_topic->ser_type));
            }
            publisher->pushMessage(
                MakeReplyPushableMessageUPtr(
                    info_topic->dest_topic,
                    "monitor-message",
                    event->channel_data.pv_name,
                    local_serialization_cache[info_topic->ser_type]
                    ),
                    {// add header
                        {
                        "k2eg-ser-type",
                        serialization_to_string(info_topic->ser_type)
                        }
                    }
           );
        }
    }
    publisher->flush(100);
}
#pragma endregion MonitorMessage