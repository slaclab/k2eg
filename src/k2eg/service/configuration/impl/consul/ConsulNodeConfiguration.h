#ifndef K2EG_SERVICE_CONFIGURATION_IMPL_CONSUL_ICONSULENODECONFIGURATIONS_H_
#define K2EG_SERVICE_CONFIGURATION_IMPL_CONSUL_ICONSULENODECONFIGURATIONS_H_

#include <k2eg/service/configuration/INodeConfiguration.h>

#include <oatpp-consul/Client.hpp>
#include <oatpp/core/Types.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>
#include <oatpp/web/client/RequestExecutor.hpp>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace k2eg::service::configuration::impl::consul {

/*
Consul impelmentation of the INodeConfiguration interface.

It permit to read ans store node configuration in a Consul KV store.
*/
class ConsulNodeConfiguration : public INodeConfiguration
{
    // Consul client
    std::shared_ptr<oatpp::consul::Client>               client;
    std::shared_ptr<oatpp::web::client::RequestExecutor> requestExecutor;
    // node configuration key
    std::string             node_configuration_key;
    std::string             session_id;
    std::atomic<bool>       session_active{false};
    std::thread             session_renewal_thread;
    std::condition_variable session_cv;
    std::mutex              session_mutex;
    // Execute a Consul transaction with provided operations
    bool executeTxn(const boost::json::array& ops) const;

    std::string                    getNodeKey() const;
    bool                           createSession();
    void                           renewSession();
    bool                           destroySession();
    const std::vector<std::string> kvGetKeys(const std::string& prefix) const;
    void                           registerService();
    void                           deregisterService();

public:
    ConsulNodeConfiguration(ConstConfigurationServiceConfigUPtr config);
    virtual ~ConsulNodeConfiguration();

    // Node configuration methods
    NodeConfigurationShrdPtr getNodeConfiguration() const override;
    bool                     setNodeConfiguration(NodeConfigurationShrdPtr node_configuration) override;
    std::string              getNodeName() const override;

    // Snapshot configuration methods
    const std::string                 getSnapshotKey(const std::string& snapshot_id) const override;
    ConstSnapshotConfigurationShrdPtr getSnapshotConfiguration(const std::string& snapshot_id) const override;
    bool                              setSnapshotConfiguration(const std::string& snapshot_id, SnapshotConfigurationShrdPtr snapshot_config) override;
    bool                              deleteSnapshotConfiguration(const std::string& snapshot_id) override;
    const std::vector<std::string>    getSnapshotIds() const override;

    // Distributed snapshot management methods
    bool                           isSnapshotRunning(const std::string& snapshot_id) const override;
    void                           setSnapshotRunning(const std::string& snapshot_id, bool running) override;
    bool                           isSnapshotArchiveRequested(const std::string& snapshot_id) const override;
    void                           setSnapshotArchiveRequested(const std::string& snapshot_id, bool archived) override;
    void                           setSnapshotArchiveStatus(const std::string& snapshot_id, const ArchiveStatusInfo& status) override;
    ArchiveStatusInfo              getSnapshotArchiveStatus(const std::string& snapshot_id) const override;
    void                           setSnapshotWeight(const std::string& snapshot_id, const std::string& weight, const std::string& weight_unit) override;
    const std::string              getSnapshotGateway(const std::string& snapshot_id) const override;
    const std::string              getSnapshotArchiver(const std::string& snapshot_id) const override;
    bool                           tryAcquireSnapshot(const std::string& snapshot_id, bool for_gateway) override;
    bool                           releaseSnapshot(const std::string& snapshot_id, bool for_gateway) override;
    const std::vector<std::string> getRunningSnapshots() const override;
    const std::vector<std::string> getSnapshots() const override;
    const std::vector<std::string> getAvailableSnapshot() const override;
    const std::vector<std::string> getRunningSnapshotToArchive() const override;
};
DEFINE_PTR_TYPES(ConsulNodeConfiguration)
} // namespace k2eg::service::configuration::impl::consul

#endif // K2EG_SERVICE_CONFIGURATION_IMPL_CONSUL_ICONSULENODECONFIGURATIONS_H_
