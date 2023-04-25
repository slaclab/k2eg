#ifndef K2EG_SERVICE_METRIC_IMETRICSERVICE_H_
#define K2EG_SERVICE_METRIC_IMETRICSERVICE_H_
#include <k2eg/common/types.h>
namespace k2eg::service::metric {

// logger configuration type
typedef struct MetricConfiguration {
    unsigned int tcp_port;

} MetricConfiguration;
DEFINE_PTR_TYPES(MetricConfiguration)

class IMetricService {
    ConstMetricConfigurationUPtr metric_configuration;
 public:
    IMetricService(ConstMetricConfigurationUPtr metric_configuration);
    virtual ~IMetricService() = default;
};
}  // namespace k2eg::service::metric

#endif  // K2EG_SERVICE_METRIC_IMETRICSERVICE_H_