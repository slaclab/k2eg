#include <gtest/gtest.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>

#include "k2eg/service/metric/ICMDControllerMetric.h"
#include "metric.h"

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;
using namespace k2eg::service::metric::impl::prometheus_impl;


using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;
using namespace k2eg::service::metric::impl::prometheus_impl;


TEST(Metric, CMDCOntrollerMetricMetricReceivedCommand) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& cmd_ctrl_metric_ref = m_uptr->getCMDControllerMetric();
  cmd_ctrl_metric_ref.incrementCounter(ICMDControllerMetricCounterType::ReceivedCommand);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.find("k2eg_cmd_reception_management{op=\"received\"} 1"), -1);
}

TEST(Metric, CMDCOntrollerMetricMetricFaultProcessedCommand) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& cmd_ctrl_metric_ref = m_uptr->getCMDControllerMetric();
  cmd_ctrl_metric_ref.incrementCounter(ICMDControllerMetricCounterType::FaultProcessingCommand);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.find("k2eg_cmd_reception_management{op=\"fault_processed\"} 1"), -1);
}

TEST(Metric, CMDCOntrollerMetricMetricBadComandCommand) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& cmd_ctrl_metric_ref = m_uptr->getCMDControllerMetric();
  cmd_ctrl_metric_ref.incrementCounter(ICMDControllerMetricCounterType::BadCommand);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.find("k2eg_cmd_reception_management{op=\"bad_command\"} 1"), -1);
}