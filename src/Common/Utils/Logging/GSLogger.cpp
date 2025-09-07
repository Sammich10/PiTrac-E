#include "Common/Utils/Logging/GSLogger.h"
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions/formatters/named_scope.hpp>
#include <boost/log/expressions/formatters/stream.hpp>
#include <boost/log/expressions/formatters/wrap_formatter.hpp>
#include <boost/log/expressions/formatters/if.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>
#include <cstdarg>
#include <cstdio>
#include <iomanip>
#include <unistd.h>
#include <climits>
namespace PiTrac
{
GSLogger::GSLogger(const logger_level logLevel)
    : logLevel_(logLevel)
{
    Init();
}

GSLogger::~GSLogger()
{
    if (fileSink_)
    {
        boost::log::core::get()->remove_sink(fileSink_);
        fileSink_->flush();
        fileSink_.reset();
    }

    if (consoleSink_)
    {
        boost::log::core::get()->remove_sink(consoleSink_);
        consoleSink_->flush();
        consoleSink_.reset();
    }
}

void GSLogger::Init()
{
    // Add common attributes
    boost::log::add_common_attributes();

    // Add process ID and process name attributes
    boost::log::core::get()->add_global_attribute("ProcessID",
                                                  boost::log::attributes::current_process_id());
    boost::log::core::get()->add_global_attribute("ProcessName",
                                                  boost::log::attributes::current_process_name());
    boost::log::core::get()->add_global_attribute("Scope",
                                                  boost::log::attributes::named_scope());

    // Set global filter based on log level
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= static_cast<boost::log::trivial::severity_level>(logLevel_)
        );

    // Create formatter with timestamp, PID, process name, thread ID, severity,
    // and message
    // Format: [YYYY-MM-DD HH:MM:SS.ffffff] [PID:0x12345] [ProcessName]
    // [TID:0x12345] [SEVERITY] Message
    auto fmtTimeStamp = boost::log::expressions::format_date_time<boost::posix_time::ptime>(
        "TimeStamp", "%Y-%m-%d %H:%M:%S.%f");
    auto fmtProcessName = boost::log::expressions::attr<std::string>("ProcessName");
    auto fmtSeverity =
        boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity");

    // Use stream formatter to ensure decimal formatting
    boost::log::formatter logFmt =
        boost::log::expressions::stream
            << "[" << fmtTimeStamp << "] "
            << "[PID:"
            << boost::log::expressions::attr<boost::log::attributes::current_process_id::value_type>
        ("ProcessID")
            << "] "
            << "[" << fmtProcessName << "] "
            << "[TID:"
            << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>(
            "ThreadID")
            << "] "
            << "[" << fmtSeverity << "] "
            << boost::log::expressions::smessage;

    // Console sink
    consoleSink_ = boost::log::add_console_log(std::clog);
    consoleSink_->set_formatter(logFmt);

    logFileName_ = "/tmp/" + getProcessName() + ".log";

    // File sink (if filename provided)
    if (!logFileName_.empty())
    {
        // Ensure log directory exists
        boost::filesystem::path logPath(logFileName_);
        boost::filesystem::path logDir = logPath.parent_path();
        if (!logDir.empty() && !boost::filesystem::exists(logDir))
        {
            boost::filesystem::create_directories(logDir);
        }

        fileSink_ = boost::log::add_file_log(
            boost::log::keywords::file_name = logFileName_,
            boost::log::keywords::rotation_size = 10 * 1024 * 1024,  // 10MB
            boost::log::keywords::min_free_space = 30 * 1024 * 1024, // 30MB
            boost::log::keywords::open_mode = std::ios_base::app,
            boost::log::keywords::auto_flush = true
            );
        fileSink_->set_formatter(logFmt);
    }
}

std::string GSLogger::formatMessage(const char *format, va_list args)
{
    // Determine required size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    if (size < 0)
    {
        return "Error formatting message";
    }

    // Format the string
    std::string result(size + 1, '\0');
    vsnprintf(&result[0], size + 1, format, args);
    result.resize(size); // Remove null terminator

    return result;
}

void GSLogger::logMessage(boost::log::trivial::severity_level severity, const std::string &message)
{
    BOOST_LOG_SEV(logger_, severity) << message;
}

// Printf-style logging methods
void GSLogger::trace(const char *format, ...)
{
    if (static_cast<boost::log::trivial::severity_level>(logLevel_) <= boost::log::trivial::trace)
    {
        va_list args;
        va_start(args, format);
        std::string message = formatMessage(format, args);
        va_end(args);
        logMessage(boost::log::trivial::trace, message);
    }
}

void GSLogger::debug(const char *format, ...)
{
    if (static_cast<boost::log::trivial::severity_level>(logLevel_) <= boost::log::trivial::debug)
    {
        va_list args;
        va_start(args, format);
        std::string message = formatMessage(format, args);
        va_end(args);
        logMessage(boost::log::trivial::debug, message);
    }
}

void GSLogger::info(const char *format, ...)
{
    if (static_cast<boost::log::trivial::severity_level>(logLevel_) <= boost::log::trivial::info)
    {
        va_list args;
        va_start(args, format);
        std::string message = formatMessage(format, args);
        va_end(args);
        logMessage(boost::log::trivial::info, message);
    }
}

void GSLogger::warning(const char *format, ...)
{
    if (static_cast<boost::log::trivial::severity_level>(logLevel_) <= boost::log::trivial::warning)
    {
        va_list args;
        va_start(args, format);
        std::string message = formatMessage(format, args);
        va_end(args);
        logMessage(boost::log::trivial::warning, message);
    }
}

void GSLogger::error(const char *format, ...)
{
    if (static_cast<boost::log::trivial::severity_level>(logLevel_) <= boost::log::trivial::error)
    {
        va_list args;
        va_start(args, format);
        std::string message = formatMessage(format, args);
        va_end(args);
        logMessage(boost::log::trivial::error, message);
    }
}

void GSLogger::fatal(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::string message = formatMessage(format, args);
    va_end(args);
    logMessage(boost::log::trivial::fatal, message);
}

// String-based logging methods
void GSLogger::trace(const std::string &message)
{
    logMessage(boost::log::trivial::trace, message);
}

void GSLogger::debug(const std::string &message)
{
    logMessage(boost::log::trivial::debug, message);
}

void GSLogger::info(const std::string &message)
{
    logMessage(boost::log::trivial::info, message);
}

void GSLogger::warning(const std::string &message)
{
    logMessage(boost::log::trivial::warning, message);
}

void GSLogger::error(const std::string &message)
{
    logMessage(boost::log::trivial::error, message);
}

void GSLogger::fatal(const std::string &message)
{
    logMessage(boost::log::trivial::fatal, message);
}

void GSLogger::setLogLevel(logger_level level)
{
    logLevel_ = level;
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= static_cast<boost::log::trivial::severity_level>(logLevel_)
        );
}

std::string GSLogger::getProcessName()
{
    // Try to read from /proc/self/comm
    std::ifstream comm("/proc/self/comm");
    if (comm.is_open())
    {
        std::string name;
        std::getline(comm, name);
        if (!name.empty())
        {
            return name;
        }
    }

    // Parse from /proc/self/cmdline
    std::ifstream cmdline("/proc/self/cmdline");
    if (cmdline.is_open())
    {
        std::string fullPath;
        std::getline(cmdline, fullPath, '\0'); // Read until null terminator

        // Extract just the executable name
        size_t lastSlash = fullPath.find_last_of('/');
        if (lastSlash != std::string::npos)
        {
            return fullPath.substr(lastSlash + 1);
        }
        return fullPath;
    }

    // Fallback
    return "unknown_process";
}
} // namespace PiTrac