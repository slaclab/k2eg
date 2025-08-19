#ifndef SERVICE_LOG_IMPL_BOOSTLOGGER_H_
#define SERVICE_LOG_IMPL_BOOSTLOGGER_H_

#include <k2eg/common/types.h>
#include <k2eg/service/log/ILogger.h>

#include <boost/log/attributes.hpp>
#include <boost/log/attributes/attribute_name.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>

namespace k2eg::service::log::impl {

/**
 * Log implementation using boost library
 */
class BoostLogger : public ILogger
{
    boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>                    logger_mt;
    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>> console_sink;
    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>>    file_sink;
    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::syslog_backend>>       syslog_sink;
    inline boost::log::trivial::severity_level                                                      getLevel(LogLevel level);

public:
    BoostLogger(ConstLogConfigurationUPtr _configuration);
    virtual ~BoostLogger();
    IScopedLoggerUPtr getScopedLogger(const std::string& scope) override;
    void              setLevel(LogLevel level) override;
    void              logMessage(const std::string& message, LogLevel level = LogLevel::INFO, const std::source_location& location = std::source_location::current()) override;
};

class BoostScopedLogger : public IScopedLogger
{
private:
    std::string  scope;
    BoostLogger& logger;

public:
    BoostScopedLogger(const std::string& _scope, BoostLogger& logger);
    BoostScopedLogger(const BoostScopedLogger&) = delete;
    virtual ~BoostScopedLogger() = default;
    void logMessage(const std::string& message, LogLevel level = LogLevel::INFO, const std::source_location& location = std::source_location::current()) override;
};
DEFINE_PTR_TYPES(BoostScopedLogger)
} // namespace k2eg::service::log::impl

#endif // SERVICE_LOG_IMPL_BOOSTLOGGER_H_