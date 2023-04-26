#ifndef K2EG_SERVICE_METRIC_IMETRICSERVICE_H_
#define K2EG_SERVICE_METRIC_IMETRICSERVICE_H_

#include <k2eg/common/types.h>

#include <memory>
#include <mutex>
namespace k2eg::service::metric {
struct MetricConfiguration {
  unsigned int tcp_port;
};
DEFINE_PTR_TYPES(MetricConfiguration)

class IEpicsMetric {
friend class IMetricService;
public:
  IEpicsMetric()  = default;
  virtual ~IEpicsMetric() = default;
};

// abstra the metric implementation
class IMetricService {
 protected:
  ConstMetricConfigurationUPtr metric_configuration;

 public:
  IMetricService(ConstMetricConfigurationUPtr metric_configuration);
  virtual ~IMetricService() = default;

  virtual IEpicsMetric *getEpicsMetric() = 0;
};
}  // namespace k2eg::service::metric

#endif  // K2EG_SERVICE_METRIC_IMETRICSERVICE_H_