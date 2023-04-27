#ifndef K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_
#define K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_

#include <k2eg/common/types.h>
#include <k2eg/service/metric/IMetricService.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include <memory>
#include <mutex>
#include <string>

namespace k2eg::service::metric::impl {

class DummyMetricService;

// low level api provider for metric based on prometheus
class DummyEpicsMetric : public IEpicsMetric {
  friend class DummyMetricService;
  DummyEpicsMetric();

 public:
  virtual ~DummyEpicsMetric() = default;
  void incrementCounter(IEpicsMetricCounterType type, double inc_value = 1.0) override final;
};

// Metric services implementation
class DummyMetricService : public IMetricService {
  std::mutex                        service_mux;
  std::shared_ptr<DummyEpicsMetric> epics_metric;

 public:
  DummyMetricService(ConstMetricConfigurationUPtr metric_configuration);
  virtual ~DummyMetricService();
  DummyMetricService(const DummyMetricService&)            = delete;
  DummyMetricService& operator=(const DummyMetricService&) = delete;

  IEpicsMetric& getEpicsMetric() override final;
};
}  // namespace k2eg::service::metric::impl

#endif  // K2EG_SERVICE_METRIC_IMPL_DUMMYMETRICSERVICE_H_