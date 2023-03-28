#ifndef SERVICE_LOG_IMPL_BOOSTLOGGER_H_
#define SERVICE_LOG_IMPL_BOOSTLOGGER_H_

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/attributes/attribute_name.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>

#include <k2eg/service/log/ILogger.h>
namespace k2eg
{
    namespace service
    {
        namespace log
        {
            namespace impl
            {
                /**
                 * Log implementation using boost library
                 */
                class BoostLogger : public ILogger
                {
                    boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> logger_mt;
                    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>> console_sink;
                    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>> file_sink;
                    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::syslog_backend>> syslog_sink;
                    inline boost::log::trivial::severity_level getLevel(LogLevel level);

                public:
                    BoostLogger(ConstLogConfigurationUPtr _configuration);
                    virtual ~BoostLogger();
                    void setLevel(LogLevel level);
                    void logMessage(const std::string &message, LogLevel level = LogLevel::INFO);
                };
            }
        }
    }
}

#endif // SERVICE_LOG_IMPL_BOOSTLOGGER_H_