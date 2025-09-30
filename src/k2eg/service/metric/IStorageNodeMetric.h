#ifndef K2EG_SERVICE_METRIC_ISTORAGENODEMETRIC_H_
#define K2EG_SERVICE_METRIC_ISTORAGENODEMETRIC_H_

#include <map>
#include <string>

namespace k2eg::service::metric {

/**
 * @brief Storage node metric gauge types
 */
enum class IStorageNodeMetricGaugeType {
  /** Current number of running archivers (gauge) */
  RunningArchivers
};

/**
 * @brief Storage node metrics interface
 */
class IStorageNodeMetric {
 public:
  IStorageNodeMetric() = default;
  virtual ~IStorageNodeMetric() = default;

  /**
   * @brief Update a storage metric value
   * @param type Metric identifier
   * @param inc_value Value to set or increment (implementation-defined)
   * @param label Optional labels
   */
  virtual void incrementCounter(IStorageNodeMetricGaugeType type,
                                const double inc_value = 1.0,
                                const std::map<std::string, std::string>& label = {}) = 0;
};

}  // namespace k2eg::service::metric

#endif  // K2EG_SERVICE_METRIC_ISTORAGENODEMETRIC_H_
