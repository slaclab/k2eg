#include "k2eg/controller/node/NodeController.h"
#include <k2eg/config.h>
#include <k2eg/k2eg.h>
#include <k2eg/version.h>

#include <k2eg/common/utility.h>

#include <k2eg/service/ServiceResolver.h>
#include <k2eg/service/configuration/configuration.h>
#include <k2eg/service/configuration/impl/consul/ConsulNodeConfiguration.h>
#include <k2eg/service/data/DataStorage.h>
#include <k2eg/service/epics/EpicsServiceManager.h>
#include <k2eg/service/log/impl/BoostLogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/metric/impl/DummyMetricService.h>
#include <k2eg/service/metric/impl/prometheus/PrometheusMetricService.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/service/pubsub/pubsub.h>
#include <k2eg/service/scheduler/Scheduler.h>
#include <k2eg/service/storage/IStorageService.h>
#include <k2eg/service/storage/StorageServiceFactory.h>

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
using namespace k2eg::service::storage;
using namespace k2eg::service::configuration;
using namespace k2eg::service::configuration::impl::consul;

using namespace k2eg::controller::node;
using namespace k2eg::controller::command;

K2EG::K2EG()
    : po(std::make_unique<k2eg::common::ProgramOptions>()), quit(false), terminated(false), running(false)
{
}

K2EG::~K2EG()
{
}

bool K2EG::setup(int argc, const char* argv[])
{
    // parse expects const char*[], so convert argv
    std::vector<const char*> cargv(argv, argv + argc);
    po->parse(argc, cargv.data());
    if (po->hasOption(HELP))
    {
        std::cout << po->getHelpDescription() << std::endl;
        return false;
    }
    if (po->hasOption(VERSION))
    {
        std::cout << getTextVersion(false) << std::endl;
        return false;
    }
    return true;
}

void K2EG::init()
{
    ServiceResolver<ILogger>::registerService(logger = std::make_shared<BoostLogger>(po->getloggerConfiguration()));
    // setup services
    logger->logMessage(getTextVersion(true));
    logger->logMessage("Start Scheduler Service");
    ServiceResolver<Scheduler>::registerService(std::make_shared<Scheduler>(po->getSchedulerConfiguration()));
    ServiceResolver<Scheduler>::resolve()->start();
    logger->logMessage("Start configuration service");
    ServiceResolver<INodeConfiguration>::registerService(std::make_shared<ConsulNodeConfiguration>(po->getConfigurationServiceConfiguration()));
    logger->logMessage("Start Metric Service");
    ServiceResolver<IMetricService>::registerService(instanceMetricService(po->getMetricConfiguration()));
    logger->logMessage("Start publisher service");
    ServiceResolver<IPublisher>::registerService(std::make_shared<RDKafkaPublisher>(po->getPublisherConfiguration()));
    logger->logMessage("Start subscriber service");
    ServiceResolver<ISubscriber>::registerService(std::make_shared<RDKafkaSubscriber>(po->getSubscriberConfiguration()));
    switch (po->getNodeType())
    {
    case NodeType::GATEWAY:
        logger->logMessage("Start Gateway Node Controller");
        logger->logMessage("Start EPICS service");
        ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>(po->getEpicsManagerConfiguration()));
        logger->logMessage("Start node controller");
        node_controller = std::make_unique<NodeController>(po->getNodeControllerConfiguration(), std::make_shared<DataStorage>(po->getStoragePath()));
        logger->logMessage("Start command controller");
        cmd_controller = std::make_unique<CMDController>(po->getCMDControllerConfiguration(), std::bind(&NodeController::submitCommand, &(*node_controller), std::placeholders::_1));
        break;
    case NodeType::STORAGE:
        logger->logMessage("Start Storage Node Controller");
        logger->logMessage("Start storage service");
        ServiceResolver<IStorageService>::registerService(StorageServiceFactory::create(po->getStorageServiceConfiguration()));
        logger->logMessage("Start node controller");
        node_controller = std::make_unique<NodeController>(po->getNodeControllerConfiguration(), std::make_shared<DataStorage>(po->getStoragePath()));
        break;
    case NodeType::FULL:
        logger->logMessage("Start Gateway + Storage Node Controller");
        logger->logMessage("Start EPICS service");
        ServiceResolver<EpicsServiceManager>::registerService(std::make_shared<EpicsServiceManager>(po->getEpicsManagerConfiguration()));
        logger->logMessage("Start storage service");
        ServiceResolver<IStorageService>::registerService(StorageServiceFactory::create(po->getStorageServiceConfiguration()));
        logger->logMessage("Start node controller");
        node_controller = std::make_unique<NodeController>(po->getNodeControllerConfiguration(), std::make_shared<DataStorage>(po->getStoragePath()));
        logger->logMessage("Start command controller");
        cmd_controller = std::make_unique<CMDController>(po->getCMDControllerConfiguration(), std::bind(&NodeController::submitCommand, &(*node_controller), std::placeholders::_1));
        break;
    default:
        throw std::runtime_error("Unknown node type in ProgramOptions configuration");
    }
    running = true;
}

void K2EG::deinit()
{
    // deallocation
    switch (po->getNodeType())
    {
    case NodeType::GATEWAY:
        logger->logMessage("Stop Gateway Node Controller");
        logger->logMessage("Stop command controller");
        cmd_controller.reset();
        logger->logMessage("Stop node controller");
        node_controller.reset();
        logger->logMessage("Stop subscriber service");
        ServiceResolver<ISubscriber>::reset();
        logger->logMessage("Stop publisher service");
        ServiceResolver<IPublisher>::reset();
        logger->logMessage("Stop EPICS service");
        ServiceResolver<EpicsServiceManager>::reset();
        break;
    case NodeType::STORAGE:
        logger->logMessage("Stop Storage Node Controller");
        logger->logMessage("Stop node controller");
        node_controller.reset();
        logger->logMessage("Stop storage service");
        ServiceResolver<IStorageService>::reset();
        break;
    case NodeType::FULL:
        logger->logMessage("Stop Gateway + Storage Controller");
        logger->logMessage("Stop command controller");
        cmd_controller.reset();
        logger->logMessage("Stop node controller");
        node_controller.reset();
        logger->logMessage("Stop subscriber service");
        ServiceResolver<ISubscriber>::reset();
        logger->logMessage("Stop publisher service");
        ServiceResolver<IPublisher>::reset();
        logger->logMessage("Stop storage service");
        ServiceResolver<IStorageService>::reset();
        logger->logMessage("Stop EPICS service");
        ServiceResolver<EpicsServiceManager>::reset();
        break;
    default:
        throw std::runtime_error("Unknown node type in ProgramOptions configuration");
    }
    logger->logMessage("Stop Metric Service");
    ServiceResolver<IMetricService>::reset();
    logger->logMessage("Stop configuration service");
    ServiceResolver<INodeConfiguration>::reset();
    logger->logMessage("Stop scheduler service");
    ServiceResolver<Scheduler>::resolve()->stop();
    ServiceResolver<Scheduler>::reset();
    logger->logMessage("Shutdown completed");
    running = false;
    terminated = true;
}

IMetricServiceShrdPtr K2EG::instanceMetricService(ConstMetricConfigurationUPtr metric_conf)
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

int K2EG::run(int argc, const char* argv[])
{
    int err = EXIT_SUCCESS;
    try
    {
        if (!setup(argc, argv))
        {
            return EXIT_SUCCESS;
        }

        init();
        {
            std::unique_lock lk(m);
            cv.wait(lk, [this]
                    {
                        return this->quit;
                    });
        }
        deinit();
    }
    catch (std::runtime_error re)
    {
        err = EXIT_FAILURE;
        if (logger)
            logger->logMessage(re.what(), LogLevel::FATAL);
    }
    catch (...)
    {
        err = EXIT_FAILURE;
        if (logger)
            logger->logMessage("Undeterminated error", LogLevel::FATAL);
    }

    return err;
}

void K2EG::stop()
{
    std::lock_guard lk(m);
    quit = true;
    cv.notify_one();
}

std::string K2EG::getTextVersion(bool long_version) const
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