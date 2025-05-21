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
public:
    k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd;
    std::int64_t                                                         snapshot_iteration_index = 0;
    const std::string                                                    queue_name;
    const bool                                                           is_triggered;
    bool                                                                 request_to_trigger = false;
    bool                                                                 is_running = true;

    SnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd)
        : WorkerAsyncOperation(std::chrono::milliseconds(cmd->time_window_msec)), queue_name(queue_name), cmd(cmd), is_triggered(cmd->triggered)
    {
    }

    virtual bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list);
    virtual void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data);
    virtual std::vector<service::epics_impl::MonitorEventShrdPtr> getData();
    bool isTimeout()
    {
                // For triggered snapshots, timeout occurs if a trigger is requested or the snapshot is stopped.
        if (is_triggered)
        {
            if (!is_running)
            {
                // If stopped, reset trigger request and expire immediately.
                request_to_trigger = false;
                return true;
            }
            if (request_to_trigger)
            {
                // If triggered, reset and expire.
                request_to_trigger = false;
                return true;
            }
            // Not triggered and not stopped: do not expire.
            return false;
        }
        return WorkerAsyncOperation::isTimeout();
    }
};
DEFINE_PTR_TYPES(SnapshotOpInfo)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_