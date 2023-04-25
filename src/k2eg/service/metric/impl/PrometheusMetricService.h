#ifndef K2EG_SERVICE_METRIC_IMPL_PROMETHEUSMETRICSERVICE_H_
#define K2EG_SERVICE_METRIC_IMPL_PROMETHEUSMETRICSERVICE_H_

#include <memory>
#include <string>

#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include <k2eg/service/metric/IMetricService.h>

namespace k2eg::service::metric::impl {


class PrometheusMetricService : public IMetricService {
    std::unique_ptr<prometheus::Exposer> exposer_uptr;
    std::shared_ptr<prometheus::Registry> registry_sptr;
    public:
    PrometheusMetricService(ConstMetricConfigurationUPtr metric_configuration);
    virtual ~PrometheusMetricService();
    PrometheusMetricService(const PrometheusMetricService&) = delete;
    PrometheusMetricService& operator=(const PrometheusMetricService&) = delete;
};
}  // namespace k2eg::service::metric::impl

#endif  // K2EG_SERVICE_METRIC_IMPL_PROMETHEUSMETRICSERVICE_H_