#include <gtest/gtest.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <chrono>
#include <cstddef>
#include <thread>

#include "metric.h"

using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;
using namespace k2eg::service::metric::impl::prometheus_impl;


TEST(Metric, MetricService) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
}

TEST(Metric, EpicsMetricGet) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& e_metric_ref = m_uptr->getEpicsMetric();
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::Get);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.find("epics_ioc_operation{op=\"get\"} 1"), -1);
}

TEST(Metric, EpicsMetricPut) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& e_metric_ref = m_uptr->getEpicsMetric();
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::Put);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.length(), 0);
  ASSERT_NE(metrics_string.find("epics_ioc_operation{op=\"put\"} 1"), -1);
}

TEST(Metric, EpicsMetricMonitorData) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& e_metric_ref = m_uptr->getEpicsMetric();
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::MonitorData);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.length(), 0);
  ASSERT_NE(metrics_string.find("epics_ioc_operation{evt_type=\"data\",op=\"monitor\"} 1"), -1);
}

TEST(Metric, EpicsMetricMonitorCancel) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& e_metric_ref = m_uptr->getEpicsMetric();
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::MonitorCancel);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.length(), 0);
  ASSERT_NE(metrics_string.find("epics_ioc_operation{evt_type=\"cancel\",op=\"monitor\"} 1"), -1);
}

TEST(Metric, EpicsMetricMonitorFail) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& e_metric_ref = m_uptr->getEpicsMetric();
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::MonitorFail);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.length(), 0);
  ASSERT_NE(metrics_string.find("epics_ioc_operation{evt_type=\"fail\",op=\"monitor\"} 1"), -1);
}

TEST(Metric, EpicsMetricMonitorTimeout) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& e_metric_ref = m_uptr->getEpicsMetric();
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::MonitorDisconnect);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.length(), 0);
  ASSERT_NE(metrics_string.find("epics_ioc_operation{evt_type=\"disconnect\",op=\"monitor\"} 1"), -1);
}

TEST(Metric, EpicsMetricMonitorRates) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& e_metric_ref = m_uptr->getEpicsMetric();
  for(int idx = 0; idx < 1000; idx++) {
    e_metric_ref.incrementCounter(IEpicsMetricCounterType::MonitorData);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.length(), 0);
  ASSERT_NE(metrics_string.find("epics_ioc_operation_rate{evt_type=\"data\",op=\"monitor\"}"), -1);
}

TEST(Metric, EpicsMetricMonitorCount) {
  IMetricServiceUPtr m_uptr;
  ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port=8080});
  EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
  auto& e_metric_ref = m_uptr->getEpicsMetric();
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::TotalMonitor, 10);
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::ActiveMonitor, 5);
  auto metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.length(), 0);
  ASSERT_NE(metrics_string.find("k2eg_epics_ioc_pv_count{type=\"total\"} 10"), -1);
  ASSERT_NE(metrics_string.find("k2eg_epics_ioc_pv_count{type=\"active\"} 5"), -1);
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::TotalMonitor, 5);
  e_metric_ref.incrementCounter(IEpicsMetricCounterType::ActiveMonitor, 4);
  metrics_string = getUrl("http://localhost:8080/metrics");
  ASSERT_NE(metrics_string.length(), 0);
  ASSERT_NE(metrics_string.find("k2eg_epics_ioc_pv_count{type=\"total\"} 5"), -1);
  ASSERT_NE(metrics_string.find("k2eg_epics_ioc_pv_count{type=\"active\"} 4"), -1);
}