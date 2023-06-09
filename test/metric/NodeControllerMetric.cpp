#include <gtest/gtest.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>

#include "k2eg/service/metric/INodeControllerMetric.h"
#include "metric.h"

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;
using namespace k2eg::service::metric::impl::prometheus_impl;


using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;
using namespace k2eg::service::metric::impl::prometheus_impl;


TEST(Metric, NodeControllerMetricSubmittedCommand) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& cmd_ctrl_metric_ref = m_uptr->getNodeControllerMetric();
  cmd_ctrl_metric_ref.incrementCounter(INodeControllerMetricCounterType::SubmittedCommand);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.find("k2eg_node_controller{op=\"command_submitted\"} 1"), -1);
}