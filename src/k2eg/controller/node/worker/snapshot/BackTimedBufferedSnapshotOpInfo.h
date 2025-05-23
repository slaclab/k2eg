#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTREPEATINGBACKTIMEDBUFFEREDREPEATINGOPINFO_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTREPEATINGBACKTIMEDBUFFEREDREPEATINGOPINFO_H_

#include <k2eg/common/TimeWindowEventBuffer.h>
#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

namespace k2eg::controller::node::worker::snapshot {

/*
@brief BackTimedBufferedSnapshotOpInfo is a class that holds information about a
repeating snapshot operation with a back-timed buffer.
@details
The buffer is used to store all received values during the time window it can
continue to receive data laso if the snahsot is not triggered. For example it
can set to autotrigger or repeating trigger with a buffer windows of two second
and a snapshot windo of 4 seconds. So evry 4 second the snapshot trigger and
publish the last 2 second of data
*/
class BackTimedBufferedSnapshotOpInfo : public SnapshotOpInfo
{
    // define when the snapshot is acquiring data
    std::mutex                                                                   buffer_mutex;
    std::atomic<bool>                                                            taking_data;
    k2eg::common::TimeWindowEventBuffer<k2eg::service::epics_impl::MonitorEvent> buffer;

public:
    // Buffer to store all received values during the time window
    std::map<std::string, std::vector<k2eg::service::epics_impl::MonitorEventShrdPtr>> value_buffer;

    BackTimedBufferedSnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd);
    ~BackTimedBufferedSnapshotOpInfo() = default;
    bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list) override;
    void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data) override;
    std::vector<service::epics_impl::MonitorEventShrdPtr> getData() override;
};
DEFINE_PTR_TYPES(BackTimedBufferedSnapshotOpInfo)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTREPEATINGBACKTIMEDBUFFEREDREPEATINGOPINFO_H_
