#ifndef k2eg_SERVICE_CONTROLLER_CMDCONTROLLER_H_
#define k2eg_SERVICE_CONTROLLER_CMDCONTROLLER_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/service/log/ILogger.h>
#include <k2eg/service/metric/IMetricService.h>
#include <k2eg/service/pubsub/ISubscriber.h>

#include <memory>
#include <string>
#include <thread>

namespace k2eg::controller::command {
// configuration
struct CMDControllerConfig {
  // the name of the message buss queue where listen for command
  const std::string topic_in;

  // max message to fetch for single call to the subscriber
  const unsigned int max_message_to_fetch = 10;

  // max message to fetch for single call to the subscriber
  const unsigned int fetch_time_out = 250;
};
DEFINE_PTR_TYPES(CMDControllerConfig)

/**
 * Manage the received command, whe an exception is thrown by this method
 * the received message on the underline message bug consumer will be not committed
 */
typedef std::function<void(cmd::ConstCommandShrdPtrVec)> CMDControllerCommandHandler;

/**
 * Receive command and dispatch to other layer
 */
class CMDController {
  CMDControllerCommandHandler                         cmd_handler;
  std::shared_ptr<k2eg::service::log::ILogger>        logger;
  std::shared_ptr<k2eg::service::pubsub::ISubscriber> subscriber;
  k2eg::service::pubsub::IPublisherShrdPtr            publisher;
  k2eg::service::metric::ICMDControllerMetric&        metric;
  std::thread                                         t_subscriber;
  bool                                                run;
  void                                                consume();
  void                                                start();
  void                                                stop();

 public:
  const ConstCMDControllerConfigUPtr configuration;
  CMDController(ConstCMDControllerConfigUPtr configuration, CMDControllerCommandHandler cmd_handler);
  CMDController()                                = delete;
  CMDController(const CMDController&)            = delete;
  CMDController& operator=(const CMDController&) = delete;
  ~CMDController();
};

DEFINE_PTR_TYPES(CMDController)
}  // namespace k2eg::controller::command

#endif  // k2eg_SERVICE_CONTROLLER_CMDCONTROLLER_H_