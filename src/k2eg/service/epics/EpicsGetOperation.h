#ifndef K2EG_SERVICE_EPICS_EPICSGETOPERATION_H_
#define K2EG_SERVICE_EPICS_EPICSGETOPERATION_H_

#include <k2eg/common/types.h>

#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/PVStructureMerger.h>

#include <pva/client.h>
#include <vector>

namespace k2eg::service::epics_impl {

// abstract get operation
class GetOperation
{
public:
    GetOperation() = default;
    virtual ~GetOperation() = default;
    virtual bool                    isDone() const = 0;
    virtual const pvac::GetEvent&   getState() const = 0;
    virtual ConstChannelDataUPtr    getChannelData() const = 0;
    virtual bool                    hasData() const = 0;
};
DEFINE_PTR_TYPES(GetOperation)

// combined get operation
class CombinedGetOperation : public GetOperation
{
    GetOperationShrdPtr   get_op_a;
    GetOperationShrdPtr   get_op_b;
    PVStructureMergerUPtr structure_merger;

public:
    CombinedGetOperation(GetOperationShrdPtr get_op_a, GetOperationShrdPtr get_op_b);
    virtual ~CombinedGetOperation() = default;
    bool                  isDone() const OVERRIDE FINAL;
    const pvac::GetEvent& getState() const OVERRIDE FINAL;
    ConstChannelDataUPtr  getChannelData() const OVERRIDE FINAL;
    bool                  hasData() const OVERRIDE FINAL;
};
DEFINE_PTR_TYPES(CombinedGetOperation)

// Get operation
class SingleGetOperation : public GetOperation, public pvac::ClientChannel::GetCallback, public pvac::ClientChannel::ConnectCallback
{
    friend class CombinedGetOperation;
    std::shared_ptr<pvac::ClientChannel> channel;
    std::vector<std::string>             requested_fields;
    const std::string                    pv_name;
    const std::string                    field;
    pvac::Operation                      op;
    pvac::GetEvent                       evt;
    bool                                 is_done;

public:
    SingleGetOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field = "field()");
    virtual ~SingleGetOperation();
    virtual void          getDone(const pvac::GetEvent& event) OVERRIDE FINAL;
    virtual void          connectEvent(const pvac::ConnectEvent& evt) OVERRIDE FINAL;
    bool                  isDone() const OVERRIDE FINAL;
    const pvac::GetEvent& getState() const OVERRIDE FINAL;
    ConstChannelDataUPtr  getChannelData() const OVERRIDE FINAL;
    bool                  hasData() const OVERRIDE FINAL;
};

DEFINE_PTR_TYPES(SingleGetOperation)
} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_EPICSGETOPERATION_H_