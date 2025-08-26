#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_

#include <chrono>
#include <k2eg/common/types.h>

#include <k2eg/service/configuration/INodeConfiguration.h>
#include <k2eg/service/epics/EpicsData.h>
#include <k2eg/service/epics/EpicsServiceManager.h>

#include <k2eg/controller/node/worker/CommandWorker.h>

#include <atomic>

namespace k2eg::controller::node::worker::snapshot {

struct SnapshotStatistic
{
    std::atomic<double> event_size = 0;  /**< Total size of events in bytes. */
    std::atomic<double> event_count = 0; /**< Number of events processed. */
};

DEFINE_PTR_TYPES(SnapshotStatistic);

/**
 * @struct Statistic
 * @brief Stores per-second statistics for snapshot operations.
 *
 * Contains the total event size and count for a given time window.
 */

class SnapshotStatisticCounter

{
private:
    SnapshotStatistic                     statistic;
    std::chrono::steady_clock::time_point start_sampling_time;

public:
    SnapshotStatisticCounter();

    /**
     * @brief Increment the total event size.
     * @param amount Number of bytes to add.
     */
    void incrementEventSize(double amount = 1);

    /**
     * @brief Increment the event count.
     * @param count Number of events to add.
     */
    void incrementEventCount(double count = 1);

    /**
     * @brief Get the current statistics.
     * @param now Current time point (default: now).
     * @return Snapshot statistics.
     */
    SnapshotStatisticShrdPtr getStatistics(const std::chrono::steady_clock::time_point& now = std::chrono::steady_clock::now()) const;

    /**
     * @brief Reset statistics and sampling times.
     */
    void reset();
};

DEFINE_PTR_TYPES(SnapshotStatisticCounter);

/**
 * @enum SnapshotSubmissionType
 * @brief Flags for different stages of snapshot submission.
 *
 * - None: No submission.
 * - Header: Submission includes header information.
 * - Data: Submission includes data payload.
 * - Tail: Submission includes tail/finalization.
 *
 * Supports bitwise operations for combining flags.
 */
enum class SnapshotSubmissionType
{
    None = 0,        /**< No submission. */
    Header = 1 << 0, /**< Submission includes header. */
    Data = 1 << 1,   /**< Submission includes data. */
    Tail = 1 << 2    /**< Submission includes tail. */
};

/**
 * @brief Bitwise OR operator for SnapshotSubmissionType.
 * @param a First flag.
 * @param b Second flag.
 * @return Combined flag.
 */
inline SnapshotSubmissionType operator|(SnapshotSubmissionType a, SnapshotSubmissionType b)
{
    return static_cast<SnapshotSubmissionType>(static_cast<int>(a) | static_cast<int>(b));
}

/**
 * @brief Bitwise AND operator for SnapshotSubmissionType.
 * @param a First flag.
 * @param b Second flag.
 * @return Intersection of flags.
 */
inline SnapshotSubmissionType operator&(SnapshotSubmissionType a, SnapshotSubmissionType b)
{
    return static_cast<SnapshotSubmissionType>(static_cast<int>(a) & static_cast<int>(b));
}

/**
 * @brief Bitwise OR assignment operator for SnapshotSubmissionType.
 * @param a Reference to flag to update.
 * @param b Flag to OR in.
 * @return Reference to updated flag.
 */
inline SnapshotSubmissionType& operator|=(SnapshotSubmissionType& a, SnapshotSubmissionType b)
{
    a = a | b;
    return a;
}

// forward declaration
class SnapshotOpInfo;

/**
 * @class SnapshotSubmission
 * @brief Represents a single snapshot submission, including its events and type.
 *
 * Contains the time of the snapshot, the events captured, and the submission type flags.
 * Enforces move semantics to avoid accidental copies of large event vectors.
 */
class SnapshotSubmission
{
public:
    std::chrono::steady_clock::time_point                 header_timestamp; /**< Time point for the snapshot header. */
    std::chrono::steady_clock::time_point                 snap_time;         /**< Time point for the snapshot. */
    std::vector<service::epics_impl::MonitorEventShrdPtr> snapshot_events;   /**< Events captured in the snapshot. */
    SnapshotSubmissionType                                submission_type;   /**< Type flags for the submission. */

    /**
     * @brief Constructs a SnapshotSubmission with given time, events, and type.
     * @param snap_time Time point for the snapshot.
     * @param snapshot_events Vector of monitor events (moved).
     * @param submission_type Submission type flags.
     */
    SnapshotSubmission(
        const std::chrono::steady_clock::time_point&            snap_time,
        const std::chrono::steady_clock::time_point&            header_timestamp,
        std::vector<service::epics_impl::MonitorEventShrdPtr>&& snapshot_events,
        SnapshotSubmissionType                                  submission_type);

    /**
     * @brief Move constructor.
     * @param other SnapshotSubmission to move from.
     */
    SnapshotSubmission(SnapshotSubmission&& other) noexcept;

    /**
     * @brief Move assignment operator.
     * @param other SnapshotSubmission to move from.
     * @return Reference to this object.
     */
    SnapshotSubmission& operator=(SnapshotSubmission&& other) noexcept;

    /**
     * @brief Deleted copy constructor to enforce move semantics.
     */
    SnapshotSubmission(const SnapshotSubmission&) = delete;
    /**
     * @brief Deleted copy assignment operator to enforce move semantics.
     */
    SnapshotSubmission& operator=(const SnapshotSubmission&) = delete;
};

/**
 * @brief Defines shared and unique pointer types for SnapshotSubmission.
 */
DEFINE_PTR_TYPES(SnapshotSubmission);

/**
 * @class SnapshotOpInfo
 * @brief Abstract base class for managing snapshot operations.
 *
 * Stores state, configuration, and provides an interface for adding and retrieving snapshot data.
 * Derived classes must implement data handling and statistics reporting.
 */
class SnapshotOpInfo : public WorkerAsyncOperation
{
    SnapshotStatisticCounterShrdPtr snapshot_statistic;

protected:
    /**
     * @brief Filters PVStructure fields, returning only those in fields_to_include.
     * @param src Source PVStructure.
     * @param fields_to_include Set of field names to include.
     * @return Filtered PVStructure pointer.
     */
    const epics::pvData::PVStructure::const_shared_pointer filterPVField(const epics::pvData::PVStructure::const_shared_pointer& src, const std::unordered_set<std::string>& fields_to_include);

public:
    std::promise<void>                                                   removal_promise;              /**< Promise for signaling removal of operation. */
    k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd;                          /**< Pointer to associated repeating snapshot command. */
    std::atomic<int64_t>                                                 snapshot_iteration_index = 0; /**< Index of current snapshot iteration. */
    const std::string                                                    queue_name;                   /**< Name of the queue for this operation. */
    const bool                                                           is_triggered;                 /**< Indicates if snapshot is triggered (immutable). */
    bool                                                                 request_to_trigger = false;   /**< Flag to request a trigger. */
    bool                                                                 is_running = true;            /**< Indicates if operation is running. */
    k2eg::service::configuration::SnapshotConfigurationShrdPtr           snapshot_configuration;       /**< Snapshot configuration pointer. */

    /**
     * @brief Constructs SnapshotOpInfo with queue name and command pointer.
     * @param queue_name Name of the queue.
     * @param cmd Pointer to repeating snapshot command.
     */
    SnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd);

    /**
     * @brief Virtual destructor.
     */
    virtual ~SnapshotOpInfo();

    /**
     * @brief Initializes operation with a list of sanitized PV names.
     * @param sanitized_pv_name_list List of sanitized PV pointers.
     * @return True if initialization succeeds.
     */
    virtual bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list) = 0;

    /**
     * @brief Adds monitor event data to the operation.
     * @param event_data Monitor event pointer.
     */
    virtual void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data) = 0;

    /**
     * @brief Retrieves collected monitor event data.
     * @return Shared pointer to SnapshotSubmission.
     */
    virtual SnapshotSubmissionShrdPtr getData() = 0;

    /**
     * @brief Checks if the operation has timed out.
     * @param now Current time point (default: now).
     * @return True if timed out.
     */
    virtual bool isTimeout(const std::chrono::steady_clock::time_point& now = std::chrono::steady_clock::now()) override;

    /**
     * @brief Retrieves the snapshot statistic counter.
     * @return Shared pointer to SnapshotStatisticCounter.
     */
    SnapshotStatisticCounterShrdPtr getStatisticCounter();
};

/**
 * @brief Defines shared and unique pointer types for SnapshotOpInfo.
 */
DEFINE_PTR_TYPES(SnapshotOpInfo)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTOPINFO_H_