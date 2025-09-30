#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTREPEATINGBACKTIMEDBUFFEREDREPEATINGOPINFO_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTREPEATINGBACKTIMEDBUFFEREDREPEATINGOPINFO_H_

#include <k2eg/common/TimeWindowEventBuffer.h>
#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace k2eg::controller::node::worker::snapshot {

using MonitorEventBacktimeBuffer = k2eg::common::TimeWindowEventBuffer<k2eg::service::epics_impl::MonitorEvent>;
DEFINE_PTR_TYPES(MonitorEventBacktimeBuffer)

/*
@brief Double-buffered back-timed snapshot operation for EPICS PV monitoring.

@details
BackTimedBufferedSnapshotOpInfo implements a double-buffered, time-windowed event buffer for repeating snapshot
operations in EPICS-based control systems.

**Purpose:**
This class enables efficient and lossless collection of Process Variable (PV) events over a configurable time window,
supporting both periodic and triggered snapshot modes. It is designed to ensure that data acquisition never stops, even
while a snapshot is being processed and published.

**How it works:**

- **Double Buffering:**
  Two internal buffers are maintained: one for acquiring new events (`acquiring_buffer`) and one for
processing/publishing (`processing_buffer`).
  - All incoming PV events are pushed into the acquiring buffer.
  - When a snapshot is triggered (either by timeout or manual trigger), the buffers are swapped under a mutex lock.
  - The processing buffer is then used to fetch and publish the snapshot data, while the acquiring buffer immediately
resumes collecting new events.

- **Time Window:**
  Each buffer only retains events within a configurable time window (e.g., the last N milliseconds). This ensures that
snapshots always contain recent data, and old data is automatically pruned.

- **Thread Safety:**
  All buffer operations and swaps are protected by a mutex to ensure thread safety between data acquisition and snapshot
processing.

- **Field Filtering:**
  When fetching data for a snapshot, the class can filter PV fields(using cmd->pv_field_filter_list), returning only the
specified fields if requested.

**Benefits:**
- No data loss during snapshot processing.
- Consistent and up-to-date snapshots.
- Efficient memory usage with automatic pruning.
- Thread-safe operation for concurrent acquisition and processing.
*/

#define FAST_EXPIRE_TIME_MSEC 1000

class BackTimedBufferedSnapshotOpInfo : public SnapshotOpInfo
{
    // define when the snapshot is acquiring data
    mutable std::shared_mutex             buffer_mutex;
    MonitorEventBacktimeBufferShrdPtr     acquiring_buffer;
    MonitorEventBacktimeBufferShrdPtr     processing_buffer;
    std::chrono::steady_clock::time_point last_forced_expire = std::chrono::steady_clock::now();
    bool                                  header_sent = false;
    std::chrono::steady_clock::time_point header_snapshot_time; /**< Time point for the snapshot. */
    bool                                  win_time_expired = false;
    std::int64_t                          fast_expire_time_msec = 0; // fast expire time in milliseconds
    // Per-window PV stats
    std::unordered_set<std::string>                    all_pvs_;        // full PV set for this snapshot
    std::unordered_set<std::string>                    pvs_no_events_;  // PVs that did not receive any event in the current window
    std::unordered_map<std::string, std::uint64_t>     events_per_pv_;  // counter of events per PV in the current window
    // Snapshot of silent PVs captured before stats reset on full window expiration
    mutable std::vector<std::string>                   pending_pvs_without_events_;
    mutable bool                                       pending_pvs_without_events_ready_ = false;
public:
    // Buffer to store all received values during the time window
    std::map<std::string, std::vector<k2eg::service::epics_impl::MonitorEventShrdPtr>> value_buffer;
    /**
     * @brief Construct a new BackTimedBufferedSnapshotOpInfo object.
     * @param queue_name The name of the queue associated with this snapshot operation.
     * @param cmd The repeating snapshot command configuration.
     */
    BackTimedBufferedSnapshotOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd);
    /**
     * @brief Destructor.
     */
    virtual ~BackTimedBufferedSnapshotOpInfo();
    /**
     * @brief Initialize the snapshot operation with a sanitized list of PVs.
     * @param sanitized_pv_name_list The list of sanitized PV shared pointers.
     * @return true if initialization succeeded, false otherwise.
     */
    bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list) override;
    /**
     * @brief Check if the snapshot operation has reached its timeout.
     *        If so, swap the acquiring and processing buffers.
     * @return true if timeout occurred and buffers were swapped, false otherwise.
     */
    bool isTimeout(const std::chrono::steady_clock::time_point& now = std::chrono::steady_clock::now()) override;
    /**
     * @brief Add a new monitor event to the acquiring buffer.
     * @param event_data The event data to add.
     */
    void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data) override;
    /**
     * @brief Retrieve all monitor events from the processing buffer.
     *        If field filtering is enabled, only the specified fields are included.
     * @return A vector of shared pointers to MonitorEvent objects.
     */
    SnapshotSubmissionShrdPtr getData() override;

    /**
     * @brief Fast retrieval of PVs that received zero events in the current window.
     */
    std::vector<std::string> getPVsWithoutEvents() const override;

protected:
    /**
     * @brief Reset per-window counters when a window expires (full or partial).
     */
    void onWindowTimeout(bool full_window) override;
};
DEFINE_PTR_TYPES(BackTimedBufferedSnapshotOpInfo)
} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SNAPSHOTREPEATINGBACKTIMEDBUFFEREDREPEATINGOPINFO_H_
