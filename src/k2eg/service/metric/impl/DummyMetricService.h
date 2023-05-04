#ifndef K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_
#define K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_

#include <k2eg/common/types.h>
#include <k2eg/service/metric/IMetricService.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include <memory>
#include <mutex>
#include <string>

#include "k2eg/service/metric/ICMDControllerMetric.h"
#include "k2eg/service/metric/INodeControllerMetric.h"

namespace k2eg::service::metric::impl {

class DummyMetricService;

#define DEFINE_METRIC(type, counter_type)                                         \
  class Dummy##type : public type {                                               \
    friend class DummyMetricService;                                              \
    Dummy##type() = default;                                                      \
                                                                                  \
   public:                                                                        \
    virtual ~Dummy##type() = default;                                             \
    void                                                                          \
    incrementCounter(counter_type type, double inc_value = 1.0) override final {} \
  };

#define CONSTRUCT_METRIC(type, counter_type)                                      \
  class Dummy##type : public type {                                               \
    friend class DummyMetricService;                                              \
    Dummy##type() = default;                                                      \
                                                                                  \
   public:                                                                        \
    virtual ~Dummy##type() = default;                                             \
    void                                                                          \
    incrementCounter(counter_type type, double inc_value = 1.0) override final {} \
  };

DEFINE_METRIC(IEpicsMetric, IEpicsMetricCounterType)
DEFINE_METRIC(ICMDControllerMetric, ICMDControllerMetricCounterType)
DEFINE_METRIC(INodeControllerMetric, INodeControllerMetricCounterType)

// Dummy Metric services implementation
class DummyMetricService : public IMetricService {
  std::mutex                             service_mux;
  std::shared_ptr<IEpicsMetric>          epics_metric;
  std::shared_ptr<ICMDControllerMetric>  cmd_controller_metric;
  std::shared_ptr<INodeControllerMetric> node_controller_metric;

 public:
  DummyMetricService(ConstMetricConfigurationUPtr metric_configuration);
  virtual ~DummyMetricService();
  DummyMetricService(const DummyMetricService&)            = delete;
  DummyMetricService& operator=(const DummyMetricService&) = delete;

  IEpicsMetric&          getEpicsMetric() override final;
  ICMDControllerMetric&  getCMDControllerMetric() override final;
  INodeControllerMetric& getNodeControllerMetric() override final;
};
}  // namespace k2eg::service::metric::impl

#endif  // K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_