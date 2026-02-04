#include "logger.h"
#include <iostream>

namespace autocaption {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::setLogFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
    file_.open(filepath, std::ios::app);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < level_) return;

    const char* level_str[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef PLATFORM_WINDOWS
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    oss << ' ' << level_str[static_cast<int>(level)] << ' ' << message;
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << oss.str() << std::endl;
    
    if (file_.is_open()) {
        file_ << oss.str() << std::endl;
        file_.flush();
    }
}

} // namespace autocaption
