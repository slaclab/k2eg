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

namespace k2eg::service::metric::impl {

class DummyMetricService;

// low level api provider for metric based on prometheus
class DummyEpicsMetric : public IEpicsMetric {
  friend class DummyMetricService;
  DummyEpicsMetric() = default;

 public:
  virtual ~DummyEpicsMetric() = default;
  void incrementCounter(IEpicsMetricCounterType type, double inc_value = 1.0) override final;
};

class DummyCMDControllerMetric : public ICMDControllerMetric {
  friend class DummyMetricService;
  DummyCMDControllerMetric() = default;

 public:
  virtual ~DummyCMDControllerMetric() = default;
  void incrementCounter(ICMDControllerMetricCounterType type, double inc_value = 1.0) override final;
};

// Metric services implementation
class DummyMetricService : public IMetricService {
  std::mutex                            service_mux;
  std::shared_ptr<IEpicsMetric>       epics_metric;
  std::shared_ptr<ICMDControllerMetric> cmd_controller_metric;

 public:
  DummyMetricService(ConstMetricConfigurationUPtr metric_configuration);
  virtual ~DummyMetricService();
  DummyMetricService(const DummyMetricService&)            = delete;
  DummyMetricService& operator=(const DummyMetricService&) = delete;

  IEpicsMetric&         getEpicsMetric() override final;
  ICMDControllerMetric& getCMDControllerMetric() override final;
};
}  // namespace k2eg::service::metric::impl

#endif  // K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_