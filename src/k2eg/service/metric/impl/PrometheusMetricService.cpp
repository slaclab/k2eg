#include <k2eg/service/metric/impl/PrometheusMetricService.h>
#include "k2eg/service/metric/IMetricService.h"

using namespace k2eg::service::metric::impl;

PrometheusMetricService::PrometheusMetricService(ConstMetricConfigurationUPtr metric_configuration):
IMetricService(std::move(metric_configuration)){}