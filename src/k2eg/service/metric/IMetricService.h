#ifndef K2EG_SERVICE_METRIC_IMETRICSERVICE_H_
#define K2EG_SERVICE_METRIC_IMETRICSERVICE_H_

#include "k2eg/service/metric/INodeControllerSystemMetric.h"
#include <k2eg/common/types.h>

#include <k2eg/service/metric/IEpicsMetric.h>
#include <k2eg/service/metric/ICMDControllerMetric.h>
#include <k2eg/service/metric/INodeControllerMetric.h>

namespace k2eg::service::metric {
struct MetricConfiguration {
  bool enable;
  unsigned int tcp_port;
};
DEFINE_PTR_TYPES(MetricConfiguration)

// abstra the metric implementation
class IMetricService {
 protected:
  ConstMetricConfigurationUPtr metric_configuration;

 public:
  IMetricService(ConstMetricConfigurationUPtr metric_configuration);
  virtual ~IMetricService() = default;

  virtual IEpicsMetric& getEpicsMetric() = 0;
  virtual ICMDControllerMetric& getCMDControllerMetric() = 0;
  virtual INodeControllerMetric& getNodeControllerMetric() = 0;
  virtual INodeControllerSystemMetric& getNodeControllerSystemMetric() = 0;
};
DEFINE_PTR_TYPES(IMetricService)

}  // namespace k2eg::service::metric

#endif  // K2EG_SERVICE_METRIC_IMETRICSERVICE_H_