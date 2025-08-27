#ifndef NODEUTILITIES_H_
#define NODEUTILITIES_H_

#include "boost/json/object.hpp"
#include "k2eg/common/types.h"
#include <cstddef>
#include <gtest/gtest.h>
#include <k2eg/k2eg.h>

#include <k2eg/common/ProgramOptions.h>
#include <k2eg/common/uuid.h>

#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/command/cmd/SnapshotCommand.h>
#include <k2eg/controller/node/NodeController.h>

#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaPublisher.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaSubscriber.h>
#include <k2eg/service/storage/StorageServiceFactory.h>
#include <k2eg/service/storage/impl/MongoDBStorageService.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

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

    /**
     * @brief Create a Kafka publisher bound to test config.
     * @return Shared pointer to publisher.
     */
    k2eg::service::pubsub::IPublisherShrdPtr getPublisherInstance()
    {
        return k2eg::service::pubsub::impl::kafka::MakeRDKafkaPublisherShrdPtr(po->getPublisherConfiguration());
    }

    /**
     * @brief Create a Kafka subscriber bound to test config.
     * @param queue Optional queue to subscribe immediately.
     * @return Shared pointer to subscriber.
     */
    k2eg::service::pubsub::ISubscriberShrdPtr getSubscriberInstance(const std::string& queue = "")
    {
        auto subscriber = k2eg::service::pubsub::impl::kafka::MakeRDKafkaSubscriberShrdPtr(po->getSubscriberConfiguration());
        if (!queue.empty())
            subscriber->setQueue({queue});
        return subscriber;
    }

    /**
     * @brief Create a MongoDB storage service instance.
     * @return Shared pointer to storage service.
     */
    k2eg::service::storage::IStorageServiceShrdPtr getStorageServiceInstance()
    {
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

    std::shared_ptr<const k2eg::service::pubsub::SubscriberInterfaceElement> waitForReplyID(k2eg::service::pubsub::ISubscriberShrdPtr subscriber, const std::string& reply_id, int timeout_ms = 1000)
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
                auto json_obj = getJsonObject(*msg);
                if (json_obj.at("reply_id").get_string() == reply_id)
                {
                    return msg;
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
inline std::shared_ptr<K2EGTestEnv> startK2EG(int& tcp_port, k2eg::controller::node::NodeType type, bool enable_debug_log = false, bool reset_conf = true)
{
    clearenv();
    if (enable_debug_log)
    {
        setenv("EPICS_k2eg_log-on-console", "true", 1);
        setenv("EPICS_k2eg_log-level", "debug", 1);
        setenv(("EPICS_k2eg_" + std::string(LOG_DEBUG_INFO)).c_str(), "true", 1);
    }
    else
    {
        setenv("EPICS_k2eg_log-on-console", "false", 1);
    }

    if (reset_conf)
    {
        setenv("EPICS_k2eg_configuration-reset-on-start", "true", 1);
    }

    switch (type)
    {
    case k2eg::controller::node::NodeType::GATEWAY:
        setenv("EPICS_k2eg_node-type", "gateway", 1);
        setenv(("EPICS_k2eg_" + std::string(CMD_INPUT_TOPIC)).c_str(), "cmd-in-topic", 1);
        setenv(("EPICS_k2eg_" + std::string(NC_MONITOR_EXPIRATION_TIMEOUT)).c_str(), "1", 1);
        break;
    case k2eg::controller::node::NodeType::STORAGE:
        setenv("EPICS_k2eg_node-type", "storage", 1);
        setenv(("EPICS_k2eg_" + std::string(k2eg::service::storage::impl::MONGODB_CONNECTION_STRING_KEY)).c_str(), "mongodb://admin:admin@mongodb-primary:27017", 1);
        break;
    case k2eg::controller::node::NodeType::FULL:
        setenv("EPICS_k2eg_node-type", "full", 1);
        setenv(("EPICS_k2eg_" + std::string(CMD_INPUT_TOPIC)).c_str(), "cmd-in-topic", 1);
        setenv(("EPICS_k2eg_" + std::string(NC_MONITOR_EXPIRATION_TIMEOUT)).c_str(), "1", 1);
        setenv(("EPICS_k2eg_" + std::string(k2eg::service::storage::impl::MONGODB_CONNECTION_STRING_KEY)).c_str(), "mongodb://admin:admin@mongodb-primary:27017", 1);
        break;
    default:
        throw std::runtime_error("Unknown node type");
    }
    setenv(("EPICS_k2eg_" + std::string(SCHEDULER_CHECK_EVERY_AMOUNT_OF_SECONDS)).c_str(), "1", 1);
    // set monitor expiration time out at minimum
    setenv(("EPICS_k2eg_" + std::string(CONFIGURATION_SERVICE_HOST)).c_str(), "consul", 1);
    setenv(("EPICS_k2eg_" + std::string(METRIC_ENABLE)).c_str(), "true", 1);
    setenv(("EPICS_k2eg_" + std::string(METRIC_HTTP_PORT)).c_str(), std::to_string(++tcp_port).c_str(), 1);
    setenv(("EPICS_k2eg_" + std::string(PUB_SERVER_ADDRESS)).c_str(), "kafka:9092", 1);
    setenv(("EPICS_k2eg_" + std::string(SUB_SERVER_ADDRESS)).c_str(), "kafka:9092", 1);
    return std::make_shared<K2EGTestEnv>();
}

#endif // NODEUTILITIES_H_
