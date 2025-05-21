#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTREPEATINGBACKTIMEDBUFFEREDREPEATINGOPINFO_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTREPEATINGBACKTIMEDBUFFEREDREPEATINGOPINFO_H_

#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

namespace k2eg::controller::node::worker::snapshot {

/*
@brief SnapshotRepeatingBackTimedBufferedRepeatingOpInfo is a class that holds information about a repeating snapshot
operation with a back-timed buffer.
@details
The buffer is used to store all received values during the time window it can continue to receive data laso if the
snahsot is not triggered. For example it can set to autotrigger or repeating trigger with a buffer windows of two second
and a snapshot windo of 4 seconds. So evry 4 second the snapshot trigger and publish the last 2 second of data
*/
class SnapshotRepeatingBackTimedBufferedRepeatingOpInfo : public SnapshotOpInfo
{
public:
    // Buffer to store all received values during the time window
    std::map<std::string, std::vector<k2eg::service::epics_impl::MonitorEventShrdPtr>> value_buffer;

    SnapshotRepeatingBackTimedBufferedRepeatingOpInfo(const std::int32_t back_time_ms, const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd)
        : SnapshotOpInfo(queue_name, cmd)
    {
    }

    virtual void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data) {}

    virtual std::vector<service::epics_impl::MonitorEventShrdPtr> getData()
    {
        return std::vector<service::epics_impl::MonitorEventShrdPtr>();
    }
};
DEFINE_PTR_TYPES(SnapshotRepeatingBackTimedBufferedRepeatingOpInfo)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTREPEATINGBACKTIMEDBUFFEREDREPEATINGOPINFO_H_
