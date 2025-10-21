#include "common/Logger.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace RSM {

std::shared_ptr<spdlog::logger> Logger::logger_;

// Hidden implementation namespace function definitions
namespace RSM_LOGGER_PRIVATE_NS {

void ensureLoggerInitialized() {
    if (!Logger::logger_) {
        Logger::logger_ = spdlog::stdout_color_mt("RSM");
        Logger::logger_->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

        // Check SPDLOG_LEVEL environment variable to set log level
        const char *env_level = std::getenv("SPDLOG_LEVEL");
        if (env_level) {
            std::string level_str(env_level);
            std::transform(level_str.begin(), level_str.end(), level_str.begin(), ::tolower);

            if (level_str == "trace") {
                Logger::logger_->set_level(spdlog::level::trace);
            } else if (level_str == "debug") {
                Logger::logger_->set_level(spdlog::level::debug);
            } else if (level_str == "info") {
                Logger::logger_->set_level(spdlog::level::info);
            } else if (level_str == "warn" || level_str == "warning") {
                Logger::logger_->set_level(spdlog::level::warn);
            } else if (level_str == "err" || level_str == "error") {
                Logger::logger_->set_level(spdlog::level::err);
            } else if (level_str == "critical") {
                Logger::logger_->set_level(spdlog::level::critical);
            } else if (level_str == "off") {
                Logger::logger_->set_level(spdlog::level::off);
            } else {
                Logger::logger_->set_level(spdlog::level::debug);
            }
        } else {
            Logger::logger_->set_level(spdlog::level::debug);
        }
    }
}

std::string extractCleanFunctionName(const std::source_location &loc) {
    std::string full_name = loc.function_name();

    // Find the opening parenthesis to locate the end of function signature
    size_t paren_pos = full_name.find('(');
    if (paren_pos == std::string::npos) {
        return "UnknownFunction";
    }

    // Work backwards from the opening parenthesis to find the function name with class
    size_t name_end = paren_pos;
    while (name_end > 0 && (std::isspace(full_name[name_end - 1]) || full_name[name_end - 1] == ')')) {
        name_end--;
    }

    // Find the start of the qualified function name (including class/namespace)
    // Look for return type separator (usually a space before the last identifier)
    size_t name_start = 0;
    size_t space_pos = std::string::npos;

    // Find the last space that's not inside template parameters
    int angle_bracket_count = 0;
    int paren_count = 0;
    for (size_t i = 0; i < name_end; i++) {
        char c = full_name[i];
        if (c == '<') {
            angle_bracket_count++;
        } else if (c == '>') {
            angle_bracket_count--;
        } else if (c == '(') {
            paren_count++;
        } else if (c == ')') {
            paren_count--;
        } else if (c == ' ' && angle_bracket_count == 0 && paren_count == 0) {
            space_pos = i;
        }
    }

    if (space_pos != std::string::npos) {
        name_start = space_pos + 1;
    }

    // Extract the qualified function name (with class/namespace)
    std::string qualified_name = full_name.substr(name_start, name_end - name_start);

    // Clean up any remaining artifacts at the beginning
    while (!qualified_name.empty() &&
           (std::isspace(qualified_name[0]) || qualified_name[0] == '*' || qualified_name[0] == '&')) {
        qualified_name = qualified_name.substr(1);
    }

    // Remove template parameters if they exist (keep class but remove templates)
    std::string result;
    int angle_count = 0;
    for (char c : qualified_name) {
        if (c == '<') {
            angle_count++;
        } else if (c == '>') {
            angle_count--;
        } else if (angle_count == 0) {
            result += c;
        }
    }

    // Remove any trailing whitespace
    while (!result.empty() && std::isspace(result.back())) {
        result.pop_back();
    }

    return result.empty() ? "UnknownFunction" : result;
}

void doFormatAndLog(spdlog::level::level_enum level, const std::string &message, const std::source_location &loc) {
    ensureLoggerInitialized();
    if (Logger::logger_) {
        std::string enhanced_message = extractCleanFunctionName(loc) + "() - " + message;
        Logger::logger_->log(level, enhanced_message);
    }
}

void doInitializeLogger(const std::string &logDir, bool logToFile) {
    if (!Logger::logger_) {
        if (logDir.empty() && !logToFile) {
            // Default initialization
            Logger::logger_ = spdlog::stdout_color_mt("RSM");
            Logger::logger_->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
        } else {
            // Initialize with file logging
            std::vector<spdlog::sink_ptr> sinks;

            // Always add console output
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
            sinks.push_back(console_sink);

            // Add file sink if file logging requested
            if (logToFile && !logDir.empty()) {
                // Create log directory
                std::filesystem::create_directories(logDir);

                // Create file path (rsm.log)
                std::filesystem::path logPath = std::filesystem::path(logDir) / "rsm.log";

                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
                sinks.push_back(file_sink);
            }

            // Create composite logger
            Logger::logger_ = std::make_shared<spdlog::logger>("RSM", sinks.begin(), sinks.end());
            Logger::logger_->set_level(spdlog::level::debug);

            // Register with spdlog registry
            spdlog::register_logger(Logger::logger_);
        }

        // Check SPDLOG_LEVEL environment variable to set log level
        const char *env_level = std::getenv("SPDLOG_LEVEL");
        if (env_level) {
            std::string level_str(env_level);
            std::transform(level_str.begin(), level_str.end(), level_str.begin(), ::tolower);

            if (level_str == "trace") {
                Logger::logger_->set_level(spdlog::level::trace);
            } else if (level_str == "debug") {
                Logger::logger_->set_level(spdlog::level::debug);
            } else if (level_str == "info") {
                Logger::logger_->set_level(spdlog::level::info);
            } else if (level_str == "warn" || level_str == "warning") {
                Logger::logger_->set_level(spdlog::level::warn);
            } else if (level_str == "err" || level_str == "error") {
                Logger::logger_->set_level(spdlog::level::err);
            } else if (level_str == "critical") {
                Logger::logger_->set_level(spdlog::level::critical);
            } else if (level_str == "off") {
                Logger::logger_->set_level(spdlog::level::off);
            } else {
                Logger::logger_->set_level(spdlog::level::debug);
            }
        } else {
            Logger::logger_->set_level(spdlog::level::debug);
        }
    }
}

}  // namespace RSM_LOGGER_PRIVATE_NS

}  // namespace RSM
