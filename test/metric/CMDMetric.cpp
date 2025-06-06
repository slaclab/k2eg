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

TEST(Metric, CMDControllerMetricReceivedCommand)
{
    IMetricServiceUPtr           m_uptr;
    unsigned int                 port = 18080 + (rand() % 1000);
    ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port = port});
    EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
    auto& cmd_ctrl_metric_ref = m_uptr->getCMDControllerMetric();
    cmd_ctrl_metric_ref.incrementCounter(ICMDControllerMetricCounterType::ReceivedCommand);
    auto metrics_string = getUrl(METRIC_URL_FROM_PORT(port));
    ASSERT_NE(metrics_string.find("k2eg_command_controller{op=\"received\"} 1"), -1);
}

TEST(Metric, CMDControllerMetricFaultProcessedCommand)
{
    IMetricServiceUPtr           m_uptr;
    unsigned int                 port = 18080 + (rand() % 1000);
    ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port = port});
    EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
    auto& cmd_ctrl_metric_ref = m_uptr->getCMDControllerMetric();
    cmd_ctrl_metric_ref.incrementCounter(ICMDControllerMetricCounterType::FaultProcessingCommand);
    auto metrics_string = getUrl(METRIC_URL_FROM_PORT(port));
    ASSERT_NE(metrics_string.find("k2eg_command_controller{op=\"fault_processed\"} 1"), -1);
}

TEST(Metric, CMDControllerMetriBadComandCommand)
{
    IMetricServiceUPtr           m_uptr;
    unsigned int                 port = 18080 + (rand() % 1000);
    ConstMetricConfigurationUPtr m_conf = MakeMetricConfigurationUPtr(MetricConfiguration{.tcp_port = port});
    EXPECT_NO_THROW(m_uptr = std::make_unique<PrometheusMetricService>(std::move(m_conf)));
    auto& cmd_ctrl_metric_ref = m_uptr->getCMDControllerMetric();
    cmd_ctrl_metric_ref.incrementCounter(ICMDControllerMetricCounterType::BadCommand);
    auto metrics_string = getUrl(METRIC_URL_FROM_PORT(port));
    ASSERT_NE(metrics_string.find("k2eg_command_controller{op=\"bad_command\"} 1"), -1);
}