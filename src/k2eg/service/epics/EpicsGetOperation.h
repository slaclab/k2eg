#ifndef K2EG_SERVICE_EPICS_EPICSGETOPERATION_H_
#define K2EG_SERVICE_EPICS_EPICSGETOPERATION_H_

#include <k2eg/common/types.h>
#include <k2eg/service/epics/EpicsData.h>
#include <pva/client.h>

namespace k2eg::service::epics_impl {

class GetOperation : public pvac::ClientChannel::GetCallback, public pvac::ClientChannel::ConnectCallback {
  std::shared_ptr<pvac::ClientChannel> channel;
  const std::string                    pv_name;
  pvac::Operation                      op;
  pvac::GetEvent                       evt;
  bool                                 is_done;

 public:
  GetOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name);
  virtual ~GetOperation();
  virtual void          getDone(const pvac::GetEvent& event) OVERRIDE FINAL;
  virtual void          connectEvent(const pvac::ConnectEvent& evt) OVERRIDE FINAL;
  bool                  isDone() const;
  const pvac::GetEvent& getState() const;
  ConstChannelDataUPtr  getChannelData() const;
  bool                  hasData() const;
};

DEFINE_PTR_TYPES(GetOperation)
}  // namespace k2eg::service::epics_impl

#endif  // K2EG_SERVICE_EPICS_EPICSGETOPERATION_H_