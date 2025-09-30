#ifndef K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_
#define K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_

#include "k2eg/service/metric/INodeControllerSystemMetric.h"
#include <k2eg/common/types.h>

#include <k2eg/service/metric/ICMDControllerMetric.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/INodeControllerMetric.h>
#include <k2eg/service/metric/IStorageNodeMetric.h>

#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace k2eg::service::metric::impl {

class DummyMetricService;

#define DEFINE_METRIC(type, counter_type)                                         \
  class Dummy##type : public type {                                               \
    friend class DummyMetricService;                                              \
    Dummy##type() = default;                                                      \
   public:                                                                        \
    std::map<counter_type, std::atomic<double>> counters;                         \
    virtual ~Dummy##type() = default;                                             \
    void                                                                          \
    incrementCounter(counter_type type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) override final {counters[type] = counters[type]+inc_value;} \
  };

// #define CONSTRUCT_METRIC(type, counter_type)                                      \
//   class Dummy##type : public type {                                               \
//     friend class DummyMetricService;                                              \
//     Dummy##type() = default;                                                      \
//    public:                                                                        \
//     std::map<counter_type, std::atomic<double>> counters;                         \
//     virtual ~Dummy##type() = default;                                             \
//     void                                                                          \
//     incrementCounter(counter_type type, const double inc_value = 1.0, const std::map<std::string, std::string>& label = {}) override final {counters[type] = counters[type]+inc_value;} \
//   };

DEFINE_METRIC(IEpicsMetric, IEpicsMetricCounterType)
DEFINE_METRIC(ICMDControllerMetric, ICMDControllerMetricCounterType)
DEFINE_METRIC(INodeControllerMetric, INodeControllerMetricCounterType)
DEFINE_METRIC(INodeControllerSystemMetric, INodeControllerSystemMetricType)
DEFINE_METRIC(IStorageNodeMetric, IStorageNodeMetricGaugeType)

// Dummy Metric services implementation
class DummyMetricService : public IMetricService
{
    std::mutex                                   service_mux;
    std::shared_ptr<IEpicsMetric>                epics_metric;
    std::shared_ptr<ICMDControllerMetric>        cmd_controller_metric;
    std::shared_ptr<INodeControllerMetric>       node_controller_metric;
    std::shared_ptr<INodeControllerSystemMetric> node_controller_system_metric;
    std::shared_ptr<IStorageNodeMetric>          storage_node_metric;

public:
    DummyMetricService(ConstMetricConfigurationShrdPtr metric_configuration);
    virtual ~DummyMetricService();
    DummyMetricService(const DummyMetricService&) = delete;
    DummyMetricService& operator=(const DummyMetricService&) = delete;

    IEpicsMetric&                getEpicsMetric() override final;
    ICMDControllerMetric&        getCMDControllerMetric() override final;
    INodeControllerMetric&       getNodeControllerMetric() override final;
    INodeControllerSystemMetric& getNodeControllerSystemMetric() override final;
    IStorageNodeMetric&          getStorageNodeMetric() override final;
};
} // namespace k2eg::service::metric::impl

#endif // K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_
