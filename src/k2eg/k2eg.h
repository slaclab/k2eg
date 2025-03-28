#ifndef k2eg_H_
#define k2eg_H_

#include <k2eg/common/ProgramOptions.h>
#include <k2eg/controller/command/CMDCommand.h>
#include <k2eg/controller/node/NodeController.h>

#include <condition_variable>
#include <mutex>
#include <string>

namespace k2eg {
// Main class
class K2EGateway {
  bool                    quit;
  bool                    terminated;
  std::mutex              m;
  std::condition_variable cv;

  k2eg::common::ProgramOptionsUPtr             po;
  k2eg::controller::command::CMDControllerUPtr cmd_controller;
  k2eg::controller::node::NodeControllerUPtr   node_controller;

  int                                          setup(int argc, const char *argv[]);
  k2eg::service::metric::IMetricServiceShrdPtr instanceMetricService(k2eg::service::metric::ConstMetricConfigurationUPtr metric_conf);
  const std::string                            getTextVersion(bool short_version = false);

 public:
  K2EGateway();
  ~K2EGateway() = default;
  int        run(int argc, const char *argv[]);
  void       stop();
  const bool isTerminated();
  const bool isStopRequested();
};
}  // namespace k2eg

#endif  // k2eg_H_