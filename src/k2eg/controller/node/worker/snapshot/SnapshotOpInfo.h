#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_

#include <chrono>
#include <k2eg/common/types.h>

#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <condition_variable>
#include <future>
#include <k2eg/controller/node/worker/CommandWorker.h>
#include <mutex>
#include <unordered_map>

#include <atomic>

namespace k2eg::controller::node::worker::snapshot {

/**
 * @brief Define the parts of a snapshot submission.
 * @details Flags describing which logical section(s) are contained in a single
 *          submission batch produced by a snapshot op. Values can be OR'ed.
 *          - Header: marks the beginning of an iteration and carries metadata.
 *          - Data: carries one or more PV events belonging to the iteration.
 *          - Tail: marks the end of an iteration and carries completion info.
 */
enum class SnapshotSubmissionType
{
    None = 0,        /**< No content. */
    Header = 1 << 0, /**< Submit the iteration header. */
    Data = 1 << 1,   /**< Submit PV data for the iteration. */
    Tail = 1 << 2    /**< Submit the iteration completion (tail). */
};

// Bitwise operators for SnapshotSubmissionType
/**
 * @brief Combine flags.
 * @param a Left-hand flag value.
 * @param b Right-hand flag value.
 * @return Bitwise OR of the two flags.
 */
inline SnapshotSubmissionType operator|(SnapshotSubmissionType a, SnapshotSubmissionType b)
{
    return static_cast<SnapshotSubmissionType>(static_cast<int>(a) | static_cast<int>(b));
}

/**
 * @brief Intersect flags.
 * @param a Left-hand flag value.
 * @param b Right-hand flag value.
 * @return Bitwise AND of the two flags.
 */
inline SnapshotSubmissionType operator&(SnapshotSubmissionType a, SnapshotSubmissionType b)
{
    return static_cast<SnapshotSubmissionType>(static_cast<int>(a) & static_cast<int>(b));
}

/**
 * @brief In-place combine flags.
 * @param a Left-hand flag reference to update.
 * @param b Right-hand flag value.
 * @return Updated left-hand flag.
 */
inline SnapshotSubmissionType& operator|=(SnapshotSubmissionType& a, SnapshotSubmissionType b)
{
    a = a | b;
    return a;
}

// forward declaration
class SnapshotOpInfo;

/**
 * @brief Hold one snapshot submission batch.
 * @details Move-only container produced by a SnapshotOpInfo implementation
 *          when the operation window expires or is triggered. It carries
 *          the events and which sections are present (header/data/tail).
 *          Ownership of events remains shared via shared_ptr.
 */
class SnapshotSubmission
{
public:
    /** @brief Time when the submission was created (steady clock). */
    std::chrono::steady_clock::time_point snap_time;
    /** @brief Collected PV events to publish. One per PV for repeating op; buffered for back-time op. */
    std::vector<service::epics_impl::MonitorEventShrdPtr> snapshot_events;
    /** @brief Which parts of a submission are present (Header/Data/Tail). */
    SnapshotSubmissionType submission_type;
    /**
     * @brief Iteration identifier assigned by the scheduler.
     * @details Binds this submission to the logical snapshot iteration it belongs to.
     *          - For batches containing Header, the scheduler assigns a new id and sets it here.
     *          - For Data/Tail-only batches, the scheduler sets the id of the current iteration.
     *          Consumers can rely on this value for coordination without reading shared state.
     */
    int64_t iteration_id{0};

    /**
     * @brief Construct a submission with explicit iteration id.
     * @param snap_time Time of submission creation.
     * @param snapshot_events Collected events (moved in).
     * @param submission_type Flags for header/data/tail presence.
     * @param iteration_id Iteration id bound to this submission.
     */
    SnapshotSubmission(const std::chrono::steady_clock::time_point& snap_time, std::vector<service::epics_impl::MonitorEventShrdPtr>&& snapshot_events, SnapshotSubmissionType submission_type, int64_t iteration_id)
        : snap_time(snap_time), snapshot_events(std::move(snapshot_events)), submission_type(submission_type), iteration_id(iteration_id)
    {
    }

    /**
     * @brief Move-construct a submission.
     * @param other Source to move from; left in valid but unspecified state.
     */
    SnapshotSubmission(SnapshotSubmission&& other) noexcept
        : snapshot_events(std::move(other.snapshot_events)), submission_type(other.submission_type), iteration_id(other.iteration_id)
    {
    }

    /**
     * @brief Move-assign a submission.
     * @param other Source to move from.
     * @return Reference to this.
     */
    SnapshotSubmission& operator=(SnapshotSubmission&& other) noexcept
    {
        if (this != &other)
        {
            snapshot_events = std::move(other.snapshot_events);
            submission_type = other.submission_type;
            iteration_id = other.iteration_id;
        }
        return *this;
    }

    /** @brief Disable copy to enforce move-only semantics. */
    SnapshotSubmission(const SnapshotSubmission&) = delete;
    /** @brief Disable copy to enforce move-only semantics. */
    SnapshotSubmission& operator=(const SnapshotSubmission&) = delete;
};

/**
 * @brief Defines shared and unique pointer types for SnapshotSubmission.
 */
DEFINE_PTR_TYPES(SnapshotSubmission);

/**
 * @brief Per-iteration synchronization primitives used by SnapshotOpInfo.
 * @details One instance exists for each logical snapshot iteration (identified
 *          by an iteration_id). It coordinates the ordering guarantees:
 *          - Header-before-Data: Data publishers wait on `header_future` until
 *            the Header publisher calls `set_value()` on `header_promise`.
 *          - Data-before-Tail: Tail waits until all scheduled Data submissions
 *            decrement `data_pending` to zero, signaled via `data_cv`.
 *
 *          Lifecycle:
 *          - Created lazily on first use for a given iteration (beginHeaderGate
 *            or dataScheduled).
 *          - Cleared by SnapshotOpInfo after `waitDataDrained(iteration_id)` returns
 *            to avoid unbounded growth.
 *
 *          Thread-safety:
 *          - The struct itself is owned behind a shared_ptr. Access to the
 *            map that holds these instances is guarded by SnapshotOpInfo's
 *            `iteration_sync_mutex` when inserting/looking up instances.
 *          - Within the struct, `data_mutex` protects `data_cv` wait/notify
 *            operations. `data_pending` uses atomics to minimize contention.
 *          - `header_promise/header_future` are set/consumed with map access
 *            protected by `iteration_sync_mutex` to avoid races on replacement.
 */
struct IterationSyncState
{
    // Header gate
    std::shared_ptr<std::promise<void>> header_promise;
    std::shared_future<void>            header_future;
    // Data drain
    std::mutex              data_mutex;
    std::condition_variable data_cv;
    std::atomic<int>        data_pending{0};
};

/**
 * @brief Base class for snapshot operations.
 * @details Stores immutable command context and common state for repeating
 *          or buffered snapshot implementations. Provides the interface to
 *          accept EPICS events and produce submission batches. Also exposes
 *          per-snapshot coordination primitives to guarantee publish order:
 *          - Header gate: Data waits until Header is published per iteration.
 *          - Data drain: Tail waits until all Data of the iteration is done.
 *          Thread-safety: addData(), getData(), and the coordination helpers
 *          are intended to be called from multiple threads.
 */
class SnapshotOpInfo : public WorkerAsyncOperation
{
    /** @brief Protect access to per-iteration synchronization map. */
    std::mutex iteration_sync_mutex;
    /** @brief Map of iteration_id to its synchronization state. */
    std::unordered_map<int64_t, std::shared_ptr<IterationSyncState>> iteration_sync_states;

protected:
    /**
     * @brief Filter PVStructure fields to a subset.
     * @param src Source PVStructure; may be null.
     * @param fields_to_include Fields to copy into the filtered structure.
     * @return New PVStructure with only requested fields; null if src is null.
     */
    const epics::pvData::PVStructure::const_shared_pointer filterPVField(const epics::pvData::PVStructure::const_shared_pointer& src, const std::unordered_set<std::string>& fields_to_include);

public:
    /**
     * @brief Promise fulfilled when the snapshot is fully removed.
     * @details Used by the manager to await clean teardown when stopping.
     */
    std::promise<void> removal_promise;

    /** @brief Command parameters associated with this operation (non-owning shared_ptr). */
    k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd;


    /** @brief Normalized queue name where events are published. */
    const std::string queue_name;

    /** @brief True if operation is trigger-driven instead of periodic. */
    const bool is_triggered;

    /** @brief Asynchronously request a trigger for the next window (triggered mode). */
    bool request_to_trigger = false;

    /** @brief True while the snapshot operation is active; false when stopping. */
    bool is_running = true;

    /**
     * @brief Construct a snapshot operation.
     * @param queue_name Normalized queue name.
     * @param cmd Repeating snapshot command parameters.
     */
    SnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd);

    /** @brief Destroy the snapshot operation. */
    virtual ~SnapshotOpInfo();

    /**
     * @brief Initialize with sanitized PV list.
     * @param sanitized_pv_name_list List of PV identifiers already sanitized.
     * @return True on success; false if initialization fails.
     */
    virtual bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list) = 0;

    /**
     * @brief Add a monitor event into the current window.
     * @param event_data Event shared_ptr; ownership is not taken.
     */
    virtual void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data) = 0;

    /**
     * @brief Produce a submission batch from the current window.
     * @return Submission object with header/data/tail flags and events.
     */
    virtual SnapshotSubmissionShrdPtr getData() = 0;

    /**
     * @brief Check whether the current window expired.
     * @param now Optional reference time; defaults to steady_clock::now().
     * @return True if a submission should be produced.
     */
    virtual bool isTimeout(const std::chrono::steady_clock::time_point& now = std::chrono::steady_clock::now()) override;

    // Submission chaining removed: Tail now waits on per-iteration data drain.

    /**
     * @brief Begin a new header gate for a specific iteration.
     * @details Creates (or resets) the IterationSyncState for `iteration_id` and
     *          initializes its header promise/future. Call exactly once per iteration
     *          when scheduling a submission that includes a Header, prior to any
     *          Data scheduling for the same iteration.
     * @param iteration_id Iteration identifier bound to this header gate.
     */
    void beginHeaderGate(int64_t iteration_id);

    /**
     * @brief Complete the header gate for a specific iteration.
     * @details Signals the header promise inside IterationSyncState so any Data publishers
     *          waiting on `waitForHeaderGate(iteration_id)` can proceed. Call right after
     *          the Header message for `iteration_id` is published.
     * @param iteration_id Iteration identifier whose header gate to release.
     */
    void completeHeaderGate(int64_t iteration_id);

    /**
     * @brief Wait until the Header for a specific iteration has been published.
     * @details Looks up the IterationSyncState for `iteration_id` and blocks on its header
     *          future. Use inside Data publishing path to guarantee Header-before-Data order.
     *          If no state exists (should not happen when scheduled correctly), returns immediately.
     * @param iteration_id Iteration identifier to wait on.
     */
    void waitForHeaderGate(int64_t iteration_id);

    /**
     * @brief Increment pending data submissions counter for a specific iteration.
     * @details Lazily creates IterationSyncState if missing and increments `data_pending`.
     *          Call exactly once per scheduled Data submission batch for `iteration_id`,
     *          before the associated task begins publishing.
     * @param iteration_id Iteration identifier whose counter to increment.
     */
    void dataScheduled(int64_t iteration_id);

    /**
     * @brief Decrement pending data submissions counter and notify waiters for a specific iteration.
     * @details Decrements `data_pending` and, when it reaches zero, notifies `data_cv` inside the
     *          IterationSyncState. Call at the end of the Data publishing task for `iteration_id`.
     * @param iteration_id Iteration identifier whose counter to decrement.
     */
    void dataCompleted(int64_t iteration_id);

    /**
     * @brief Block until all scheduled data submissions are completed for a specific iteration.
     * @details Waits on the `data_cv` of IterationSyncState for `iteration_id` until `data_pending`
     *          is zero, ensuring Tail publishes after all Data. After the wait completes, the
     *          per-iteration state is cleaned up to avoid leaks.
     * @param iteration_id Iteration identifier to wait on.
     */
    void waitDataDrained(int64_t iteration_id);
};

/**
 * @brief Defines shared and unique pointer types for SnapshotOpInfo.
 */
DEFINE_PTR_TYPES(SnapshotOpInfo)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_
