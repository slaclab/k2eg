#ifndef NODEUTILITIES_H_
#define NODEUTILITIES_H_

#include "boost/json/object.hpp"
#include "k2eg/common/BaseSerialization.h"
#include "k2eg/common/types.h"
#include "k2eg/service/log/ILogger.h"
#include <cstddef>
#include <gtest/gtest.h>
#include <k2eg/k2eg.h>

#include <k2eg/common/ProgramOptions.h>
#include <k2eg/common/uuid.h>

#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/command/cmd/SnapshotCommand.h>
#include <k2eg/controller/node/NodeController.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaPublisher.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaSubscriber.h>
#include <k2eg/service/storage/StorageServiceFactory.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <k2eg/common/MsgpackSerialization.h>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Command message wrapper for publishing commands.
 * @tparam T Command pointer type used by controller.
 */
template <typename T>
class CMDMessage : public k2eg::service::pubsub::PublishMessage
{
    const std::string request_type;
    const std::string distribution_key;
    const std::string queue;
    std::string       json_nmessage;
    //! the message data
    T cmd;

public:
    /**
     * @brief Construct a command message.
     * @param queue Target topic/queue name.
     * @param cmd Command pointer to serialize.
     */
    CMDMessage(const std::string& queue, T cmd)
        : request_type("test"), distribution_key(k2eg::common::UUID::generateUUIDLite()), queue(queue), cmd(cmd)
    {
        json_nmessage = k2eg::controller::command::to_json_string_cmd_ptr(cmd);
    }

    /**
     * @brief Destroy the message.
     */
    virtual ~CMDMessage() {}

    /**
     * @brief Get mutable buffer pointer for publisher.
     * @return Pointer to internal JSON buffer.
     */
    char* getBufferPtr()
    {
        return const_cast<char*>(json_nmessage.c_str());
    }

    /**
     * @brief Get payload size in bytes.
     * @return Size of the JSON buffer.
     */
    const size_t getBufferSize()
    {
        return json_nmessage.size();
    }

    /**
     * @brief Get target queue name.
     * @return Queue/topic string.
     */
    const std::string& getQueue()
    {
        return queue;
    }

    /**
     * @brief Get distribution key used for partitioning.
     * @return Stable per-message key.
     */
    const std::string& getDistributionKey()
    {
        return distribution_key;
    }

    /**
     * @brief Get request type label.
     * @return Constant request type string.
     */
    const std::string& getReqType()
    {
        return request_type;
    }
};

/**
 * @brief Create a Kafka topic using librdkafka admin APIs and wait until brokers report it.
 *
 * Tests can call this helper before booting a node to ensure required topics exist.
 * The function tolerates pre-existing topics and fails the current test on other errors.
 */
inline void ensureKafkaTopicExists(
    const std::string&                                      bootstrap_servers,
    const std::string&                                      topic_name,
    int                                                     partitions = 1,
    int                                                     replication_factor = 1,
    const std::unordered_map<std::string, std::string>&     topic_config = {},
    std::chrono::milliseconds                               admin_timeout = std::chrono::seconds(30),
    std::chrono::milliseconds                               readiness_timeout = std::chrono::seconds(30),
    std::chrono::milliseconds                               poll_interval = std::chrono::milliseconds(250))
{
    const int         errstr_cnt = 512;
    char              errstr[errstr_cnt];
    std::string       conf_errstr;

    // Create an admin-capable producer handle.
    std::unique_ptr<RdKafka::Conf> admin_conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!admin_conf)
    {
        ADD_FAILURE() << "Failed to allocate librdkafka admin configuration";
        return;
    }
    if (admin_conf->set("bootstrap.servers", bootstrap_servers, conf_errstr) != RdKafka::Conf::CONF_OK)
    {
        ADD_FAILURE() << "Failed to set bootstrap servers for admin client: " << conf_errstr;
        return;
    }
    if (admin_conf->set("client.id", "k2eg-topic-preparer", conf_errstr) != RdKafka::Conf::CONF_OK)
    {
        ADD_FAILURE() << "Failed to set client.id for admin client: " << conf_errstr;
        return;
    }

    auto producer = std::unique_ptr<RdKafka::Producer>(RdKafka::Producer::create(admin_conf.get(), conf_errstr));
    if (!producer)
    {
        ADD_FAILURE() << "Failed to create librdkafka producer for admin operations: " << conf_errstr;
        return;
    }

    std::unique_ptr<rd_kafka_NewTopic_t*, k2eg::service::pubsub::impl::kafka::RdKafkaNewTopicArrayDeleter> new_topics(
        static_cast<rd_kafka_NewTopic_t**>(malloc(sizeof(rd_kafka_NewTopic_t*))),
        k2eg::service::pubsub::impl::kafka::RdKafkaNewTopicArrayDeleter(1));
    if (!new_topics)
    {
        ADD_FAILURE() << "Failed to allocate topic descriptor array";
        return;
    }

    new_topics.get()[0] = rd_kafka_NewTopic_new(
        topic_name.c_str(),
        partitions > 0 ? partitions : 1,
        replication_factor > 0 ? replication_factor : 1,
        errstr,
        errstr_cnt);
    if (!new_topics.get()[0])
    {
        ADD_FAILURE() << "Failed to initialise topic descriptor: " << errstr;
        return;
    }

    for (const auto& [key, value] : topic_config)
    {
        auto err = rd_kafka_NewTopic_set_config(new_topics.get()[0], key.c_str(), value.c_str());
        if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
        {
            ADD_FAILURE() << "Failed to set topic configuration '" << key << "': " << rd_kafka_err2str(err);
            return;
        }
    }

    std::unique_ptr<rd_kafka_AdminOptions_t, k2eg::service::pubsub::impl::kafka::RdKafkaAdminOptionDeleter> admin_options(
        rd_kafka_AdminOptions_new(producer->c_ptr(), RD_KAFKA_ADMIN_OP_CREATETOPICS),
        k2eg::service::pubsub::impl::kafka::RdKafkaAdminOptionDeleter());
    if (!admin_options)
    {
        ADD_FAILURE() << "Failed to allocate admin options";
        return;
    }

    const int request_timeout_ms = static_cast<int>(admin_timeout.count());
    const int operation_timeout_ms = std::max(1000, request_timeout_ms - 1000);
    if (auto err = rd_kafka_AdminOptions_set_request_timeout(admin_options.get(), request_timeout_ms, errstr, errstr_cnt);
        err != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
        ADD_FAILURE() << "Failed to set admin request timeout: " << errstr;
        return;
    }
    if (auto err = rd_kafka_AdminOptions_set_operation_timeout(admin_options.get(), operation_timeout_ms, errstr, errstr_cnt);
        err != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
        ADD_FAILURE() << "Failed to set admin operation timeout: " << errstr;
        return;
    }

    std::unique_ptr<rd_kafka_queue_t, k2eg::service::pubsub::impl::kafka::RdKafkaQueueDeleter> queue(
        rd_kafka_queue_new(producer->c_ptr()),
        k2eg::service::pubsub::impl::kafka::RdKafkaQueueDeleter());
    if (!queue)
    {
        ADD_FAILURE() << "Failed to allocate librdkafka queue";
        return;
    }

    rd_kafka_CreateTopics(producer->c_ptr(), new_topics.get(), 1, admin_options.get(), queue.get());

    std::unique_ptr<rd_kafka_event_t, k2eg::service::pubsub::impl::kafka::RdKafkaEventDeleter> event;
    const auto                                             admin_deadline = std::chrono::steady_clock::now() + admin_timeout;
    while (std::chrono::steady_clock::now() < admin_deadline)
    {
        rd_kafka_event_t* raw_event = rd_kafka_queue_poll(queue.get(), 100);
        if (!raw_event)
        {
            continue;
        }
        event.reset(raw_event);
        if (rd_kafka_event_type(event.get()) == RD_KAFKA_EVENT_CREATETOPICS_RESULT)
        {
            break;
        }
        // Log unexpected errors and keep polling until deadline.
        if (rd_kafka_event_error(event.get()) != RD_KAFKA_RESP_ERR_NO_ERROR)
        {
            std::cerr << "[WARN] unexpected admin event error: " << rd_kafka_event_error_string(event.get()) << std::endl;
        }
        event.reset();
    }

    if (!event)
    {
        ADD_FAILURE() << "Timed out waiting for CreateTopics result for '" << topic_name << "'";
        return;
    }

    const rd_kafka_CreateTopics_result_t* result = rd_kafka_event_CreateTopics_result(event.get());
    size_t                                topic_count = 0;
    const rd_kafka_topic_result_t**       topic_results = rd_kafka_CreateTopics_result_topics(result, &topic_count);
    bool                                  creation_ok = false;
    for (size_t i = 0; i < topic_count; ++i)
    {
        const auto* tres = topic_results[i];
        auto        err = rd_kafka_topic_result_error(tres);
        if (err == RD_KAFKA_RESP_ERR_NO_ERROR || err == RD_KAFKA_RESP_ERR_TOPIC_ALREADY_EXISTS)
        {
            creation_ok = true;
            continue;
        }
        ADD_FAILURE() << "Failed to create topic '" << rd_kafka_topic_result_name(tres) << "': "
                      << rd_kafka_err2name(err) << " - " << rd_kafka_topic_result_error_string(tres);
        return;
    }

    if (!creation_ok)
    {
        ADD_FAILURE() << "CreateTopics returned no usable results for '" << topic_name << "'";
        return;
    }

    // Poll metadata until the topic reports ready.
    std::unique_ptr<RdKafka::Conf> consumer_conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!consumer_conf)
    {
        ADD_FAILURE() << "Failed to allocate metadata consumer configuration";
        return;
    }
    conf_errstr.clear();
    if (consumer_conf->set("bootstrap.servers", bootstrap_servers, conf_errstr) != RdKafka::Conf::CONF_OK)
    {
        ADD_FAILURE() << "Failed to set bootstrap servers for metadata consumer: " << conf_errstr;
        return;
    }
    if (consumer_conf->set("group.id", "k2eg-metadata-" + k2eg::common::UUID::generateUUIDLite(), conf_errstr) != RdKafka::Conf::CONF_OK)
    {
        ADD_FAILURE() << "Failed to set group.id for metadata consumer: " << conf_errstr;
        return;
    }
    if (consumer_conf->set("enable.auto.commit", "false", conf_errstr) != RdKafka::Conf::CONF_OK)
    {
        ADD_FAILURE() << "Failed to disable auto commit for metadata consumer: " << conf_errstr;
        return;
    }

    auto consumer = std::unique_ptr<RdKafka::KafkaConsumer>(RdKafka::KafkaConsumer::create(consumer_conf.get(), conf_errstr));
    if (!consumer)
    {
        ADD_FAILURE() << "Failed to create metadata consumer: " << conf_errstr;
        return;
    }

    const auto ready_deadline = std::chrono::steady_clock::now() + readiness_timeout;
    bool       ready = false;
    while (std::chrono::steady_clock::now() < ready_deadline)
    {
        RdKafka::Metadata* metadata = nullptr;
        auto               md_err = consumer->metadata(true, nullptr, &metadata, static_cast<int>(admin_timeout.count()));
        if (md_err == RdKafka::ERR_NO_ERROR && metadata)
        {
            const auto* topics = metadata->topics();
            if (topics != nullptr)
            {
                for (const auto* topic_md : *topics)
                {
                    if (!topic_md)
                    {
                        continue;
                    }
                    if (topic_md->topic() == topic_name && topic_md->err() == RdKafka::ERR_NO_ERROR)
                    {
                        ready = true;
                        break;
                    }
                }
            }
        }
        if (metadata)
        {
            delete metadata;
        }
        if (ready)
        {
            break;
        }
        std::this_thread::sleep_for(poll_interval);
    }

    consumer->close();

    if (!ready)
    {
        ADD_FAILURE() << "Timed out waiting for topic '" << topic_name << "' to appear in metadata";
    }
}

/**
 * @brief Test environment bootstrapping K2EG for integration tests.
 */
class K2EGTestEnv : public k2eg::K2EG
{
public:
    /**
     * @brief Initialize K2EG with test defaults.
     */
    K2EGTestEnv()
    {
        int         argc = 1;
        const char* argv[1] = {"epics-k2eg-test"};
        if (K2EG::setup(argc, argv))
        {
            init();
        }
    }

    /**
     * @brief Shutdown K2EG on destruction.
     */
    ~K2EGTestEnv()
    {
        deinit();
    }

    k2eg::service::log::ILoggerShrdPtr getLoggerReference()
    {
        return k2eg::service::ServiceResolver<k2eg::service::log::ILogger>::resolve();
    }

    k2eg::controller::node::NodeController& getNodeControllerReference()
    {
        return *this->node_controller;
    }

    /**
     * @brief Create a Kafka publisher bound to test config.
     * @return Shared pointer to publisher.
     */
    k2eg::service::pubsub::IPublisherShrdPtr getPublisherInstance()
    {
        return k2eg::service::pubsub::impl::kafka::MakeRDKafkaPublisherShrdPtr(po->getPublisherConfiguration());
    }

    /**
     * @brief Create a Kafka publisher with custom configuration.
     * @param configuration Publisher configuration to use.
     * @return Shared pointer to publisher.
     */
    k2eg::service::pubsub::IPublisherShrdPtr getPublisherInstance(k2eg::service::pubsub::ConstPublisherConfigurationShrdPtr configuration)
    {
        return k2eg::service::pubsub::impl::kafka::MakeRDKafkaPublisherShrdPtr(configuration);
    }

    /**
     * @brief Create a Kafka subscriber bound to test config.
     * @param queue Optional queue to subscribe immediately.
     * @return Shared pointer to subscriber.
     */
    k2eg::service::pubsub::ISubscriberShrdPtr getSubscriberInstance(const std::string& queue, const std::string& client_name="generaic-test-subscriber-prefix")
    {
        // Build a const SubscriberConfiguration with a random group id and custom client.id
        auto sub_conf = std::make_shared<const k2eg::service::pubsub::SubscriberConfiguration>(
            k2eg::service::pubsub::SubscriberConfiguration{
                po->getSubscriberConfiguration()->server_address,
                std::string("subscriber-random-test-") + k2eg::common::UUID::generateUUIDLite(),
                // Add conservative consumer timing settings to avoid heartbeat expiration
                // during long test sleeps / rebalances in CI environments.
                k2eg::common::MapStrKV{
                    {"client.id", std::any(std::string(client_name + "-" + k2eg::common::UUID::generateUUIDLite()))},
                    {"session.timeout.ms", std::any(std::string("60000"))},
                    {"heartbeat.interval.ms", std::any(std::string("3000"))},
                    {"max.poll.interval.ms", std::any(std::string("300000"))},
                    // If the consumer subscribes after the first message is produced,
                    // prefer earliest so test consumers don't miss replies due to 'latest'.
                    {"auto.offset.reset", std::any(std::string("earliest"))}
                }
            });

        auto subscriber = k2eg::service::pubsub::impl::kafka::MakeRDKafkaSubscriberShrdPtr(sub_conf);
        if (!queue.empty())
        {
            subscriber->setQueue({queue});
            // Wait for the consumer to be assigned partitions to avoid race where
            // the test publishes messages before the consumer is ready.
            subscriber->waitForAssignment(10000);
        }
        return subscriber;
    }

    /**
     * @brief Create a MongoDB storage service instance.
     * @return Shared pointer to storage service.
     */
    k2eg::service::storage::IStorageServiceShrdPtr getStorageServiceInstance()
    {
        // create one if not running a storage-capable node
        return k2eg::service::storage::StorageServiceFactory::create(po->getStorageServiceConfiguration());
    }

    /**
     * @brief Get the gateway command topic.
     * @return Topic string used for commands.
     */
    const std::string& getGatewayCMDTopic()
    {
        return po->getOption<std::string>(CMD_INPUT_TOPIC);
    }

    /**
     * @brief Publish a command message with basic safety checks.
     * @param publisher Publisher to use.
     * @param command Serialized command message.
     */
    void sendCommand(k2eg::service::pubsub::IPublisherShrdPtr publisher, k2eg::service::pubsub::PublishMessageUniquePtr command)
    {
        if (!publisher)
        {
            ADD_FAILURE() << "publisher is null";
            return;
        }
        if (!command)
        {
            ADD_FAILURE() << "command is null";
            return;
        }

        publisher->pushMessage(std::move(command));
    }

    /**
     * @brief Drain messages from a subscriber until count or timeout.
     * @param subscriber Subscriber to read from.
     * @param num_of_msg Target number of messages.
     * @param timeout_ms Max wait in milliseconds (<=0 waits indefinitely).
     * @return Collected messages up to requested count.
     */
    k2eg::service::pubsub::SubscriberInterfaceElementVector getMessages(k2eg::service::pubsub::ISubscriberShrdPtr subscriber, int num_of_msg, int timeout_ms = 10000)
    {
        if (!subscriber)
        {
            ADD_FAILURE() << "Subscriber is null";
            return {};
        }
        k2eg::service::pubsub::SubscriberInterfaceElementVector mesg_received;
        auto                                                    start = std::chrono::steady_clock::now();

        while (mesg_received.size() < static_cast<size_t>(num_of_msg))
        {
            subscriber->getMsg(mesg_received, num_of_msg);

            if (timeout_ms > 0)
            {
                auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
                if (elapsed_ms >= timeout_ms)
                    break;
            }

            if (mesg_received.size() < static_cast<size_t>(num_of_msg))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        if (mesg_received.size() < static_cast<size_t>(num_of_msg) && timeout_ms > 0)
        {
            std::cout << "[WARN] getJsonMessage timeout: expected "
                      << num_of_msg << " got " << mesg_received.size()
                      << " within " << timeout_ms << " ms\n";
        }

        return mesg_received;
    }

    /**
     * @brief Poll storage and return all snapshot IDs found within a time range.
     *
     * Repeatedly queries `listSnapshotIdsInRange` and paginates within each poll
     * iteration until all IDs in the range are returned by the storage layer.
     * Stops early if any IDs are found, or when attempts/timeout are reached.
     * Duplicate IDs are de-duplicated in the result.
     *
     * @param storage_service Storage service instance.
     * @param lookback Amount of time to look back from now (default 2 minutes).
     * @param page_size Page size for each storage call (default 10).
     * @param max_attempts Maximum polling attempts (default 10).
     * @param max_total Maximum wall-clock time to wait (default 30s).
     * @param sleep_between Delay between attempts (default 500ms).
     * @return Vector with all unique snapshot IDs found.
     */
    std::vector<std::string> waitForSnapshotIdsInRange(
        k2eg::service::storage::IStorageServiceShrdPtr storage_service,
        const std::chrono::system_clock::duration&     lookback = std::chrono::minutes(2),
        size_t                                         page_size = 10,
        int                                            max_attempts = 10,
        std::chrono::milliseconds                      max_total = std::chrono::seconds(120),
        std::chrono::milliseconds                      sleep_between = std::chrono::milliseconds(500))
    {
        if (!storage_service)
        {
            ADD_FAILURE() << "Storage service is null";
            return {};
        }

        std::vector<std::string>        all_ids;
        std::unordered_set<std::string> seen;
        int                             attempts = 0;
        const auto                      deadline = std::chrono::steady_clock::now() + max_total;
        while (attempts < max_attempts && std::chrono::steady_clock::now() < deadline)
        {
            const auto start_time = std::chrono::system_clock::now() - lookback;
            const auto end_time = std::chrono::system_clock::now();

            std::optional<std::string> token;
            bool                       has_more = true;
            while (has_more)
            {
                auto result = storage_service->listSnapshotIdsInRange(
                    start_time,
                    end_time,
                    page_size,
                    token);

                for (const auto& id : result.snapshot_ids)
                {
                    if (seen.insert(id).second)
                        all_ids.push_back(id);
                }

                has_more = result.has_more;
                token = result.continuation_token;
                if (!has_more)
                    break;
            }

            if (!all_ids.empty())
            {
                break;
            }
            std::this_thread::sleep_for(sleep_between);
            ++attempts;
        }
        return all_ids;
    }

    /**
     * @brief Parse a subscriber message into a JSON object.
     * @param message Subscriber message buffer and size.
     * @return Parsed JSON object; adds a test failure on parse errors.
     */
    boost::json::object getJsonObject(const k2eg::service::pubsub::SubscriberInterfaceElement& message)
    {
        boost::json::object result;
        try
        {
            result = boost::json::parse(std::string(message.data.get(), message.data_len)).as_object();
        }
        catch (const std::exception& ex)
        {
            ADD_FAILURE() << "JSON parse failed: " << ex.what();
        }
        return result;
    }

    /**
     * @brief Zone-safe holder for a map view over a Msgpack-encoded payload.
     * Keeps the msgpack object_handle (and its zone) alive while referencing objects in the map.
     */
    struct MsgpackMapView
    {
        msgpack::object_handle                           handle;
        std::unordered_map<std::string, msgpack::object> map;

        bool valid() const
        {
            return handle.get().type != msgpack::type::NIL || !map.empty();
        }
    };

    /**
     * @brief Deserialize a MessagePack-encoded subscriber payload into a zone-safe map view.
     * @param message Subscriber message containing Msgpack bytes.
     * @return MsgpackMapView with live zone and map of key->object.
     */
    MsgpackMapView getMsgpackObject(const k2eg::service::pubsub::SubscriberInterfaceElement& message)
    {
        MsgpackMapView             out;
        std::vector<unsigned char> buff(message.data_len);
        std::memcpy(buff.data(), message.data.get(), message.data_len);
        try
        {
            // Build a local handle and move it into the view to keep the zone alive
            msgpack::object_handle oh;
            msgpack::unpack(oh, reinterpret_cast<const char*>(buff.data()), buff.size());
            out.handle = std::move(oh);
            out.handle.get().convert(out.map);
        }
        catch (const std::exception& ex)
        {
            ADD_FAILURE() << "Msgpack unpack failed: " << ex.what();
            out.map.clear();
        }
        return out;
    }

    std::shared_ptr<const k2eg::service::pubsub::SubscriberInterfaceElement> waitForReplyID(k2eg::service::pubsub::ISubscriberShrdPtr subscriber, const std::string& reply_id, k2eg::common::SerializationType serialization_type, int timeout_ms = 1000)
    {
        if (!subscriber)
        {
            ADD_FAILURE() << "Subscriber is null";
            return nullptr;
        }

        auto start = std::chrono::steady_clock::now();
        while (true)
        {
            k2eg::service::pubsub::SubscriberInterfaceElementVector messages;
            subscriber->getMsg(messages, 1);

            for (const auto& msg : messages)
            {
                if (serialization_type == k2eg::common::SerializationType::JSON)
                {
                    auto json_obj = getJsonObject(*msg);
                    if (json_obj.if_contains("reply_id") && json_obj.at("reply_id").is_string() && json_obj.at("reply_id").get_string() == reply_id)
                    {
                        return msg;
                    }
                }
                else
                {
                    auto mv = getMsgpackObject(*msg);
                    auto it = mv.map.find("reply_id");
                    if (it != mv.map.end())
                    {
                        std::string rid;
                        try
                        {
                            it->second.convert(rid);
                        }
                        catch (...)
                        {
                            rid.clear();
                        }
                        if (rid == reply_id)
                        {
                            return msg;
                        }
                    }
                }
            }

            if (timeout_ms > 0)
            {
                auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
                if (elapsed_ms >= timeout_ms)
                    break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return nullptr;
    }
};

/**
 * @brief Create and start a K2EG test environment for unit/integration tests.
 *
 * This helper clears the current process environment and sets a collection of
 * test-specific environment variables appropriate for the requested node
 * type. It then constructs a {@link K2EGTestEnv} instance which calls
 * `K2EG::setup(...)` and, on success, `K2EG::init()`.
 *
 * The returned shared pointer owns the running test environment; when the
 * shared pointer is destroyed the environment's destructor calls
 * `K2EG::deinit()` to shut down services and threads.
 *
 * @param tcp_port  [in,out] A port base used to assign the metrics HTTP port.
 *                  The function increments this value and uses the incremented
 *                  value when setting the corresponding environment variable
 *                  so tests can avoid port collisions.
 * @param type      Node type to start: GATEWAY, STORAGE or FULL. Controls which
 *                  node-specific environment variables are set.
 * @param enable_debug_log  If true, enables console logging and sets the log
 *                  level to debug through environment variables.
 * @param reset_conf If true, sets the configuration-reset-on-start variable so
 *                   the configuration store is reset for a clean test run.
 *
 * @return A non-null std::shared_ptr<K2EGTestEnv> on success. The caller owns
 *         the returned pointer and may call reset() to stop the environment.
 *
 * @throws std::runtime_error if an unknown NodeType is provided.
 *
 * @note This function calls clearenv() and therefore removes all existing
 * environment variables before setting test-specific values. Only use in test
 * processes where this behavior is acceptable.
 *
 * Environment variables set (non-exhaustive):
 *  - EPICS_k2eg_log-on-console
 *  - EPICS_k2eg_log-level
 *  - EPICS_k2eg_configuration-reset-on-start
 *  - EPICS_k2eg_node-type
 *  - EPICS_k2eg_<CMD_INPUT_TOPIC> (gateway/full)
 *  - EPICS_k2eg_<NC_MONITOR_EXPIRATION_TIMEOUT> (gateway/full)
 *  - EPICS_k2eg_<MONGODB_CONNECTION_STRING_KEY> (storage/full)
 *  - EPICS_k2eg_<SCHEDULER_CHECK_EVERY_AMOUNT_OF_SECONDS>
 *  - EPICS_k2eg_<CONFIGURATION_SERVICE_HOST>
 *  - EPICS_k2eg_<METRIC_ENABLE>
 *  - EPICS_k2eg_<METRIC_HTTP_PORT> (uses ++tcp_port)
 *  - EPICS_k2eg_<PUB_SERVER_ADDRESS>
 *  - EPICS_k2eg_<SUB_SERVER_ADDRESS>
 *
 * Example:
 * @code{cpp}
 * int tcp = 9000;
 * auto env = startK2EG(tcp, k2eg::controller::node::NodeType::STORAGE, true, true);
 * // use env for test; env.reset() will stop the environment
 * @endcode
 */
inline std::shared_ptr<K2EGTestEnv> startK2EG(
    int&                                                tcp_port,
    k2eg::controller::node::NodeType                    type,
    bool                                                enable_debug_log = false,
    bool                                                reset_conf = true,
    const std::unordered_map<std::string, std::string>& env_overrides = {})
{
    clearenv();
    auto get_override = [&](const std::string& key, const std::string& defval) -> std::string
    {
        auto it = env_overrides.find(key);
        return it != env_overrides.end() ? it->second : defval;
    };
    auto set_env_for = [&](const std::string& key, const std::string& defval)
    {
        auto val = get_override(key, defval);
        setenv(key.c_str(), val.c_str(), 1);
    };
    if (enable_debug_log)
    {
        set_env_for("EPICS_k2eg_log-on-console", "true");
        set_env_for("EPICS_k2eg_log-level", "debug");
        set_env_for("EPICS_k2eg_" + std::string(LOG_DEBUG_INFO), "true");
    }
    else
    {
        set_env_for("EPICS_k2eg_log-on-console", "false");
    }

    if (reset_conf)
    {
        set_env_for("EPICS_k2eg_configuration-reset-on-start", "true");
    }

    // apply all the overrides
    for (const auto& [key, value] : env_overrides)
    {
        setenv(key.c_str(), value.c_str(), 1);
    }

    switch (type)
    {
    case k2eg::controller::node::NodeType::GATEWAY:
        set_env_for("EPICS_k2eg_node-type", "gateway");
        set_env_for("EPICS_k2eg_" + std::string(CMD_INPUT_TOPIC), "cmd-in-topic");
        set_env_for("EPICS_k2eg_" + std::string(NC_MONITOR_EXPIRATION_TIMEOUT), "1");
        break;
    case k2eg::controller::node::NodeType::STORAGE:
        set_env_for("EPICS_k2eg_node-type", "storage");
        set_env_for("EPICS_k2eg_" + std::string(k2eg::service::storage::impl::MONGODB_CONNECTION_STRING_KEY), "mongodb://admin:admin@mongodb-primary:27017");
        break;
    case k2eg::controller::node::NodeType::FULL:
        set_env_for("EPICS_k2eg_node-type", "full");
        set_env_for("EPICS_k2eg_" + std::string(CMD_INPUT_TOPIC), "cmd-in-topic");
        set_env_for("EPICS_k2eg_" + std::string(NC_MONITOR_EXPIRATION_TIMEOUT), "1");
        set_env_for("EPICS_k2eg_" + std::string(k2eg::service::storage::impl::MONGODB_CONNECTION_STRING_KEY), "mongodb://admin:admin@mongodb-primary:27017");
        break;
    default:
        throw std::runtime_error("Unknown node type");
    }
    set_env_for("EPICS_k2eg_" + std::string(SCHEDULER_CHECK_EVERY_AMOUNT_OF_SECONDS), "1");
    // set monitor expiration time out at minimum
    set_env_for("EPICS_k2eg_" + std::string(CONFIGURATION_SERVICE_HOST), "consul");
    set_env_for("EPICS_k2eg_" + std::string(METRIC_ENABLE), "true");
    // Always increment the tcp_port to avoid collisions; allow override for the value used.
    ++tcp_port;
    set_env_for("EPICS_k2eg_" + std::string(METRIC_HTTP_PORT), std::to_string(tcp_port));
    set_env_for("EPICS_k2eg_" + std::string(PUB_SERVER_ADDRESS), "kafka:9092");
    set_env_for("EPICS_k2eg_" + std::string(SUB_SERVER_ADDRESS), "kafka:9092");
    return std::make_shared<K2EGTestEnv>();
}

#endif // NODEUTILITIES_H_
