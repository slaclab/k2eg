#include <k2eg/service/pubsub/ISubscriber.h>

using namespace k2eg::service::pubsub;

ISubscriber::ISubscriber(ConstSubscriberConfigurationShrdPtr configuration)
    : configuration(std::move(configuration))
{
    if (this->configuration->server_address.empty())
    {
        throw std::runtime_error("The pub/sub server address is mandatory");
    }
}

ISubscriber::ISubscriber(ConstSubscriberConfigurationShrdPtr              configuration,
                         const std::unordered_map<std::string, std::any>& overrides)
    : configuration(std::move(configuration)), runtime_overrides_(overrides)
{
    if (this->configuration->server_address.empty())
    {
        throw std::runtime_error("The pub/sub server address is mandatory");
    }
}
