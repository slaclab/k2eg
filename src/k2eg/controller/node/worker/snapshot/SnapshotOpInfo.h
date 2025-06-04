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
    // Filter PVStructure fields, returning only those in fields_to_include
    const epics::pvData::PVStructure::const_shared_pointer filterPVField(const epics::pvData::PVStructure::const_shared_pointer& src, const std::unordered_set<std::string>& fields_to_include);

public:
    // Pointer to the repeating snapshot command associated with this operation
    k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd;

    // Index of the current snapshot iteration
    std::int64_t snapshot_iteration_index = 0;

    // Name of the queue associated with this operation
    const std::string queue_name;

    // Indicates if the snapshot is triggered (immutable after construction)
    const bool is_triggered;

    // Flag to request a trigger for the snapshot operation
    bool request_to_trigger = false;

    // Indicates if the operation is currently running
    bool is_running = true;
    
    // Constructor: initializes with queue name and command pointer
    SnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd);

    // Destructor
    virtual ~SnapshotOpInfo();

    // Initialize operation with a list of sanitized PV names
    virtual bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list) = 0;

    // Add monitor event data to the operation
    virtual void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data) = 0;

    // Retrieve collected monitor event data
    virtual std::vector<service::epics_impl::MonitorEventShrdPtr> getData() = 0;

    // Check if the operation has timed out
    virtual bool isTimeout();
};
DEFINE_PTR_TYPES(SnapshotOpInfo)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_