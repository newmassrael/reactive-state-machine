#pragma once

#include "common/Logger.h"
#include "Result.h"
#include <memory>
#include <string>

namespace SCXML {
namespace Common {

/**
 * @brief Unified error handling utility for consistent error patterns
 *
 * This class provides a single point of truth for error handling across
 * the entire SCXML codebase. It eliminates the inconsistent patterns of:
 * - Direct std::cerr usage
 * - Mixed exception handling
 * - Inconsistent error logging
 * - Ad-hoc error reporting
 *
 * Design Principles:
 * - Single Responsibility: Only handles error reporting and conversion
 * - Consistent Interface: Same error handling pattern everywhere
 * - Structured Errors: All errors include context and severity
 * - Testable: Can be mocked for unit testing
 */
class ErrorHandler {
public:
    /**
     * @brief Get singleton instance of error handler
     * @return Global error handler instance
     */
    static ErrorHandler &getInstance();

    /**
     * @brief Set custom logger for error output
     * @param logger Custom logger instance
     */
    void setLogger(std::shared_ptr<Logger> logger);

    // ========== Result Creation Methods ==========

    /**
     * @brief Create success result
     * @tparam T Result type
     * @param value Success value
     * @return Success result
     */
    template <typename T> static Result<T> success(T &&value) {
        Result<T> result;
        result.success = true;
        result.value = std::forward<T>(value);
        return result;
    }

    /**
     * @brief Create success result for void operations
     * @return Success result
     */
    static Result<void> success() {
        return Result<void>::success();
    }

    /**
     * @brief Create error result with structured error info
     * @tparam T Result type
     * @param severity Error severity level
     * @param code Error code (e.g., "SCXML_001")
     * @param message Human-readable error message
     * @param context Additional context (e.g., file:line, state ID)
     * @return Error result
     */
    template <typename T>
    Result<T> createError(ErrorSeverity severity, const std::string &code, const std::string &message,
                          const std::string &context = "") {
        ErrorInfo errorInfo{severity, code, message, context};

        // Log the error using consistent logging
        logError(errorInfo);

        return Result<T>::failure(message, code, severity, context);
    }

    /**
     * @brief Create error result from exception
     * @tparam T Result type
     * @param e Exception to convert
     * @param context Additional context information
     * @return Error result
     */
    template <typename T> Result<T> fromException(const std::exception &e, const std::string &context = "") {
        return createError<T>(ErrorSeverity::ERROR, "EXCEPTION", e.what(), context);
    }

    // ========== Convenience Methods ==========

    /**
     * @brief Create critical error (system cannot continue)
     * @tparam T Result type
     * @param code Error code
     * @param message Error message
     * @param context Error context
     * @return Critical error result
     */
    template <typename T>
    Result<T> critical(const std::string &code, const std::string &message, const std::string &context = "") {
        return createError<T>(ErrorSeverity::CRITICAL, code, message, context);
    }

    /**
     * @brief Create error (operation failed)
     * @tparam T Result type
     * @param code Error code
     * @param message Error message
     * @param context Error context
     * @return Error result
     */
    template <typename T>
    Result<T> error(const std::string &code, const std::string &message, const std::string &context = "") {
        return createError<T>(ErrorSeverity::ERROR, code, message, context);
    }

    /**
     * @brief Create warning (operation succeeded with issues)
     * @tparam T Result type
     * @param value Success value
     * @param code Warning code
     * @param message Warning message
     * @param context Warning context
     * @return Success result with warning
     */
    template <typename T>
    Result<T> warning(T &&value, const std::string &code, const std::string &message, const std::string &context = "") {
        Result<T> result;
        result.success = true;
        result.value = std::forward<T>(value);
        result.warnings.push_back(ErrorInfo{ErrorSeverity::WARNING, code, message, context});

        // Log the warning
        logError(result.warnings.back());

        return result;
    }

    // ========== Legacy Pattern Replacement ==========

    /**
     * @brief Replace std::cerr error logging
     * @param level Error level string (e.g., "ERROR", "WARNING")
     * @param message Error message
     * @param context Additional context
     */
    void logLegacyError(const std::string &level, const std::string &message, const std::string &context = "");

    /**
     * @brief Convert boolean success pattern to Result<void>
     * @param success Boolean success indicator
     * @param errorMessage Error message if failed
     * @param errorCode Error code if failed
     * @param context Error context
     * @return Result<void>
     */
    Result<void> fromBooleanSuccess(bool success, const std::string &errorMessage = "Operation failed",
                                    const std::string &errorCode = "BOOL_FAIL", const std::string &context = "");

private:
    ErrorHandler() = default;

    std::shared_ptr<Logger> logger_;

    /**
     * @brief Internal error logging
     * @param errorInfo Error information to log
     */
    void logError(const ErrorInfo &errorInfo);

    /**
     * @brief Get error level string for logging
     * @param severity Error severity
     * @return Level string
     */
    std::string getSeverityString(ErrorSeverity severity) const;
};

// ========== Convenience Macros for Common Patterns ==========

/**
 * @brief Create error result with current location context
 */
#define SCXML_ERROR(type, code, message)                                                                               \
    ErrorHandler::getInstance().error<type>(code, message, __FILE__ ":" + std::to_string(__LINE__))

/**
 * @brief Create critical error with current location context
 */
#define SCXML_CRITICAL(type, code, message)                                                                            \
    ErrorHandler::getInstance().critical<type>(code, message, __FILE__ ":" + std::to_string(__LINE__))

/**
 * @brief Create success result
 */
#define SCXML_SUCCESS(value) ErrorHandler::success(value)

/**
 * @brief Replace legacy std::cerr usage
 */
#define SCXML_LOG_ERROR(level, message)                                                                                \
    ErrorHandler::getInstance().logLegacyError(level, message, __FILE__ ":" + std::to_string(__LINE__))

}  // namespace Common
}  // namespace SCXML