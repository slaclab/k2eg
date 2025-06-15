#ifndef K2EG_SERVICE_EPICS_PUTOPERATION_H_
#define K2EG_SERVICE_EPICS_PUTOPERATION_H_

#include <k2eg/common/MsgpackSerialization.h>
#include <k2eg/common/types.h>

#include <pva/client.h>

namespace k2eg::service::epics_impl {

class PutOperation : public pvac::ClientChannel::PutCallback, public pvac::ClientChannel::ConnectCallback
{
    std::shared_ptr<pvac::ClientChannel>                       channel;
    pvac::Operation                                            op;
    const std::string                                          field;
    const std::unique_ptr<k2eg::common::MsgpackObjectWithZone> value;
    std::string                                                message;
    pvac::PutEvent                                             evt;
    const epics::pvData::PVStructure::const_shared_pointer     pv_req;
    bool                                                       done;
    virtual void putBuild(const epics::pvData::StructureConstPtr& build, pvac::ClientChannel::PutCallback::Args& args) OVERRIDE FINAL;
    virtual void putDone(const pvac::PutEvent& evt) OVERRIDE FINAL;

public:
    PutOperation(std::shared_ptr<pvac::ClientChannel> channel, const epics::pvData::PVStructure::const_shared_pointer& pv_req, const std::string& field, std::unique_ptr<k2eg::common::MsgpackObjectWithZone> value);
    virtual ~PutOperation();

    const std::string     getOpName() const;
    const pvac::PutEvent& getState() const;
    const bool            isDone() const;
    virtual void          connectEvent(const pvac::ConnectEvent& evt) OVERRIDE FINAL;
};
DEFINE_PTR_TYPES(PutOperation)

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_PUTOPERATION_H_