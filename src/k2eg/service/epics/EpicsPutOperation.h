#ifndef K2EG_SERVICE_EPICS_PUTOPERATION_H_
#define K2EG_SERVICE_EPICS_PUTOPERATION_H_

#include <k2eg/common/types.h>
#include <pva/client.h>

namespace k2eg::service::epics_impl {

class PutOperation : public pvac::ClientChannel::PutCallback {
  std::shared_ptr<pvac::ClientChannel> channel;
  pvac::Operation                      op;
  const std::string                    field;
  const std::string                    value;
  std::string                          message;
  pvac::PutEvent                       evt;
  bool                                 done;
  virtual void                         putBuild(const epics::pvData::StructureConstPtr& build, pvac::ClientChannel::PutCallback::Args& args) OVERRIDE FINAL;
  virtual void                         putDone(const pvac::PutEvent& evt) OVERRIDE FINAL;

 public:
  PutOperation(std::shared_ptr<pvac::ClientChannel> channel, const epics::pvData::PVStructure::const_shared_pointer& pvReq, const std::string& field, const std::string& value);
  virtual ~PutOperation();

  const std::string     getOpName() const;
  const pvac::PutEvent& getState() const;
  const bool            isDone() const;
};
DEFINE_PTR_TYPES(PutOperation)

}  // namespace k2eg::service::epics_impl

#endif  // K2EG_SERVICE_EPICS_PUTOPERATION_H_