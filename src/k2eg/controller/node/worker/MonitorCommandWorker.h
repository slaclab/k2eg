#ifndef k2eg_CONTROLLER_NODE_WORKER_MonitorCommandWORKER_H_
#define k2eg_CONTROLLER_NODE_WORKER_MonitorCommandWORKER_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/metric/IMetricService.h>

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
namespace k2eg::controller::node::worker {

/**
Monitor reply message
*/
struct MonitorCommandReply : public k2eg::controller::node::worker::CommandReply {
    const std::string message;
};
DEFINE_PTR_TYPES(MonitorCommandReply)

/**
Monitor reply message json serialization
*/
inline void
serializeJson(const MonitorCommandReply& reply, common::JsonMessage& json_message) {
  serializeJson(static_cast<CommandReply>(reply), json_message);
  if (!reply.message.empty()) { json_message.getJsonObject()["message"] = reply.message; }
}

/**
Monitor reply message msgpack serialization
*/
inline void
serializeMsgpack(const MonitorCommandReply& reply, common::MsgpackMessage& msgpack_message, std::uint8_t map_size = 0) {
  serializeMsgpack(static_cast<CommandReply>(reply), msgpack_message, map_size + (reply.message.empty() ? 0 : 1));
  msgpack::packer<msgpack::sbuffer> packer(msgpack_message.getBuffer());
  if (!reply.message.empty()) {
    packer.pack("message");
    packer.pack(reply.message);
  }
  // service::epics_impl::epics_serializer_factory.resolve(common::SerializationType::Msgpack)->serialize(*reply.pv_data, msgpack_message);
}

// contains the information for the forward
// of the monitor data to a topic
struct ChannelTopicMonitorInfo {
    std::string dest_topic;
    k2eg::common::SerializationType ser_type;
};
DEFINE_PTR_TYPES(ChannelTopicMonitorInfo);

// map a channel to the topics where it need to be published
DEFINE_MAP_FOR_TYPE(std::string, std::vector<ChannelTopicMonitorInfoUPtr>, ChannelTopicsMap);

//
// ss the command handler for the management of the MonitorCommand
//
class MonitorCommandWorker : public CommandWorker {
    mutable std::shared_mutex channel_map_mtx;
    ChannelTopicsMap channel_topics_map;
    k2eg::service::log::ILoggerShrdPtr logger;
    k2eg::service::pubsub::IPublisherShrdPtr publisher;
    k2eg::service::metric::IEpicsMetric& metric;
    k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager;
    // Handler's liveness token
    k2eg::common::BroadcastToken handler_token;
    
    void manageReply(const std::int8_t error_code, const std::string& error_message, k2eg::controller::command::cmd::ConstMonitorCommandShrdPtr cmd);
    void epicsMonitorEvent(k2eg::service::epics_impl::EpicsServiceManagerHandlerParamterType event_received);
public:
    MonitorCommandWorker(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager);
    virtual ~MonitorCommandWorker() = default;
    void processCommand(k2eg::controller::command::cmd::ConstCommandShrdPtr command);
};

} // namespace k2eg::controller::node::worker

#endif // k2eg_CONTROLLER_NODE_WORKER_MonitorCommandWORKER_H_