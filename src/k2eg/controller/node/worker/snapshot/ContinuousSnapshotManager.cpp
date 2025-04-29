#include <k2eg/controller/node/worker/snapshot/ContinuousSnapshotManager.h>

using namespace k2eg::controller::node::worker::snapshot;
using namespace k2eg::service::epics_impl;

void set_snapshot_thread_name(const std::size_t idx)
{
    const std::string name = "Snapshot " + std::to_string(idx);
    const bool        result = BS::this_thread::set_os_thread_name(name);
}

ContinuousSnapshotManager::ContinuousSnapshotManager(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager)
    : epics_service_manager(epics_service_manager), thread_pool(std::make_shared<BS::light_thread_pool>(std::thread::hardware_concurrency(), set_snapshot_thread_name))
{
    // initialize the local cache
    global_cache_.clear();
}

void ContinuousSnapshotManager::submitSnapshot(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr snapsthot_command)
{
    // TODO: implement processCommand
    //  This function should be implemented to process the command
    auto s_op_ptr = MakeRepeatingSnapshotOpInfoShrdPtr(snapsthot_command);
    thread_pool->detach_task(
        [this, s_op_ptr]()
        {
            this->processSnapshot(s_op_ptr);
        });
}

void ContinuousSnapshotManager::epicsMonitorEvent(EpicsServiceManagerHandlerParamterType event_received)
{
    // TODO fill the global cache
    //  manage the disconnections
    for (auto& event : *event_received->event_fail)
    {
        // channel_topics_map[event->channel_data.pv_name].active = false;
        // logger->logMessage(STRING_FORMAT("PV %1% is not connected", event->channel_data.pv_name), LogLevel::DEBUG);
    }

    // manage the received data
    for (auto& event : *event_received->event_data)
    {
    }
}

void ContinuousSnapshotManager::processSnapshot(RepeatingSnapshotOpInfoShrdPtr snapstho_command_info)
{

}