#pragma once

#include <memory>
#include <source_location>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <string>

// Modern macro-based private management
#define RSM_LOGGER_PRIVATE_NS __detail
#define RSM_PRIVATE_CALL(func) RSM_LOGGER_PRIVATE_NS::func

namespace RSM {

// Hidden implementation namespace
namespace RSM_LOGGER_PRIVATE_NS {
void doFormatAndLog(spdlog::level::level_enum level, const std::string &message, const std::source_location &loc);
void doInitializeLogger(const std::string &logDir, bool logToFile);
std::string extractCleanFunctionName(const std::source_location &loc);
void ensureLoggerInitialized();
}  // namespace RSM_LOGGER_PRIVATE_NS

class Logger {
public:
    static void initialize() {
        RSM_PRIVATE_CALL(doInitializeLogger)("", false);
    }

    static void initialize(const std::string &logDir, bool logToFile = true) {
        RSM_PRIVATE_CALL(doInitializeLogger)(logDir, logToFile);
    }

    // Legacy interface - keep for runtime string concatenation (with caller location)
    static void trace(const std::string &message, const std::source_location &loc = std::source_location::current()) {
        RSM_PRIVATE_CALL(doFormatAndLog)(spdlog::level::trace, message, loc);
    }

    static void debug(const std::string &message, const std::source_location &loc = std::source_location::current()) {
        RSM_PRIVATE_CALL(doFormatAndLog)(spdlog::level::debug, message, loc);
    }

    static void info(const std::string &message, const std::source_location &loc = std::source_location::current()) {
        RSM_PRIVATE_CALL(doFormatAndLog)(spdlog::level::info, message, loc);
    }

    static void warn(const std::string &message, const std::source_location &loc = std::source_location::current()) {
        RSM_PRIVATE_CALL(doFormatAndLog)(spdlog::level::warn, message, loc);
    }

    static void error(const std::string &message, const std::source_location &loc = std::source_location::current()) {
        RSM_PRIVATE_CALL(doFormatAndLog)(spdlog::level::err, message, loc);
    }

private:
    static std::shared_ptr<spdlog::logger> logger_;

    // Friend declaration for private namespace access
    friend void RSM_LOGGER_PRIVATE_NS::ensureLoggerInitialized();
    friend void RSM_LOGGER_PRIVATE_NS::doFormatAndLog(spdlog::level::level_enum level, const std::string &message,
                                                      const std::source_location &loc);
    friend void RSM_LOGGER_PRIVATE_NS::doInitializeLogger(const std::string &logDir, bool logToFile);
};

}  // namespace RSM

// Macro definitions for proper source_location capture with fmt::format support
#define LOG_TRACE(...) RSM::Logger::trace(fmt::format(__VA_ARGS__), std::source_location::current())
#define LOG_DEBUG(...) RSM::Logger::debug(fmt::format(__VA_ARGS__), std::source_location::current())
#define LOG_INFO(...) RSM::Logger::info(fmt::format(__VA_ARGS__), std::source_location::current())
#define LOG_WARN(...) RSM::Logger::warn(fmt::format(__VA_ARGS__), std::source_location::current())
#define LOG_ERROR(...) RSM::Logger::error(fmt::format(__VA_ARGS__), std::source_location::current())
