
#include <k2eg/common/utility.h>

#include <k2eg/service/ServiceResolver.h>

#include <k2eg/controller/node/worker/snapshot/ContinuousSnapshotManager.h>
#include <memory>

using namespace k2eg::controller::node::worker::snapshot;

using namespace k2eg::service;
using namespace k2eg::service::log;
using namespace k2eg::service::epics_impl;

void set_snapshot_thread_name(const std::size_t idx)
{
    const std::string name = "Snapshot " + std::to_string(idx);
    const bool        result = BS::this_thread::set_os_thread_name(name);
}

ContinuousSnapshotManager::ContinuousSnapshotManager(k2eg::service::epics_impl::EpicsServiceManagerShrdPtr epics_service_manager)
    : logger(ServiceResolver<ILogger>::resolve())
    , epics_service_manager(epics_service_manager)
    , thread_pool(std::make_shared<BS::light_thread_pool>(std::thread::hardware_concurrency(), set_snapshot_thread_name))
{
    // initialize the local cache
    global_cache_.clear();
}

void ContinuousSnapshotManager::submitSnapshot(k2eg::controller::command::cmd::ConstSnapshotCommandShrdPtr snapsthot_command)
{
    // TODO: implement processCommand
    logger->logMessage(STRING_FORMAT("Perepare continuous snapshot ops for '%1%' on topic %2% with sertype: %3%",
                                 snapsthot_command->snapshot_name % snapsthot_command->reply_topic % serialization_to_string(snapsthot_command->serialization)),
                     LogLevel::DEBUG);
    //  This function should be implemented to process the command
    auto s_op_ptr = MakeRepeatingSnapshotOpInfoShrdPtr(snapsthot_command);

    // prepare global cache with all the needed key
    std::shared_lock read_lock(global_cache_mutex_);

    // get the pv to add to the cache
    for (const auto& pv_uri : s_op_ptr->cmd->pv_name_list)
    {
        auto it = global_cache_.find(pv_uri);

        auto mon_op = epics_service_manager->getMonitorOp(pv_uri);
        if (it == global_cache_.end()) {
            read_lock.unlock();                        // drop shared
            std::unique_lock write(global_cache_mutex_); // take exclusive
            // double-check in case of race
            it = global_cache_.try_emplace(pv_uri).first;
            // (optionally) rebuild your snapshot_views_ under this lock
        } 
    }

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