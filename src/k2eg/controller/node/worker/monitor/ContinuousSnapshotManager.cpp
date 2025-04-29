#include <k2eg/controller/node/worker/monitor/ContinuousSnapshotManager.h>

using namespace k2eg::controller::node::worker::monitor;
using namespace k2eg::service::epics_impl;

ContinuousSnapshotManager::ContinuousSnapshotManager(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager)
    : epics_service_manager(epics_service_manager)
{
    // initialize the local cache
    global_cache_.clear();
}

void ContinuousSnapshotManager::processCommand(std::shared_ptr<BS::light_thread_pool>  command_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command) {
    //TODO: implement processCommand
    // This function should be implemented to process the command
}

bool ContinuousSnapshotManager::isReady(){
    return true;
}

void ContinuousSnapshotManager::epicsMonitorEvent(EpicsServiceManagerHandlerParamterType event_received) {

}
void ContinuousSnapshotManager::manageSnapshot(std::shared_ptr<BS::light_thread_pool> command_pool, k2eg::controller::command::cmd::ConstCommandShrdPtr command) {

}