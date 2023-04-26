
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/PrometheusMetricService.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>

#include <algorithm>
#include <memory>

using namespace prometheus;

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;

PrometheusEpicsMetric::PrometheusEpicsMetric():registry(std::make_shared<Registry>()) {

    auto& packet_counter = BuildCounter().Name("Epics").Help("Epics description").Register(*registry);

}

PrometheusMetricService::PrometheusMetricService(ConstMetricConfigurationUPtr metric_configuration) : IMetricService(std::move(metric_configuration)) {
  std::string uri = "127.0.0.1:" + std::to_string(metric_configuration->tcp_port);
  exposer_uptr    = std::make_unique<Exposer>(uri);
}

PrometheusMetricService::~PrometheusMetricService() {}

IEpicsMetric *PrometheusMetricService::getEpicsMetric() {
     std::lock_guard<std::mutex> lk(service_mux);
     if(!epics_metric) {
        epics_metric = std::shared_ptr<PrometheusEpicsMetric>(new PrometheusEpicsMetric());
     }
     return epics_metric.get();
}