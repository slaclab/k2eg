#include <k2eg/common/uuid.h>
#include <k2eg/service/pubsub/pubsub.h>
#include <k2eg/common/ProcSystemMetrics.h>

#include <iomanip>
#include <iostream>

class Message : public k2eg::service::pubsub::PublishMessage {
    const std::string request_type;
    const std::string distribution_key;
    const std::string queue;
    //! the message data
    const std::string message;
  
   public:
    Message(const std::string& queue, const std::string& message)
        : request_type("test"), distribution_key(k2eg::common::UUID::generateUUIDLite()), queue(queue), message(message) {}
    virtual ~Message() {}
  
    char*
    getBufferPtr() {
      return const_cast<char*>(message.c_str());
    }
    const size_t
    getBufferSize() {
      return message.size();
    }
    const std::string&
    getQueue() {
      return queue;
    }
    const std::string&
    getDistributionKey() {
      return distribution_key;
    }
    const std::string&
    getReqType() {
      return request_type;
    }
  };

  ;

// Class that owns ProcSystemMetrics and provides refresh, printHeader, and printSample
class SystemResourcePrinter {
    k2eg::common::ProcSystemMetrics metrics;

public:
    void refresh() {
        metrics.refresh();
    }

    static void printHeader() {
        std::cout << std::left
                  << std::setw(12) << "VmRSS[kB]"
                  << std::setw(12) << "VmHWM[kB]"
                  << std::setw(14) << "VmSize[kB]"
                  << std::setw(14) << "VmData[kB]"
                  << std::setw(14) << "VmSwap[kB]"
                  << std::setw(10) << "Threads"
                  << std::setw(12) << "OpenFDs"
                  << std::setw(12) << "MaxFDs"
                  << std::setw(12) << "Uptime[s]"
                  << std::setw(18) << "Starttime[jiffies]"
                  << std::endl;
    }

    void printSample() const {
        std::cout << std::left
                  << std::setw(12) << metrics.vm_rss
                  << std::setw(12) << metrics.vm_hwm
                  << std::setw(14) << metrics.vm_size
                  << std::setw(14) << metrics.vm_data
                  << std::setw(14) << metrics.vm_swap
                  << std::setw(10) << metrics.threads
                  << std::setw(12) << metrics.open_fds
                  << std::setw(12) << metrics.max_fds
                  << std::setw(12) << metrics.uptime_sec
                  << std::setw(18) << metrics.starttime_jiffies
                  << std::endl;
    }
};