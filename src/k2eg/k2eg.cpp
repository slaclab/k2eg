#include <k2eg/config.h>
#include <k2eg/k2eg.h>
#include <k2eg/version.h>

#include <k2eg/common/utility.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/configuration/configuration.h>
#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>
#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/log/impl/BoostLogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/DummyMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/pubsub.h>
#include <k2eg/service/scheduler/Scheduler.h>

#include <cstdlib>

using namespace k2eg;
using namespace k2eg::service;

using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;
using namespace k2eg::service::metric;
using namespace k2eg::service::metric::impl;
using namespace k2eg::service::metric::impl::prometheus_impl;
using namespace k2eg::service::epics_impl;
using namespace k2eg::service::data;
using namespace k2eg::service::pubsub;
using namespace k2eg::service::pubsub::impl::kafka;
using namespace k2eg::service::scheduler;
using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

using namespace k2eg::controller::node;
using namespace k2eg::controller::command;

K2EGateway::K2EGateway() : po(std::make_unique<k2eg::common::ProgramOptions>()), quit(false), terminated(false) {}

int K2EGateway::setup(int argc, const char* argv[])
{
    int                      err = 0;
    std::shared_ptr<ILogger> logger;
    std::unique_lock         lk(m);
    try
    {
        po->parse(argc, argv);
        if (po->hasOption(HELP))
        {
            std::cout << po->getHelpDescription() << std::endl;
            return err;
        }
        if (po->hasOption(VERSION))
        {
            std::cout << getTextVersion(false) << std::endl;
            return err;
        }
        // setup services

        ServiceResolver<ILogger>::registerService(logger = std::make_shared<BoostLogger>(po->getloggerConfiguration()));
        logger->logMessage(getTextVersion(true));
        logger->logMessage("Start Scheduler Service");
        ServiceResolver<Scheduler>::registerService(std::make_shared<Scheduler>(po->getSchedulerConfiguration()));
        logger->logMessage("Start configuration service");
        ServiceResolver<INodeConfiguration>::registerService(std::make_shared<ConsuleNodeConfiguration>(po->getConfigurationServiceConfiguration()));
        logger->logMessage("Start Metric Service");
        ServiceResolver<IMetricService>::registerService(instanceMetricService(po->getMetricConfiguration()));
        logger->logMessage("Start EPICS service");
        ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>(po->getEpicsManagerConfiguration()));
        logger->logMessage("Start publisher service");
        ServiceResolver<IPublisher>::registerService(std::make_shared<RDKafkaPublisher>(po->getPublisherConfiguration()));
        logger->logMessage("Start subscriber service");
        ServiceResolver<ISubscriber>::registerService(std::make_shared<RDKafkaSubscriber>(po->getSubscriberConfiguration()));
        logger->logMessage("Start node controller");
        node_controller = std::make_unique<NodeController>(po->getNodeControllerConfiguration(), std::make_shared<DataStorage>(po->getStoragePath()));
        logger->logMessage("Start command controller");
        cmd_controller = std::make_unique<CMDController>(po->getCMDControllerConfiguration(), std::bind(&NodeController::submitCommand, &(*node_controller), std::placeholders::_1));
        logger->logMessage("Start Scheduler Service");
        ServiceResolver<Scheduler>::resolve()->start();
        // wait for termination request
        cv.wait(lk,
                [this]
                {
                    return this->quit;
                });

        // deallocation
        logger->logMessage("Stop command controller");
        cmd_controller.reset();
        logger->logMessage("Stop node controller");
        node_controller.reset();
        logger->logMessage("Stop subscriber service");
        ServiceResolver<ISubscriber>::reset();
        logger->logMessage("Stop publihser service");
        ServiceResolver<IPublisher>::reset();
        logger->logMessage("Stop EPICS service");
        ServiceResolver<EpicsServiceManager>::reset();
        logger->logMessage("Stop Metric Service");
        ServiceResolver<IMetricService>::reset();
        logger->logMessage("Stop configuration service");
        ServiceResolver<INodeConfiguration>::reset();
        logger->logMessage("Stop scheduler service");
        ServiceResolver<Scheduler>::resolve()->stop();
        ServiceResolver<Scheduler>::reset();
        logger->logMessage("Shutdown completed");
        ServiceResolver<ILogger>::resolve().reset();
        terminated = true;
    }
    catch (std::runtime_error re)
    {
        err = 1;
        if (logger)
            logger->logMessage(re.what(), LogLevel::FATAL);
    }
    catch (...)
    {
        err = 1;
        if (logger)
            logger->logMessage("Undeterminated error", LogLevel::FATAL);
    }

    return err;
}

IMetricServiceShrdPtr K2EGateway::instanceMetricService(ConstMetricConfigurationUPtr metric_conf)
{
    if (metric_conf->enable)
    {
        return std::make_shared<PrometheusMetricService>(std::move(metric_conf));
    }
    else
    {
        return std::make_shared<DummyMetricService>(std::move(metric_conf));
    }
}

int K2EGateway::run(int argc, const char* argv[])
{
    if (setup(argc, argv))
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void K2EGateway::stop()
{
    std::lock_guard lk(m);
    quit = true;
    cv.notify_one();
}

const bool K2EGateway::isStopRequested()
{
    return quit;
}

const bool K2EGateway::isTerminated()
{
    return terminated;
}

const std::string K2EGateway::getTextVersion(bool long_version)
{
    return long_version ? STRING_FORMAT(R"VERSION(k2eg Epics Kafka Gateway %1%)VERSION", k2eg_VERSION)
                        : STRING_FORMAT(R"VERSION(k2eg Epics Kafka Gateway %1%
Boost                     %2%
EPICS                     %3%
LibLZ4                    %4%
Sqlite                    %5%
SqliteORM                 %6%
MsgPack                   %7%
Prometheus                %8%
)VERSION",
                                        k2eg_VERSION % k2eg_BOOST_VERSION % k2eg_EPICS_VERSION % k2eg_LIBLZ4_VERSION % k2eg_SQLITE_VERSION % k2eg_SQLITEORM_VERSION % k2eg_MSGPACK_VERSION % k2eg_PROMETHEUS_VERSION);
}