#ifndef ISUBSCRIBER_H
#define ISUBSCRIBER_H

#include <k2eg/common/types.h>

#include <functional>
#include <memory>
#include <string>

namespace k2eg::service::pubsub {

// publisher configuration
struct SubscriberConfiguration {
    // subscriber broker address
    std::string server_address;
    // subscriber group id
    std::string group_id;
    // custom k/v string map for implementation parameter
    k2eg::common::MapStrKV custom_impl_parameter;
};
DEFINE_PTR_TYPES(SubscriberConfiguration)

DEFINE_MAP_FOR_TYPE(std::string, std::string, SubscriberHeaders)
typedef struct SubscriberInterfaceElement {
    SubscriberHeaders header;
    const std::string key;
    const size_t data_len;
    std::unique_ptr<const char[]> data;
    // Commit handle that contains offset information and an optional on-commit action.
    // The on_commit_action will be executed by the underlying subscriber when the
    // message is committed; SnapshotArchiver will populate it to perform storage
    // only when the message is actually committed.
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

/*

 */
class ISubscriber {
protected:
    DEFINE_MAP_FOR_TYPE(ConsumerInterfaceEventType, SubscriberInterfaceHandler, handlers)
    const ConstSubscriberConfigurationUPtr configuration;
public:
    ISubscriber(ConstSubscriberConfigurationUPtr configuration);
    virtual ~ISubscriber() = default;
    /**
     * @brief Set the Topics where the consumer need to fetch data
     *
     * @param topics
     */
    virtual void setQueue(const k2eg::common::StringVector& queue) = 0;
    virtual void addQueue(const k2eg::common::StringVector& queue) = 0;
    virtual void commit(const bool& async = false) = 0;
    // Commit a single message using its opaque handle obtained from the SubscriberInterfaceElement
    virtual void commit(const std::shared_ptr<const void>& handle, const bool& async = false) = 0;
    //! Fetch in synchronous way the message
    /**
         waith until the request number of message are not received keeping in mind the timeout
     */
    virtual int getMsg(SubscriberInterfaceElementVector& dataVector, unsigned int m_num, unsigned int timeo = 10) = 0;
};
DEFINE_PTR_TYPES(ISubscriber)
} // namespace k2eg::service::pubsub

#endif