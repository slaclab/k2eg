#include <format>
#include <k2eg/service/log/impl/BoostLogger.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <algorithm>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>
#include <regex>
#include <string_view>
#include <thread>

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
using namespace boost::log::expressions;
using namespace k2eg::service::log;
using namespace k2eg::service::log::impl;

#define MESSAGE_ONLY_FORMAT  "[%TimeStamp%][%Severity%] %Message%"

#ifndef PROJECT_ROOT
    #define PROJECT_ROOT ""
#endif
namespace {
inline std::string_view shortenPath(std::string_view p)
{
    // Always return only the filename (basename) without directories.
    size_t slash = p.find_last_of("/\\");
    return (slash == std::string_view::npos) ? p : p.substr(slash + 1);
}

// Ellide the middle of a string to fit `width` characters, using '...' in the center.
static std::string ellide_middle(std::string_view s, std::size_t width)
{
    if (s.size() <= width)
        return std::string(s);
    if (width <= 3)
        return std::string(s.substr(0, width));
    std::size_t keep = (width - 3) / 2;
    std::size_t first = keep;
    std::size_t last = width - 3 - first;
    return std::string(std::string(s.substr(0, first)) + "..." + std::string(s.substr(s.size() - last)));
}
} // namespace

static std::unordered_map<std::string, std::string> method_name_cache;
static std::mutex                                   method_name_cache_mutex;

static std::string getClassAndMethod(std::string_view full_name)
{
    static const std::regex re(R"(([\w:]+)::(\w+)\s*\()");
    std::cmatch             match;
    if (std::regex_search(full_name.data(), match, re))
    {
        std::string class_name = match[1].str(); // Convert to std::string
        size_t      pos = class_name.rfind("::");
        if (pos != std::string::npos)
            class_name = class_name.substr(pos + 2);
        return class_name + "::" + match[2].str(); // Convert to std::string
    }
    return std::string(full_name);
}

static std::string getClassAndMethodEllipsis(std::string_view full_name, size_t width = 24)
{
    std::string result = getClassAndMethod(full_name);
    if (result.size() < width)
    {
        // Pad with spaces to the right
        result.append(width - result.size(), ' ');
        return result;
    }
    if (result.size() > width)
    {
        // Show start and end, ellipsis in the middle
        size_t first = (width / 2) - 2;
        size_t last = width - first - 3; // 3 for "..."
        return result.substr(0, first) + "..." + result.substr(result.size() - last, last);
    }
    return result;
}

static std::string getClassAndMethodEllipsisCached(std::string_view full_name, size_t width = 24)
{
    std::string key(full_name);
    {
        std::lock_guard<std::mutex> lock(method_name_cache_mutex);
        auto                        it = method_name_cache.find(key);
        if (it != method_name_cache.end())
            return it->second;
    }
    std::string formatted = getClassAndMethodEllipsis(full_name, width);
    {
        std::lock_guard<std::mutex> lock(method_name_cache_mutex);
        method_name_cache[key] = formatted;
    }
    return formatted;
}

static LogLevel
string_to_log_level(const std::string& token)
{
    LogLevel    level = LogLevel::INFO;
    std::string non_const_str = token;
    std::transform(non_const_str.begin(), non_const_str.end(), non_const_str.begin(), [](unsigned char c)
                   {
                       return std::tolower(c);
                   });
    if (non_const_str == "error")
        level = LogLevel::ERROR;
    else if (non_const_str == "info")
        level = LogLevel::INFO;
    else if (non_const_str == "debug")
        level = LogLevel::DEBUG;
    else if (non_const_str == "fatal")
        level = LogLevel::FATAL;
    else if (non_const_str == "trace")
        level = LogLevel::TRACE;
    return level;
}

BoostLogger::BoostLogger(ConstLogConfigurationUPtr _configuration)
    : ILogger(std::move(_configuration))
{
    logging::add_common_attributes();
    boost::shared_ptr<logging::core> logger = boost::log::core::get();

    if (configuration->log_on_console)
    {
        console_sink = logging::add_console_log(std::clog);
        console_sink->set_formatter(
            expr::stream
            << '[' << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << ']'
            << ' ' << '[' << std::setw(5) << std::left << logging::trivial::severity << ']'
            << ' ' << expr::smessage);
    }

    if (configuration->log_on_file)
    {
        file_sink = logging::add_file_log(keywords::file_name = configuration->log_file_name,                                    // file name pattern
                                          keywords::rotation_size = configuration->log_file_max_size_mb * 1024 * 1024,           // rotate files every 10 MiB...
                                          keywords::time_based_rotation = logging::sinks::file::rotation_at_time_point(0, 0, 0), // ...or at midnight
                                          keywords::format = MESSAGE_ONLY_FORMAT,
                                          keywords::auto_flush = true);
        file_sink->set_formatter(
            expr::stream
            << '[' << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << ']'
            << ' ' << '[' << std::setw(5) << std::left << logging::trivial::severity << ']'
            << ' ' << expr::smessage);
    }

    if (configuration->log_on_syslog)
    {
        // Creating a syslog sink.
        syslog_sink.reset(
            new sinks::synchronous_sink<sinks::syslog_backend>(keywords::use_impl = sinks::syslog::udp_socket_based, keywords::format = MESSAGE_ONLY_FORMAT));
        // Setting the remote address to sent syslog messages to.
        syslog_sink->locked_backend()->set_target_address(configuration->log_syslog_srv, configuration->log_syslog_srv_port);
        syslog_sink->set_formatter(
            expr::stream
            << '[' << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << ']'
            << ' ' << '[' << std::setw(5) << std::left << logging::trivial::severity << ']'
            << ' ' << expr::smessage);
        // Adding the sink to the core.b
        logger->add_sink(syslog_sink);
    }
    setLevel(string_to_log_level(configuration->log_level));
    // enable the log output in case of needs
    logger->set_logging_enabled(configuration->log_on_console || configuration->log_on_file || configuration->log_on_syslog);
}

BoostLogger::~BoostLogger()
{
    boost::shared_ptr<logging::core> logger = boost::log::core::get();
    if (console_sink.get())
    {
        logger->remove_sink(console_sink);
        console_sink.reset();
    }
    if (file_sink.get())
    {
        logger->remove_sink(file_sink);
        file_sink.reset();
    }
    if (syslog_sink.get())
    {
        logger->remove_sink(syslog_sink);
        syslog_sink.reset();
    }
}

IScopedLoggerUPtr BoostLogger::getScopedLogger(const std::string& scope)
{
    return MakeBoostScopedLoggerUPtr(scope, *this);
}

logging::trivial::severity_level
BoostLogger::getLevel(LogLevel level)
{
    logging::trivial::severity_level boost_level = logging::trivial::info;

    switch (level)
    {
    case LogLevel::ERROR: boost_level = logging::trivial::error; break;
    case LogLevel::INFO: boost_level = logging::trivial::info; break;
    case LogLevel::TRACE: boost_level = logging::trivial::trace; break;
    case LogLevel::DEBUG: boost_level = logging::trivial::debug; break;
    case LogLevel::FATAL: boost_level = logging::trivial::fatal; break;
    default: boost_level = logging::trivial::info;
    }
    return boost_level;
}

void BoostLogger::setLevel(LogLevel level)
{
    // set log
    logging::core::get()->set_filter(logging::trivial::severity >= getLevel(level));
}

void BoostLogger::logMessage(const std::string& message, LogLevel level, const std::source_location& location)
{
    if (!logging::core::get()->get_logging_enabled())
        return;
    logging::record rec = logger_mt.open_record(keywords::severity = getLevel(level));
    if (rec)
    {
        std::string final_msg;
        if (configuration->debug_info_in_log)
        {
            // thread id as hex (stable-ish)
            auto        tid_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
            std::string tid_hex = std::format("0x{:016x}", static_cast<unsigned long long>(tid_hash));

            std::string file = std::string(shortenPath(location.file_name()));
            std::string func = getClassAndMethod(location.function_name());

            // Pad the inner content of [file-line] column to a fixed width so messages align
            constexpr std::size_t FILELINE_COL_WIDTH = 30;
            // Reserve 2 chars for the surrounding brackets
            constexpr std::size_t INNER_WIDTH = (FILELINE_COL_WIDTH > 2) ? (FILELINE_COL_WIDTH - 2) : FILELINE_COL_WIDTH;
            std::string           fileline_inner = std::format("{}-{}", file, location.line());
            std::string           fileline_inner_ellided = ellide_middle(fileline_inner, INNER_WIDTH);
            std::string           fileline_inner_padded = std::format("{:<{}}", fileline_inner_ellided, INNER_WIDTH);
            std::string           fileline_padded = std::format("[tid:{}][{}]", tid_hex, fileline_inner_padded);
            final_msg = std::format("{} {}", fileline_padded, message);
        } else {
            final_msg = message;
        }

        logging::record_ostream strm(rec);
        strm << final_msg;
        strm.flush();
        logger_mt.push_record(boost::move(rec));
    }
}

#pragma region ScopedLogger Implementation

BoostScopedLogger::BoostScopedLogger(const std::string& _scope, BoostLogger& logger)
    : scope(_scope), logger(logger)
{
}

void BoostScopedLogger::logMessage(const std::string& message, LogLevel level, const std::source_location& location)
{
    logger.logMessage(std::format("[{}] {}", scope, message), level, location);
}