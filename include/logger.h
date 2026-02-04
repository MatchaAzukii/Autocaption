#pragma once

#include "common.h"

namespace autocaption {

// Simple logging utility
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& instance();

    void setLogLevel(LogLevel level) { level_ = level; }
    void setLogFile(const std::string& filepath);

    void log(LogLevel level, const std::string& message);
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg) { log(LogLevel::INFO, msg); }
    void warn(const std::string& msg) { log(LogLevel::WARN, msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }

private:
    Logger() = default;
    ~Logger();

    LogLevel level_ = LogLevel::INFO;
    std::ofstream file_;
    std::mutex mutex_;
};

// Convenience macros
#define LOG_DEBUG(msg) autocaption::Logger::instance().debug(msg)
#define LOG_INFO(msg) autocaption::Logger::instance().info(msg)
#define LOG_WARN(msg) autocaption::Logger::instance().warn(msg)
#define LOG_ERROR(msg) autocaption::Logger::instance().error(msg)

} // namespace autocaption
