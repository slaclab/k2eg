#ifndef K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SIMPLEREPEATINGSNAPSHOTOPINFO_H_
#define K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SIMPLEREPEATINGSNAPSHOTOPINFO_H_

#include <atomic>
#include <k2eg/common/types.h>
#include <k2eg/controller/node/worker/snapshot/SnapshotOpInfo.h>

namespace k2eg::controller::node::worker::snapshot {
using AtomicMonitorEventShrdPtr = std::atomic<k2eg::service::epics_impl::MonitorEventShrdPtr>;
using ShdAtomicCacheElementShrdPtr = std::shared_ptr<AtomicMonitorEventShrdPtr>;

/*
@brief SnapshotRepeatingOpInfo implements the "Normal" type of repeating snapshot operation.
@details
This class manages the state and data for a "Normal" repeating snapshot operation.
It holds the command specification and, after the command expires (i.e., is ready to be fired),
collects all PV data using the snapshot view (allowing retrieval of the latest received data for each PV).
The collected data is then published as part of the snapshot process.
*/
class SnapshotRepeatingOpInfo : public SnapshotOpInfo
{
public:
    /**
     * @brief Map of PV names to their atomic cache elements for this snapshot.
     * Each entry holds a shared pointer to an atomic pointer of the latest MonitorEvent for a PV.
     */
    std::unordered_map<std::string, ShdAtomicCacheElementShrdPtr> element_data;

    /**
     * @brief Indicates whether the snapshot is currently acquiring data.
     * Set to true while data is being collected for this snapshot.
     */
    std::atomic<bool> taking_data;

    /**
     * @brief Constructor for SnapshotRepeatingOpInfo.
     * @param queue_name The name of the queue associated with this snapshot.
     * @param cmd Shared pointer to the repeating snapshot command.
     */
    SnapshotRepeatingOpInfo(const std::string& queue_name, k2eg::controller::command::cmd::ConstRepeatingSnapshotCommandShrdPtr cmd);

    /**
     * @brief Initialize the snapshot operation with a list of PVs.
     * @param sanitized_pv_name_list List of shared pointers to sanitized PVs.
     * @return True if initialization is successful, false otherwise.
     */
    bool init(std::vector<service::epics_impl::PVShrdPtr>& sanitized_pv_name_list) override;

    /**
     * @brief Check if the snapshot operation has timed out.
     * @return True if the operation has timed out, false otherwise.
     */
    bool isTimeout(const std::chrono::steady_clock::time_point& now = std::chrono::steady_clock::now()) override ;

    /**
     * @brief Add monitor event data for a specific PV to the snapshot.
     * @details This method is called when the cache is being updated for a specific PV name.
     * It can be overridden by derived classes to perform specific actions when the cache is updated.
     * @param event_data Shared pointer to the monitor event data.
     */
    void addData(k2eg::service::epics_impl::MonitorEventShrdPtr event_data) override;

    /**
     * @brief Retrieve all collected monitor event data for this snapshot.
     * @return Vector of shared pointers to monitor event data.
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
DEFINE_PTR_TYPES(SnapshotRepeatingOpInfo)

} // namespace k2eg::controller::node::worker::snapshot

#endif // K2EG_CONTROLLER_NODE_WORKER_SNAPSHOT_SIMPLEREPEATINGSNAPSHOTOPINFO_H_