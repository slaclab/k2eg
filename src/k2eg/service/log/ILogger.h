#ifndef __ILOGGER_H__
#define __ILOGGER_H__

#include <k2eg/common/types.h>

#include <memory>
#include <string>

namespace k2eg::service::log {
// logger configuration type
typedef struct LogConfiguration {
    std::string log_level;
    bool log_on_console;
    bool log_on_file;
    std::string log_file_name;
    int log_file_max_size_mb;
    bool log_on_syslog;
    std::string log_syslog_srv;
    int log_syslog_srv_port;
} LogConfiguration;
DEFINE_PTR_TYPES(LogConfiguration)

typedef enum class LogLevel { TRACE, DEBUG, INFO, ERROR, FATAL } LogLevel;

// logger abstraction class
class ILogger {
protected:
    ConstLogConfigurationUPtr configuration;

public:
    ILogger(ConstLogConfigurationUPtr configuration)
        : configuration(std::move(configuration)){};
    virtual ~ILogger() = default;
    virtual void setLevel(LogLevel level) = 0;
    virtual void logMessage(const std::string& message, LogLevel level = LogLevel::INFO) = 0;
};

DEFINE_PTR_TYPES(ILogger)
} // namespace k2eg::service::log

#endif // __ILOGGER_H__