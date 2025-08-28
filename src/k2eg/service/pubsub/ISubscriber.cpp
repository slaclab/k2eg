#include <k2eg/service/pubsub/ISubscriber.h>

using namespace k2eg::service::pubsub;

ISubscriber::ISubscriber(ConstSubscriberConfigurationShrdPtr configuration)
    : configuration(std::move(configuration)) {
    if (this->configuration->server_address.empty()) {
        throw std::runtime_error("The pub/sub server address is mandatory");
    }
}
