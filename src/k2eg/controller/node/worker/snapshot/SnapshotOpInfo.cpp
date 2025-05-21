#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

using namespace k2eg::controller::node::worker::snapshot;

SnapshotOpInfo::SnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd)
    : WorkerAsyncOperation(std::chrono::milliseconds(cmd->time_window_msec)), queue_name(queue_name), cmd(cmd), is_triggered(cmd->triggered)
{
}

SnapshotOpInfo::~SnapshotOpInfo() = default;

bool SnapshotOpInfo::isTimeout()
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