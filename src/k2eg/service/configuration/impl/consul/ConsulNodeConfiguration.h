#ifndef K2EG_SERVICE_CONFIGURATION_IMPL_CONSUL_ICONSULENODECONFIGURATIONS_H_
#define K2EG_SERVICE_CONFIGURATION_IMPL_CONSUL_ICONSULENODECONFIGURATIONS_H_

#include <k2eg/service/configuration/INodeConfiguration.h>

#include <oatpp-consul/Client.hpp>

namespace k2eg::service::configuration::impl::consul {

    /*
    Consul impelmentation of the INodeConfiguration interface.

    It permit to read ans store node configuration in a Consul KV store.
    */
class ConsuleNodeConfiguration : public INodeConfiguration {
    // Consul client
    std::shared_ptr<oatpp::consul::Client> client;
    // node configuration key
    std::string node_configuration_key;

    std::string getNodeKey();
public:
    ConsuleNodeConfiguration(ConstConfigurationServceiConfigUPtr config);
    virtual ~ConsuleNodeConfiguration();

    ConstNodeConfigurationShrdPtr getNodeConfiguration() const override;
    bool setNodeConfiguration(ConstNodeConfigurationShrdPtr node_configuration) override;

};
}

#endif // K2EG_SERVICE_CONFIGURATION_IMPL_CONSUL_ICONSULENODECONFIGURATIONS_H_