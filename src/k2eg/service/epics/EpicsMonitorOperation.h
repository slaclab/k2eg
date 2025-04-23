#ifndef K2EG_SERVICE_EPICS_EPICSMONITOROPERATION_H_
#define K2EG_SERVICE_EPICS_EPICSMONITOROPERATION_H_

#include <k2eg/common/types.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsGetOperation.h>
#include <k2eg/service/epics/PVStructureMerger.h>
#include <pvData.h>
#include <pva/client.h>
#include <sys/types.h>

#include <atomic>
#include <mutex>

namespace k2eg::service::epics_impl {
class MonitorOperationImpl;
class CombinedMonitorOperation;

// abstract monitor operation
class MonitorOperation
{
    friend class MonitorOperationImpl;
    friend class CombinedMonitorOperation;

    MonitorOperation() = default;

protected:
    mutable std::atomic_bool force_update = false;

public:
    virtual ~MonitorOperation() = default;
    virtual void                 poll(uint element_to_fetch = 2) const = 0;
    virtual EventReceivedShrdPtr getEventData() const = 0;
    virtual bool                 hasData() const = 0;
    virtual bool                 hasEvents() const = 0;
    virtual const std::string&   getPVName() const = 0;

    virtual void forceUpdate() const
    {
        force_update = true;
    };
};
DEFINE_PTR_TYPES(MonitorOperation)

// async monitor operation ofr a single set of field
class MonitorOperationImpl : public pvac::ClientChannel::MonitorCallback, public MonitorOperation, public pvac::ClientChannel::ConnectCallback
{
    const std::string                    field;
    const std::string                    pv_name;
    mutable pvac::Monitor                mon;
    std::shared_ptr<pvac::ClientChannel> channel;
    mutable GetOperationUPtr             get_op;
    mutable EventReceivedShrdPtr         received_event;
    mutable std::mutex                   ce_mtx;
    mutable bool                         has_data;

public:
    MonitorOperationImpl(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& field = "field()");
    virtual ~MonitorOperationImpl();
    virtual void         monitorEvent(const pvac::MonitorEvent& evt) OVERRIDE FINAL;
    virtual void         poll(uint element_to_fetch = 2) const OVERRIDE FINAL;
    virtual void         connectEvent(const pvac::ConnectEvent& evt) OVERRIDE FINAL;
    EventReceivedShrdPtr getEventData() const OVERRIDE FINAL;
    bool                 hasData() const OVERRIDE FINAL;
    bool                 hasEvents() const OVERRIDE FINAL;
    const std::string&   getPVName() const OVERRIDE FINAL;
};

DEFINE_PTR_TYPES(MonitorOperationImpl)

// combine two async monitor operation together
class CombinedMonitorOperation : public MonitorOperation
{
    MonitorOperationImplUPtr    monitor_principal_request;
    MonitorOperationImplUPtr    monitor_additional_request;
    mutable MonitorEventShrdPtr last_additional_evt_received;
    PVStructureMergerUPtr       structure_merger;
    EventReceivedShrdPtr        evt_received;
    mutable std::mutex          evt_mtx;
    mutable bool                structure_initilized;

public:
    CombinedMonitorOperation(std::shared_ptr<pvac::ClientChannel> channel, const std::string& pv_name, const std::string& principal_request, const std::string& additional_request);
    virtual ~CombinedMonitorOperation() = default;
    virtual void poll(uint element_to_fetch = 2) const OVERRIDE FINAL;
    /*
    Return alway the princiapl latest data
    merged with at least the last received
    addiotnal info
    */
    EventReceivedShrdPtr getEventData() const OVERRIDE FINAL;
    bool                 hasData() const OVERRIDE FINAL;
    bool                 hasEvents() const OVERRIDE FINAL;
    const std::string&   getPVName() const OVERRIDE FINAL;
    void                 forceUpdate() const OVERRIDE FINAL;
};
DEFINE_PTR_TYPES(CombinedMonitorOperation)

} // namespace k2eg::service::epics_impl

#endif // K2EG_SERVICE_EPICS_EPICSMONITOROPERATION_H_