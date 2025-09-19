#pragma once

#include <string>
#include <sstream>
#include <spdlog/spdlog.h>
#include <source_location>

namespace RSM {

// Forward declaration for LoggerStream
class LoggerStream;

class Logger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    // String-based methods with automatic function name
    static void log(Level level, const std::string& message, const std::source_location& location = std::source_location::current());
    static void debug(const std::string& message, const std::source_location& location = std::source_location::current());
    static void info(const std::string& message, const std::source_location& location = std::source_location::current());
    static void warn(const std::string& message, const std::source_location& location = std::source_location::current());
    static void error(const std::string& message, const std::source_location& location = std::source_location::current());
    
    // Format-based methods with automatic function name
    template<typename... Args>
    static void debug(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void info(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void warn(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void error(const std::string& format, Args&&... args);
    
    // Stream-based methods with automatic function name
    static LoggerStream debug(const std::source_location& location = std::source_location::current());
    static LoggerStream info(const std::source_location& location = std::source_location::current());
    static LoggerStream warn(const std::source_location& location = std::source_location::current());
    static LoggerStream error(const std::source_location& location = std::source_location::current());

private:
    // Helper to format message with function name
    static std::string formatWithFunction(const std::string& message, const std::source_location& location);
};;;

// LoggerStream class for supporting << operator
class LoggerStream {
private:
    std::string buffer_;
    Logger::Level level_;
    std::source_location location_;
    
public:
    explicit LoggerStream(Logger::Level level, const std::source_location& location = std::source_location::current());
    ~LoggerStream();
    
    // Copy constructor and assignment operator (deleted to prevent issues)
    LoggerStream(const LoggerStream&) = delete;
    LoggerStream& operator=(const LoggerStream&) = delete;
    
    // Move constructor and assignment operator
    LoggerStream(LoggerStream&& other) noexcept;
    LoggerStream& operator=(LoggerStream&& other) noexcept;
    
    // Stream operator for string and other basic types
    LoggerStream& operator<<(const std::string& value);
    LoggerStream& operator<<(const char* value);
    LoggerStream& operator<<(int value);
    LoggerStream& operator<<(long value);
    LoggerStream& operator<<(long long value);
    LoggerStream& operator<<(unsigned int value);
    LoggerStream& operator<<(unsigned long value);
    LoggerStream& operator<<(unsigned long long value);
    LoggerStream& operator<<(float value);
    LoggerStream& operator<<(double value);
    LoggerStream& operator<<(bool value);
    LoggerStream& operator<<(char value);
};

// === Template method implementations ===
// Internal helper to access spdlog logger
namespace detail {
    std::shared_ptr<spdlog::logger> getLogger();
}

template<typename... Args>
void Logger::debug(const std::string& format, Args&&... args) {
    debug(fmt::format(fmt::runtime(format), std::forward<Args>(args)...));
}

template<typename... Args>
void Logger::info(const std::string& format, Args&&... args) {
    info(fmt::format(fmt::runtime(format), std::forward<Args>(args)...));
}

template<typename... Args>
void Logger::warn(const std::string& format, Args&&... args) {
    warn(fmt::format(fmt::runtime(format), std::forward<Args>(args)...));
}

template<typename... Args>
void Logger::error(const std::string& format, Args&&... args) {
    error(fmt::format(fmt::runtime(format), std::forward<Args>(args)...));
}

}  // namespace RSM