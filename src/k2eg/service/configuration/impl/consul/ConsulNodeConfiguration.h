#ifndef K2EG_SERVICE_CONFIGURATION_IMPL_CONSUL_ICONSULENODECONFIGURATIONS_H_
#define K2EG_SERVICE_CONFIGURATION_IMPL_CONSUL_ICONSULENODECONFIGURATIONS_H_

#include <k2eg/service/configuration/INodeConfiguration.h>

namespace k2eg::service::configuration::impl::consul {

    /*
    Consul impelmentation of the INodeConfiguration interface.

    It permit to read ans store node configuration in a Consul KV store.
    */
class ConsuleNodeConfiguration : public INodeConfiguration {
public:
    ConsuleNodeConfiguration(ConstConfigurationServceiConfigUPtr config);
    virtual ~ConsuleNodeConfiguration();

    ConstNodeConfigurationUPtr getNodeConfiguration() const override;
    void setNodeConfiguration(ConstNodeConfigurationUPtr node_configuration) override;

};
}

#endif // K2EG_SERVICE_CONFIGURATION_IMPL_CONSUL_ICONSULENODECONFIGURATIONS_H_