#include <k2eg/controller/node/worker/snapshot/BackTimedBufferedSnapshotOpInfo.h>

using namespace k2eg::controller::node::worker::snapshot;
using namespace k2eg::controller::command::cmd;

BackTimedBufferedSnapshotOpInfo::BackTimedBufferedSnapshotOpInfo(
    const std::int32_t back_time_ms, 
    const std::string& queue_name, 
    ConstRepeatingSnapshotCommandShrdPtr cmd)
    : SnapshotOpInfo(queue_name, cmd)
{
}