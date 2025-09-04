#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>
#include <k2eg/controller/node/worker/snapshot/ContinuousSnapshotManager.h>
#include <k2eg/service/pubsub/IPublisher.h>
#include <k2eg/common/utility.h>

using namespace k2eg::service::log;;
using namespace k2eg::controller::node::worker::snapshot;

SnapshotOpInfo::SnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd)
    : WorkerAsyncOperation(std::chrono::milliseconds(cmd->time_window_msec)), queue_name(queue_name), cmd(cmd), is_triggered(cmd->triggered)
{
}

SnapshotOpInfo::~SnapshotOpInfo() = default;

bool SnapshotOpInfo::isTimeout(const std::chrono::steady_clock::time_point& now)
{
    if (!is_running)
    {
        // If stopped, reset trigger request and expire immediately.
        request_to_trigger = false;
        return true;
    }

    // For triggered snapshots, timeout occurs if a trigger is requested or the snapshot is stopped.
    if (is_triggered)
    {
        if (request_to_trigger)
        {
            // If triggered, reset and expire.
            request_to_trigger = false;
            return true;
        }
        // Not triggered and not stopped do not expire.
        return false;
    }
    return WorkerAsyncOperation::isTimeout(now);
}

// prepareNextSubmissionChain removed: Tail ordering handled by per-iteration drain.

void SnapshotOpInfo::beginHeaderGate(int64_t iteration_id)
{
    std::shared_ptr<IterationSyncState> state;
    {
        std::lock_guard<std::mutex> lk(iteration_sync_mutex);
        auto& slot = iteration_sync_states[iteration_id];
        if (!slot)
            slot = std::make_shared<IterationSyncState>();
        state = slot;
    }
    state->header_promise = std::make_shared<std::promise<void>>();
    state->header_future = state->header_promise->get_future().share();
}

void SnapshotOpInfo::completeHeaderGate(int64_t iteration_id)
{
    std::shared_ptr<std::promise<void>> p;
    {
        std::lock_guard<std::mutex> lk(iteration_sync_mutex);
        auto it = iteration_sync_states.find(iteration_id);
        if (it != iteration_sync_states.end() && it->second)
        {
            p = it->second->header_promise;
        }
    }
    if (p)
    {
        try
        {
            p->set_value();
        }
        catch (...)
        {
        }
    }
}

void SnapshotOpInfo::waitForHeaderGate(int64_t iteration_id)
{
    std::shared_future<void> f;
    {
        std::lock_guard<std::mutex> lk(iteration_sync_mutex);
        auto it = iteration_sync_states.find(iteration_id);
        if (it != iteration_sync_states.end() && it->second)
        {
            f = it->second->header_future;
        }
    }
    if (f.valid())
    {
        try
        {
            f.wait();
        }
        catch (...)
        {
        }
    }
}

void SnapshotOpInfo::dataScheduled(int64_t iteration_id)
{
    std::shared_ptr<IterationSyncState> state;
    {
        std::lock_guard<std::mutex> lk(iteration_sync_mutex);
        auto& slot = iteration_sync_states[iteration_id];
        if (!slot)
            slot = std::make_shared<IterationSyncState>();
        state = slot;
    }
    state->data_pending.fetch_add(1, std::memory_order_relaxed);
}

void SnapshotOpInfo::dataCompleted(int64_t iteration_id)
{
    std::shared_ptr<IterationSyncState> state;
    {
        std::lock_guard<std::mutex> lk(iteration_sync_mutex);
        auto it = iteration_sync_states.find(iteration_id);
        if (it != iteration_sync_states.end())
            state = it->second;
    }
    if (!state)
        return;
    int new_val = state->data_pending.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (new_val <= 0)
    {
        std::lock_guard<std::mutex> lk(state->data_mutex);
        state->data_cv.notify_all();
    }
}

void SnapshotOpInfo::waitDataDrained(int64_t iteration_id)
{
    std::shared_ptr<IterationSyncState> state;
    {
        std::lock_guard<std::mutex> lk(iteration_sync_mutex);
        auto it = iteration_sync_states.find(iteration_id);
        if (it != iteration_sync_states.end())
            state = it->second;
    }
    if (!state)
        return;

    std::unique_lock<std::mutex> lk(state->data_mutex);
    state->data_cv.wait(lk, [&] { return state->data_pending.load(std::memory_order_acquire) == 0; });

    // Optional cleanup: remove iteration state after drain completes.
    {
        std::lock_guard<std::mutex> g(iteration_sync_mutex);
        iteration_sync_states.erase(iteration_id);
    }
}

const epics::pvData::PVStructure::const_shared_pointer SnapshotOpInfo::filterPVField(const epics::pvData::PVStructure::const_shared_pointer& src, const std::unordered_set<std::string>& fields_to_include)
{
    using namespace epics::pvData;
    if (!src)
        return PVStructure::const_shared_pointer();

    FieldCreatePtr  fieldCreate = getFieldCreate();
    FieldBuilderPtr builder = fieldCreate->createFieldBuilder();

    // Add only the requested fields
    for (const auto& field : fields_to_include)
    {
        auto pvField = src->getSubFieldT<const PVField>(field);
        if (pvField)
        {
            builder = builder->add(field, pvField->getField());
        }
    }

    StructureConstPtr filteredStruct = builder->createStructure();
    PVStructurePtr    filteredPV = getPVDataCreate()->createPVStructure(filteredStruct);

    // Copy values
    for (const auto& field : fields_to_include)
    {
        auto srcField = src->getSubFieldT<const PVField>(field);
        auto dstField = filteredPV->getSubField(field);
        if (srcField && dstField)
        {
            dstField->copy(*srcField);
        }
    }

    return filteredPV;
}

void SnapshotOpInfo::publishHeader(const std::shared_ptr<k2eg::service::pubsub::IPublisher>& publisher,
                                   const std::shared_ptr<k2eg::service::log::ILogger>&       logger,
                                   int64_t                                                   snap_ts,
                                   int64_t                                                   iteration_id) const
{
    auto serialized_header_message = serialize(RepeatingSnaptshotHeader{0, cmd->snapshot_name, snap_ts, iteration_id}, cmd->serialization);
    if (!serialized_header_message)
    {
        logger->logMessage("Invalid serialized header message", LogLevel::ERROR);
        return;
    }
    publisher->pushMessage(MakeReplyPushableMessageUPtr(queue_name, "repeating-snapshot-events", cmd->snapshot_name, serialized_header_message),
                           {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
}

std::uint64_t SnapshotOpInfo::publishData(const std::shared_ptr<k2eg::service::pubsub::IPublisher>&          publisher,
                                          const std::shared_ptr<k2eg::service::log::ILogger>&                logger,
                                          int64_t                                                            snap_ts,
                                          int64_t                                                            iteration_id,
                                          const std::vector<k2eg::service::epics_impl::MonitorEventShrdPtr>& events) const
{
    std::uint64_t published = 0;
    for (auto& event : events)
    {
        auto serialized_message = serialize(RepeatingSnaptshotData{1, snap_ts, iteration_id, MakeChannelDataShrdPtr(event->channel_data)}, cmd->serialization);
        if (serialized_message)
        {
            publisher->pushMessage(MakeReplyPushableMessageUPtr(queue_name, "repeating-snapshot-events", cmd->snapshot_name, serialized_message),
                                   {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
            ++published;
        }
        else
        {
            logger->logMessage(STRING_FORMAT("Failing serializing snapshot %1% for PV %2%", cmd->snapshot_name % event->channel_data.pv_name),
                               k2eg::service::log::LogLevel::ERROR);
        }
    }
    return published;
}

void SnapshotOpInfo::publishTail(const std::shared_ptr<k2eg::service::pubsub::IPublisher>& publisher,
                                 const std::shared_ptr<k2eg::service::log::ILogger>&       logger,
                                 int64_t                                                   snap_ts,
                                 int64_t                                                   iteration_id) const
{
    auto serialized_completion_message = serialize(RepeatingSnaptshotCompletion{2, 0, "", cmd->snapshot_name, snap_ts, iteration_id}, cmd->serialization);
    if (!serialized_completion_message)
    {
        logger->logMessage("Invalid serialized tail message", k2eg::service::log::LogLevel::ERROR);
        return;
    }
    publisher->pushMessage(MakeReplyPushableMessageUPtr(queue_name, "repeating-snapshot-events", cmd->snapshot_name, serialized_completion_message),
                           {{"k2eg-ser-type", serialization_to_string(cmd->serialization)}});
}
