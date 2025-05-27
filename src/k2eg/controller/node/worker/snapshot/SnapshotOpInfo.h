#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_

#include "k2eg/common/types.h"
#include "k2eg/service/epics/EpicsServiceManager.h"
#include <k2eg/service/epics/EpicsData.h>

#include <k2eg/controller/node/worker/CommandWorker.h>

namespace k2eg::controller::node::worker::snapshot {
/*
@brief define the snapshot operation info
@details This class is used to store the information and data about the snapshot operation
it define an interface to add data and get data that will be implemented by the
specific snapshot operation info class
*/
class SnapshotOpInfo : public WorkerAsyncOperation
{
protected:
    const epics::pvData::PVStructure::const_shared_pointer filterPVField(const epics::pvData::PVStructure::const_shared_pointer& src, const std::unordered_set<std::string>& fields_to_include);

public:
    k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd;
    std::int64_t                                                         snapshot_iteration_index = 0;
    const std::string                                                    queue_name;
    const bool                                                           is_triggered;
    bool                                                                 request_to_trigger = false;
    bool                                                                 is_running = true;
    
    SnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd);
    virtual ~SnapshotOpInfo();

    virtual bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list) = 0;
    virtual void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data) = 0;
    virtual std::vector<service::epics_impl::MonitorEventShrdPtr> getData() = 0;
    virtual bool                                                  isTimeout();
};
DEFINE_PTR_TYPES(SnapshotOpInfo)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_