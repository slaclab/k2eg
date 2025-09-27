#ifndef ISUBSCRIBER_H
#define ISUBSCRIBER_H

#include <k2eg/common/types.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <any>

namespace k2eg::service::pubsub {

/**
 * @brief Subscriber configuration parameters.
 */
struct SubscriberConfiguration {
    /** @brief Subscriber broker address (host:port). */
    std::string server_address;
    /** @brief Subscriber group id. */
    std::string group_id;
    /**
     * @brief Implementation-specific overrides.
     *
     * Keys and values are implementation-defined; values are stored as
     * scalar types in std::any (commonly std::string, integers, bool).
     */
    k2eg::common::MapStrKV custom_impl_parameter;
};
DEFINE_PTR_TYPES(SubscriberConfiguration)

DEFINE_MAP_FOR_TYPE(std::string, std::string, SubscriberHeaders)
/**
 * @brief Message received from a subscriber.
 *
 * Owns payload memory and exposes optional commit handle.
 */
typedef struct SubscriberInterfaceElement {
    SubscriberHeaders header;
    const std::string key;
    const size_t data_len;
    std::unique_ptr<const char[]> data;
    /**
     * @brief Commit handle for acknowledging a single message.
     *
     * The underlying implementation may populate partition/offset and execute
     * an optional action when the message is committed.
     */
    struct CommitHandle
    {
        std::string topic;
        int32_t partition{0};
        int64_t offset_to_commit{0};
        std::function<void()> on_commit_action; // optional
    };
    std::shared_ptr<CommitHandle> commit_handle;
} SubscriberInterfaceElement;

DEFINE_VECTOR_FOR_TYPE(std::shared_ptr<const SubscriberInterfaceElement>, SubscriberInterfaceElementVector);

typedef std::function<void(SubscriberInterfaceElement&)> SubscriberInterfaceHandler;

typedef enum ConsumerInterfaceEventType { ONDELIVERY, ONARRIVE, ONERROR } ConsumerInterfaceEventType;

/**
 * @brief Abstract subscriber interface.
 *
 * Implementations should honor configuration and optional runtime overrides.
 */
class ISubscriber {
protected:
    DEFINE_MAP_FOR_TYPE(ConsumerInterfaceEventType, SubscriberInterfaceHandler, handlers)
    const ConstSubscriberConfigurationShrdPtr configuration;
    std::unordered_map<std::string, std::any> runtime_overrides_;
public:
    /**
     * @brief Construct with configuration.
     * @param configuration shared immutable subscriber configuration
     */
    ISubscriber(ConstSubscriberConfigurationShrdPtr configuration);
    /**
     * @brief Construct with configuration and runtime overrides.
     * @param configuration shared immutable subscriber configuration
     * @param overrides implementation-specific runtime overrides
     */
    ISubscriber(ConstSubscriberConfigurationShrdPtr configuration, const std::unordered_map<std::string, std::any>& overrides);
    virtual ~ISubscriber() = default;
    /** @brief Access provided runtime overrides. */
    const std::unordered_map<std::string, std::any>& getRuntimeOverrides() const { return runtime_overrides_; }
    /**
     * @brief Set the Topics where the consumer need to fetch data
     *
     * @param topics
     */
    virtual void setQueue(const k2eg::common::StringVector& queue) = 0;
    /** @brief Add topics to the existing subscription set. */
    virtual void addQueue(const k2eg::common::StringVector& queue) = 0;
    /** @brief Commit current offsets; async when true. */
    virtual void commit(const bool& async = false) = 0;
    /** @brief Commit a single message using its opaque handle. */
    virtual void commit(const std::shared_ptr<const void>& handle, const bool& async = false) = 0;
    /**
     * @brief Fetch messages synchronously.
     * @param dataVector destination vector
     * @param m_num number of messages to wait for
     * @param timeo timeout in milliseconds
     * @return 0 on success; implementation-defined error code otherwise
     */
    virtual int getMsg(SubscriberInterfaceElementVector& dataVector, unsigned int m_num, unsigned int timeo = 10) = 0;
    /**
     * @brief Wait until the underlying consumer has been assigned partitions.
     *
     * Implementations may block up to timeout_ms and return true when the
     * consumer has been assigned at least one partition. Default implementation
     * is a no-op that returns true so non-kafka implementations are unaffected.
     *
     * @param timeout_ms maximum time to wait in milliseconds
     * @return true if assigned, false on timeout or error
     */
    virtual bool waitForAssignment(int timeout_ms = 5000) { return true; }
};
DEFINE_PTR_TYPES(ISubscriber)
} // namespace k2eg::service::pubsub

#endif
