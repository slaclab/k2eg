// Lightweight helpers to register core services with ServiceResolver
#ifndef K2EG_SERVICE_SERVICEREGISTRATION_H_
#define K2EG_SERVICE_SERVICEREGISTRATION_H_

#include <k2eg/common/ProgramOptions.h>
#include <k2eg/service/ServiceResolver.h>

#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/log/impl/BoostLogger.h>

#include <k2eg/service/scheduler/Scheduler.h>

#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>

#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/DummyMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>

#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/ISubscriber.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaPublisher.h>
#include <k2eg/service/pubsub/impl/kafka/RDKafkaSubscriber.h>

#include <k2eg/service/epics/EpicsServiceManager.h>

#include <k2eg/service/storage/StorageServiceFactory.h>

namespace k2eg::service {

inline void registerLogger(common::ProgramOptions* po)
{
    ServiceResolver<log::ILogger>::registerService<log::ConstLogConfigurationShrdPtr, log::impl::BoostLogger>(po->getloggerConfiguration());
}

inline void registerScheduler(common::ProgramOptions* po)
{
    ServiceResolver<scheduler::Scheduler>::registerService<scheduler::ConstSchedulerConfigurationShrdPtr, scheduler::Scheduler>(po->getSchedulerConfiguration());
}

inline void registerConfigService(common::ProgramOptions* po)
{
    ServiceResolver<configuration::INodeConfiguration>::registerService<configuration::ConstConfigurationServiceConfigShrdPtr, configuration::impl::consul::ConsulNodeConfiguration>(po->getConfigurationServiceConfiguration());
}

inline void registerMetrics(common::ProgramOptions* po)
{
    auto cfg = po->getMetricConfiguration();
    if (cfg->enable)
    {
        ServiceResolver<metric::IMetricService>::registerService<metric::ConstMetricConfigurationShrdPtr, metric::impl::prometheus_impl::PrometheusMetricService>(cfg);
    }
    else
    {
        ServiceResolver<metric::IMetricService>::registerService<metric::ConstMetricConfigurationShrdPtr, metric::impl::DummyMetricService>(cfg);
    }
}

inline void registerPublisher(common::ProgramOptions* po)
{
    ServiceResolver<pubsub::IPublisher>::registerService<pubsub::ConstPublisherConfigurationShrdPtr, pubsub::impl::kafka::RDKafkaPublisher>(po->getPublisherConfiguration());
}

inline void registerSubscriber(common::ProgramOptions* po)
{
    ServiceResolver<pubsub::ISubscriber>::registerService<pubsub::ConstSubscriberConfigurationShrdPtr, pubsub::impl::kafka::RDKafkaSubscriber>(po->getSubscriberConfiguration());
}

inline void registerEpics(common::ProgramOptions* po)
{
    ServiceResolver<epics_impl::EpicsServiceManager>::registerService<epics_impl::ConstEpicsServiceManagerConfigShrdPtr, epics_impl::EpicsServiceManager>(po->getEpicsManagerConfiguration());
}

inline void registerStorage(common::ProgramOptions* po)
{
    ServiceResolver<storage::IStorageService>::registerFactory<storage::StorageServiceConfigurationUPtr>(
        po,
        &common::ProgramOptions::getStorageServiceConfiguration,
        &storage::StorageServiceFactory::create);
}

} // namespace k2eg::service

#endif // K2EG_SERVICE_SERVICEREGISTRATION_H_

