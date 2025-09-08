/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022-2025, Verdant Consultants, LLC.
 */

#ifndef GSLOGGER_H
#define GSLOGGER_H

// Boost.Log includes
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/attributes/current_process_id.hpp>
#include <boost/log/attributes/current_process_name.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
// OpenCV includes
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/highgui.hpp>
// Standard includes
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <cstdarg>

namespace PiTrac
{
enum class logger_level
{
    trace   = boost::log::trivial::trace,
    debug   = boost::log::trivial::debug,
    info    = boost::log::trivial::info,
    warning = boost::log::trivial::warning,
    error   = boost::log::trivial::error,
    fatal   = boost::log::trivial::fatal
};

class GSLogger
{
  public:

    static std::shared_ptr<GSLogger> getInstance();

    GSLogger
    (
        const GSLogger &
    ) = delete;

    GSLogger &operator=
    (
        const GSLogger &
    ) = delete;

    ~GSLogger
    (
        void
    );

    // Printf-style logging methods for each severity level
    void trace
    (
        const char *format, ...) __attribute__((format(printf, 2, 3))
                                               );

    void debug
    (
        const char *format, ...) __attribute__((format(printf, 2, 3))
                                               );

    void info
    (
        const char *format, ...) __attribute__((format(printf, 2, 3))
                                               );

    void warning
    (
        const char *format, ...) __attribute__((format(printf, 2, 3))
                                               );

    void error
    (
        const char *format, ...) __attribute__((format(printf, 2, 3))
                                               );

    void fatal
    (
        const char *format, ...) __attribute__((format(printf, 2, 3))
                                               );

    // String-based logging methods (for compatibility)
    void trace
    (
        const std::string &message
    );

    void debug
    (
        const std::string &message
    );

    void info
    (
        const std::string &message
    );

    void warning
    (
        const std::string &message
    );

    void error
    (
        const std::string &message
    );

    void fatal
    (
        const std::string &message
    );


    // Set log level at runtime
    void setLogLevel(logger_level level);

  private:
    explicit GSLogger
    (
        const logger_level logLevel = logger_level::info
    );
    void Init();
    std::string formatMessage(const char *format, va_list args);
    void logMessage(boost::log::trivial::severity_level severity, const std::string &message);
    std::string getProcessName();

    logger_level logLevel_;
    std::string logFileName_;
    boost::shared_ptr<boost::log::sinks::text_file_backend> fileBackend_;
    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> >
    fileSink_;
    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend> >
    consoleSink_;
    boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger_;

    static std::shared_ptr<GSLogger> instance_;
    static std::mutex instance_mutex_;
};
}

#endif // GSLOGGER_H
