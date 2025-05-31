#ifndef k2eg_CONTROLLER_NODE_WORKER_MonitorCommandWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_MonitorCommandWORKER_H_

#include <k2eg/common/types.h>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/controller/command/cmd/MonitorCommand.h>
#include <k2eg/service/data/repository/ChannelRepository.h>

#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>
#include <k2eg/controller/node/configuration/NodeConfiguration.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <shared_mutex>

namespace k2eg::controller::node::worker::monitor {

struct MonitorCommandConfiguration
{
    // the cron stirng for schedule the monitor
    std::string                 cron_scheduler_monitor_check;
    MonitorCheckerConfiguration monitor_checker_configuration;
};
DEFINE_PTR_TYPES(MonitorCommandConfiguration)

/**
Monitor reply message
*/
struct MonitorCommandReply : public k2eg::controller::node::worker::CommandReply
{
    const std::string message;
};
DEFINE_PTR_TYPES(MonitorCommandReply)

/**
Monitor reply message json serialization
*/
inline void serializeJson(const MonitorCommandReply& reply, common::JsonMessage& json_message)
{
    serializeJson(static_cast<CommandReply>(reply), json_message);
    if (!reply.message.empty())
    {
        json_message.getJsonObject()["message"] = reply.message;
    }
}

/**
Monitor reply message msgpack serialization
*/
inline void serializeMsgpack(const MonitorCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0)
{
    serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
    msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
    if (!reply.message.empty())
    {
        packer.pack("message");
        packer.pack(reply.message);
    }
    // service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*reply.pv_data,
    // msgpack_message);
}

// contains the information for the forward
// of the monitor data to a topic
struct ChannelTopicMonitorInfo
{
    bool                                                             active = false;
    std::vector<k2eg::service::data::repository::ChannelMonitorType> cmd_vec;
};

DEFINE_PTR_TYPES(ChannelTopicMonitorInfo);

// map a channel to the topics where it need to be published
DEFINE_MAP_FOR_TYPE(std::string, ChannelTopicMonitorInfo, ChannelTopicsMap);

//
// ss the command handler for the management of the MonitorCommand
//
class MonitorCommandWorker : public CommandWorker
{
    const MonitorCommandConfiguration                               monitor_command_configuration;
    mutable std::shared_mutex                                       channel_map_mtx;
    mutable std::mutex                                              periodic_task_mutex;
    ChannelTopicsMap                                                channel_topics_map;
    k2eg::controller::node::configuration::NodeConfigurationShrdPtr node_configuration_db;
    k2eg::service::log::ILoggerShrdPtr                              logger;
    k2eg::service::pubsub::IPublisherShrdPtr                        publisher;
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr           epics_service_manager;
    MonitorCheckerShrdPtr                                           monitor_checker_shrd_ptr;
    // Handler's liveness token
    k2eg::common::BroadcastToken epics_handler_token;
    k2eg::common::BroadcastToken monitor_checker_token;
    std::atomic_bool             starting_up;

    const std::string get_queue_for_pv(const std::string& pv_name);
    void              manage_single_monitor(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
    void              manage_multiple_monitor(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
    void manageReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstCommandShrdPtr cmd);
    void epicsMonitorEvent(k2eg::service::epics_impl::EpicsServiceManagerHandlerParamterType event_received);
    void handleMonitorCheckEvents(MonitorHandlerData checker_event_data);
    void handleRestartMonitorTask(k2eg::service::scheduler::TaskProperties& task_properties);
    void handlePeriodicTask(k2eg::service::scheduler::TaskProperties& task_properties);
    void publishEvtCB(k2eg::service::pubsub::EventType type, k2eg::service::pubsub::PublishMessage* const msg, const std::string& error_message);

public:
    MonitorCommandWorker(const MonitorCommandConfiguration& monitor_command_configuration, k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager, k2eg::controller::node::configuration::NodeConfigurationShrdPtr node_configuration_db);
    virtual ~MonitorCommandWorker();
    void executePeriodicTask();
    void processCommand(std::shared_ptr<BS::light_thread_pool> thread_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command);
    bool isReady();
};

} // namespace k2eg::controller::node::worker::monitor

#endif // k2eg_CONTROLLER_NODE_WORKER_MonitorCommandWORKER_H_