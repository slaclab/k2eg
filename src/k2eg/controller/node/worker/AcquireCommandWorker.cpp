#include <k2eg/common/utility.h>
#include <k2eg/controller/node/worker/AcquireCommandWorker.h>
#include <k2eg/service/ServiceResolver.h>

#include <cassert>
#include <functional>

using namespace k2eg::controller::node::worker;
using namespace k2eg::controller::command;

using namespace k2eg::service;
using namespace k2eg::service::log;

using namespace k2eg::service::epics_impl;

using namespace k2eg::service::pubsub;

#pragma region MonitorMessage
MonitorMessage::MonitorMessage(const std::string& queue, ConstMonitorEventShrdPtr monitor_event)
    : request_type("monitor")
    , monitor_event(monitor_event)
    , queue(queue)
    , message(to_json(this->monitor_event->channel_data)) {}

char* MonitorMessage::getBufferPtr() { return const_cast<char*>(message.c_str()); }
size_t MonitorMessage::getBufferSize() { return message.size(); }
const std::string& MonitorMessage::getQueue() { return queue; }
const std::string& MonitorMessage::getDistributionKey() { return monitor_event->channel_data.channel_name; }
const std::string& MonitorMessage::getReqType() { return request_type; }
#pragma endregion MonitorMessage

#pragma region AcquireCommandWorker
AcquireCommandWorker::AcquireCommandWorker(std::shared_ptr<BS::thread_pool> shared_worker_processing,
                                           EpicsServiceManagerShrdPtr epics_service_manager)
    : CommandWorker(shared_worker_processing)
    , logger(ServiceResolver<ILogger>::resolve())
    , publisher(ServiceResolver<IPublisher>::resolve())
    , epics_service_manager(epics_service_manager) {
    handler_token = epics_service_manager->addHandler(
        std::bind(&AcquireCommandWorker::epicsMonitorEvent, this, std::placeholders::_1));
}

AcquireCommandWorker::~AcquireCommandWorker() {}

bool AcquireCommandWorker::submitCommand(CommandConstShrdPtr command) {
    if (command->type != CommandType::monitor) return false;
    shared_worker_processing->push_task(&AcquireCommandWorker::acquireManagement, this, command);
    return true;
}

void AcquireCommandWorker::acquireManagement(k2eg::controller::command::CommandConstShrdPtr command) {
    bool activate = false;
    ConstAquireCommandShrdPtr a_ptr = static_pointer_cast<const AquireCommand>(command);
    // lock the vector for write
    {
        logger->logMessage(STRING_FORMAT("%1% monitor on '%2%' for topic '%3%'",
                                         (a_ptr->activate ? "Activate" : "Deactivate") % a_ptr->channel_name
                                             % a_ptr->destination_topic));

        auto& vec_ref = channel_topics_map[a_ptr->channel_name];
        if (a_ptr->activate) {
            // add topic to channel
            // check if the topic is already present[fault tollerant check]
            if (std::find(std::begin(vec_ref), std::end(vec_ref), a_ptr->destination_topic) == std::end(vec_ref)) {
                std::unique_lock lock(channel_map_mtx);
                channel_topics_map[a_ptr->channel_name].push_back(a_ptr->destination_topic);
            } else {
                logger->logMessage(STRING_FORMAT("Monitor for '%1%' for topic '%2%' already activated",
                                                 a_ptr->channel_name % a_ptr->destination_topic));
            }
        } else {
            // remove topic to channel
            auto itr = std::find(std::begin(vec_ref), std::end(vec_ref), a_ptr->destination_topic);
            if (itr != std::end(vec_ref)) {
                std::unique_lock lock(channel_map_mtx);
                vec_ref.erase(itr);
            } else {
                logger->logMessage(STRING_FORMAT("No active monitor on '%1%' for topic '%2%'",
                                                 a_ptr->channel_name % a_ptr->destination_topic));
            }
        }
        activate = vec_ref.size();
    }
    // if the vec_ref has size > 0 mean that someone is still needed the channel data in monitor way
    epics_service_manager->monitorChannel(a_ptr->channel_name, activate, a_ptr->protocol);
}

void AcquireCommandWorker::epicsMonitorEvent(const MonitorEventVecShrdPtr& event_data) {
#ifdef __DEBUG__
    logger->logMessage(STRING_FORMAT("Received epics monitor %1% events", event_data->size()), LogLevel::TRACE);
#endif
    std::shared_lock slock(channel_map_mtx);
    for (auto& e: *event_data) {
        // publisher
        if (e->type != MonitorType::Data) continue;
        for (auto& topic: channel_topics_map[e->channel_data.channel_name]) {
            logger->logMessage(STRING_FORMAT("Publish channel %1% on topic %2%", e->channel_data.channel_name % topic),
                               LogLevel::TRACE);
            publisher->pushMessage(std::make_unique<MonitorMessage>(topic, e));
        }
    }
    publisher->flush(100);
}
#pragma endregion MonitorMessage