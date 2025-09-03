#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

using namespace k2eg::controller::node::worker::snapshot;

#pragma region SnapshotOpInfo

SnapshotStatisticCounter::SnapshotStatisticCounter()
    : start_sampling_time(std::chrono::steady_clock::now()) {}

void SnapshotStatisticCounter::incrementEventSize(double amount)
{
    statistic.event_size.fetch_add(amount, std::memory_order_relaxed);
}

void SnapshotStatisticCounter::incrementEventCount(double count)
{
    statistic.event_count.fetch_add(count, std::memory_order_relaxed);
}

SnapshotStatisticShrdPtr SnapshotStatisticCounter::getStatistics(const std::chrono::steady_clock::time_point& now) const
{
    SnapshotStatisticShrdPtr result = std::make_shared<SnapshotStatistic>();
    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - start_sampling_time).count();
    if (elapsed > 0.0)
    {
        result->event_size = statistic.event_size / elapsed;
        result->event_count = statistic.event_count / elapsed;
    }
    return result;
}

void SnapshotStatisticCounter::reset()
{
    statistic.event_count = 0;
    statistic.event_size = 0;
    start_sampling_time = std::chrono::steady_clock::now();
}

#pragma region SnapshotSubmission
SnapshotSubmission::SnapshotSubmission(const std::chrono::steady_clock::time_point& snap_time, const std::chrono::steady_clock::time_point& header_timestamp, std::vector<service::epics_impl::MonitorEventShrdPtr>&& snapshot_events, SnapshotSubmissionType submission_type)
        : snap_time(snap_time), header_timestamp(header_timestamp), snapshot_events(std::move(snapshot_events)), submission_type(submission_type)
    {
    }

    /**
     * @brief Move constructor.
     * @param other SnapshotSubmission to move from.
     */
    SnapshotSubmission::SnapshotSubmission(SnapshotSubmission&& other) noexcept
        : snapshot_events(std::move(other.snapshot_events)), submission_type(other.submission_type)
    {
    }

    /**
     * @brief Move assignment operator.
     * @param other SnapshotSubmission to move from.
     * @return Reference to this object.
     */
    SnapshotSubmission& SnapshotSubmission::operator=(SnapshotSubmission&& other) noexcept
    {
        if (this != &other)
        {
            snapshot_events = std::move(other.snapshot_events);
            submission_type = other.submission_type;
        }
        return *this;
    }

#pragma region SnapshotOpInfo

SnapshotOpInfo::SnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd)
    : WorkerAsyncOperation(std::chrono::milliseconds(cmd->time_window_msec)), snapshot_statistic(MakeSnapshotStatisticCounterShrdPtr()), queue_name(queue_name), cmd(cmd), is_triggered(cmd->triggered)
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
