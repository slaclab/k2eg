#ifndef k2eg_CONTROLLER_NODE_WORKER_MonitorCommandWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_MonitorCommandWORKER_H_

#include <k2eg/common/types.h>

#include <k2eg/controller/command/cmd/MonitorCommand.h>
#include <k2eg/service/data/repository/ChannelRepository.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>

#include <k2eg/controller/node/configuration/NodeConfiguration.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/controller/node/worker/monitor/MonitorChecker.h>

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace k2eg::controller::node::worker::monitor {

/**
 * @brief Configuration for monitor command handling.
 *
 * Controls the periodic monitor health checking and checker behavior.
 */
struct MonitorCommandConfiguration
{
    /** @brief Cron expression used to schedule monitor checks. */
    std::string                 cron_scheduler_monitor_check;
    /** @brief Parameters for the monitor checker component. */
    MonitorCheckerConfiguration monitor_checker_configuration;
};
DEFINE_PTR_TYPES(MonitorCommandConfiguration)

/**
 * @brief Reply message for monitor commands.
 */
struct MonitorCommandReply : public k2eg::controller::node::worker::CommandReply
{
    const std::string message;
};
DEFINE_PTR_TYPES(MonitorCommandReply)

/**
 * @brief Serialize a monitor reply to JSON.
 * @param reply source reply to serialize
 * @param json_message destination JSON message wrapper
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
 * @brief Serialize a monitor reply to Msgpack.
 * @param reply source reply to serialize
 * @param msgpack_message destination Msgpack wrapper
 * @param map_size extra map entries contributed by callers (default 0)
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

/**
 * @brief State for forwarding monitor updates of one channel.
 *
 * Tracks whether forwarding is active and which monitor commands are
 * currently attached to the channel.
 */
struct ChannelTopicMonitorInfo
{
    /** @brief Whether the forwarding for this channel is active. */
    bool                                                             active = false;
    /** @brief Attached monitor command descriptors for this channel. */
    std::vector<k2eg::service::data::repository::ChannelMonitorType> cmd_vec;
};

DEFINE_PTR_TYPES(ChannelTopicMonitorInfo);

/** @brief Map a channel name to its topic-forwarding state. */
DEFINE_MAP_FOR_TYPE(std::string, ChannelTopicMonitorInfo, ChannelTopicsMap);

//
// ss the command handler for the management of the MonitorCommand
//
/**
 * @brief Command handler for EPICS monitor lifecycle.
 *
 * Manages start/stop of channel monitors, publishes updates, and runs
 * periodic checks to ensure monitors remain healthy.
 */
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

    /**
     * @brief Compute the target queue/topic for a PV name.
     * @param pv_name EPICS channel name
     * @return destination queue/topic identifier
     */
    const std::string get_queue_for_pv(const std::string& pv_name);
    /**
     * @brief Handle a command operating on a single monitor.
     * @param command monitor command (start/stop/snapshot)
     */
    void              manage_single_monitor(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
    /**
     * @brief Handle a command operating on multiple/patterned monitors.
     * @param command monitor command affecting multiple channels
     */
    void              manage_multiple_monitor(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
    /**
     * @brief Build and publish a reply for a processed command.
     * @param error_code error code (0 for success)
     * @param error_message optional error details
     * @param cmd original command reference
     */
    void              manageReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstCommandShrdPtr cmd);
    /**
     * @brief Handle an EPICS monitor event emitted by the service manager.
     */
    void              epicsMonitorEvent(k2eg::service::epics_impl::EpicsServiceManagerHandlerParamterType event_received);
    /**
     * @brief Process events coming from the monitor checker component.
     */
    void              handleMonitorCheckEvents(MonitorHandlerData checker_event_data);
    /**
     * @brief Scheduler callback to restart monitors when required.
     */
    void              handleRestartMonitorTask(k2eg::service::scheduler::TaskProperties& task_properties);
    /**
     * @brief Scheduler callback invoked periodically for housekeeping.
     */
    void              handlePeriodicTask(k2eg::service::scheduler::TaskProperties& task_properties);
    /**
     * @brief Callback invoked after a publish event completes or fails.
     */
    void              publishEvtCB(k2eg::service::pubsub::EventType type, k2eg::service::pubsub::PublishMessage* const msg, const std::string& error_message);

public:
    /**
     * @brief Construct a monitor worker.
     * @param monitor_command_configuration configuration for checks and behavior
     * @param epics_service_manager EPICS services accessor/manager
     * @param node_configuration_db node configuration repository
     */
    MonitorCommandWorker(const MonitorCommandConfiguration& monitor_command_configuration, k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager, k2eg::controller::node::configuration::NodeConfigurationShrdPtr node_configuration_db);
    /** @brief Virtual destructor. */
    virtual ~MonitorCommandWorker();
    /**
     * @brief Execute the periodic monitor maintenance task.
     *
     * Schedules or runs housekeeping like restarts based on configuration.
     */
    void executePeriodicTask();
    /**
     * @brief Process an incoming command (monitor start/stop, multi/pattern).
     * @param thread_pool worker pool used for async work
     * @param command incoming command instance
     */
    void processCommand(std::shared_ptr<BS::light_thread_pool> thread_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command);
    /**
     * @brief Report whether the worker is ready to accept commands.
     * @return true when internal services are initialized
     */
    bool isReady();
};

} // namespace k2eg::controller::node::worker::monitor

#endif // k2eg_CONTROLLER_NODE_WORKER_MonitorCommandWORKER_H_
