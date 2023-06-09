#include <k2eg/service/metric/IMetricService.h>

#include <memory>

using namespace k2eg::service::metric;

IMetricService::IMetricService(ConstMetricConfigurationUPtr metric_configuration) : metric_configuration(std::move(metric_configuration)) {}