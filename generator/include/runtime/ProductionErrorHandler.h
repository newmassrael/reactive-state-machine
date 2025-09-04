#pragma once

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SCXML {

/**
 * @brief Production-ready error handling system for SCXML runtime
 *
 * This class provides comprehensive error handling capabilities including
 * error recovery, logging, monitoring, and graceful degradation for
 * production SCXML environments.
 */
class ProductionErrorHandler {
public:
    /**
     * @brief Error severity levels
     */
    enum class ErrorSeverity {
        INFO,      // Informational messages
        WARNING,   // Warning conditions
        ERROR,     // Error conditions
        CRITICAL,  // Critical errors
        FATAL      // Fatal errors requiring shutdown
    };

    /**
     * @brief Error categories for classification
     */
    enum class ErrorCategory {
        PARSER_ERROR,       // SCXML parsing errors
        RUNTIME_ERROR,      // Runtime execution errors
        STATE_ERROR,        // State machine errors
        TRANSITION_ERROR,   // Transition errors
        ACTION_ERROR,       // Action execution errors
        MEMORY_ERROR,       // Memory management errors
        PERFORMANCE_ERROR,  // Performance-related errors
        VALIDATION_ERROR,   // Input validation errors
        SYSTEM_ERROR,       // System-level errors
        NETWORK_ERROR,      // Network communication errors
        UNKNOWN_ERROR       // Unclassified errors
    };

    /**
     * @brief Error recovery strategy
     */
    enum class RecoveryStrategy {
        IGNORE,                // Ignore the error and continue
        LOG_AND_CONTINUE,      // Log error and continue execution
        RETRY,                 // Retry the failed operation
        FALLBACK,              // Use fallback behavior
        GRACEFUL_DEGRADATION,  // Reduce functionality but continue
        SAFE_SHUTDOWN,         // Shutdown safely
        IMMEDIATE_SHUTDOWN     // Shutdown immediately
    };

    /**
     * @brief Error information structure
     */
    struct ErrorInfo {
        size_t errorId;
        ErrorSeverity severity;
        ErrorCategory category;
        std::string errorCode;
        std::string message;
        std::string location;  // File:line or component
        std::string stackTrace;
        std::chrono::system_clock::time_point timestamp;
        std::unordered_map<std::string, std::string> context;  // Additional context
        RecoveryStrategy suggestedRecovery;
        bool recovered;
        std::string recoveryAction;

        ErrorInfo()
            : errorId(0), severity(ErrorSeverity::ERROR), category(ErrorCategory::UNKNOWN_ERROR),
              timestamp(std::chrono::system_clock::now()), suggestedRecovery(RecoveryStrategy::LOG_AND_CONTINUE),
              recovered(false) {}
    };

    /**
     * @brief Error statistics
     */
    struct ErrorStats {
        size_t totalErrors = 0;
        size_t errorsByCategory[static_cast<int>(ErrorCategory::UNKNOWN_ERROR) + 1] = {0};
        size_t errorsBySeverity[static_cast<int>(ErrorSeverity::FATAL) + 1] = {0};
        size_t recoveredErrors = 0;
        size_t unrecoveredErrors = 0;
        double errorRate = 0.0;  // Errors per second
        std::chrono::system_clock::time_point lastError;
        std::chrono::system_clock::time_point startTime;
    };

    /**
     * @brief Error recovery handler function type
     */
    using RecoveryHandler = std::function<bool(const ErrorInfo &)>;

    /**
     * @brief Error notification callback function type
     */
    using ErrorCallback = std::function<void(const ErrorInfo &)>;

    /**
     * @brief Construct a new Production Error Handler
     */
    ProductionErrorHandler();

    /**
     * @brief Destructor
     */
    ~ProductionErrorHandler();

    /**
     * @brief Initialize error handling system
     */
    void initialize();

    /**
     * @brief Shutdown error handling system
     */
    void shutdown();

    /**
     * @brief Report an error
     * @param severity Error severity level
     * @param category Error category
     * @param errorCode Unique error code
     * @param message Error message
     * @param location Source location
     * @param context Additional error context
     * @return Error ID for tracking
     */
    size_t reportError(ErrorSeverity severity, ErrorCategory category, const std::string &errorCode,
                       const std::string &message, const std::string &location = "",
                       const std::unordered_map<std::string, std::string> &context = {});

    /**
     * @brief Report an exception
     * @param ex Exception to report
     * @param category Error category
     * @param location Source location
     * @param context Additional context
     * @return Error ID for tracking
     */
    size_t reportException(const std::exception &ex, ErrorCategory category = ErrorCategory::RUNTIME_ERROR,
                           const std::string &location = "",
                           const std::unordered_map<std::string, std::string> &context = {});

    /**
     * @brief Set recovery strategy for error category
     * @param category Error category
     * @param strategy Recovery strategy to use
     */
    void setRecoveryStrategy(ErrorCategory category, RecoveryStrategy strategy);

    /**
     * @brief Register recovery handler for specific error code
     * @param errorCode Error code to handle
     * @param handler Recovery handler function
     */
    void registerRecoveryHandler(const std::string &errorCode, RecoveryHandler handler);

    /**
     * @brief Register error notification callback
     * @param callback Notification callback function
     */
    void registerErrorCallback(ErrorCallback callback);

    /**
     * @brief Attempt to recover from error
     * @param errorId Error ID to recover from
     * @return true if recovery was successful
     */
    bool attemptRecovery(size_t errorId);

    /**
     * @brief Get error information
     * @param errorId Error ID to query
     * @return Error information if found
     */
    std::optional<ErrorInfo> getErrorInfo(size_t errorId) const;

    /**
     * @brief Get recent errors
     * @param count Number of recent errors to retrieve
     * @return Vector of recent error information
     */
    std::vector<ErrorInfo> getRecentErrors(size_t count = 100) const;

    /**
     * @brief Get errors by category
     * @param category Error category to filter by
     * @return Vector of errors in category
     */
    std::vector<ErrorInfo> getErrorsByCategory(ErrorCategory category) const;

    /**
     * @brief Get errors by severity
     * @param severity Error severity to filter by
     * @return Vector of errors with severity
     */
    std::vector<ErrorInfo> getErrorsBySeverity(ErrorSeverity severity) const;

    /**
     * @brief Get error statistics
     * @return Current error statistics
     */
    ErrorStats getErrorStats() const;

    /**
     * @brief Clear error history
     */
    void clearErrorHistory();

    /**
     * @brief Set error rate monitoring
     * @param enabled Whether to enable error rate monitoring
     * @param threshold Error rate threshold (errors per second)
     */
    void setErrorRateMonitoring(bool enabled, double threshold = 10.0);

    /**
     * @brief Check if system is in healthy state
     * @return true if error rate is acceptable
     */
    bool isSystemHealthy() const;

    /**
     * @brief Generate error report
     * @return Formatted error report string
     */
    std::string generateErrorReport() const;

    /**
     * @brief Export error data to JSON
     * @return JSON string containing error information
     */
    std::string exportToJson() const;

    /**
     * @brief Set logging level
     * @param minSeverity Minimum severity to log
     */
    void setLoggingLevel(ErrorSeverity minSeverity);

    /**
     * @brief Enable or disable stack trace capture
     * @param enabled Whether to capture stack traces
     */
    void setStackTraceCapture(bool enabled);

    /**
     * @brief Set maximum error history size
     * @param maxSize Maximum number of errors to keep in history
     */
    void setMaxErrorHistory(size_t maxSize);

    // Convenience methods for common error reporting
    size_t reportInfo(const std::string &message, const std::string &location = "");
    size_t reportWarning(const std::string &message, const std::string &location = "");
    size_t reportError(const std::string &message, const std::string &location = "");
    size_t reportCritical(const std::string &message, const std::string &location = "");
    size_t reportFatal(const std::string &message, const std::string &location = "");

    // Helper methods for string conversion
    static std::string severityToString(ErrorSeverity severity);
    static std::string categoryToString(ErrorCategory category);
    static std::string strategyToString(RecoveryStrategy strategy);

    static ErrorSeverity stringToSeverity(const std::string &severity);
    static ErrorCategory stringToCategory(const std::string &category);
    static RecoveryStrategy stringToStrategy(const std::string &strategy);

protected:
    /**
     * @brief Execute recovery strategy for error
     * @param errorInfo Error to recover from
     * @return true if recovery was successful
     */
    bool executeRecoveryStrategy(ErrorInfo &errorInfo);

    /**
     * @brief Notify registered callbacks about error
     * @param errorInfo Error information
     */
    void notifyCallbacks(const ErrorInfo &errorInfo);

    /**
     * @brief Update error statistics
     * @param errorInfo New error information
     */
    void updateErrorStats(const ErrorInfo &errorInfo);

    /**
     * @brief Check error rate and trigger alerts if necessary
     */
    void checkErrorRate();

    /**
     * @brief Capture stack trace for current location
     * @return Stack trace string
     */
    std::string captureStackTrace() const;

    /**
     * @brief Trim error history to maximum size
     */
    void trimErrorHistory();

private:
    // Error tracking
    std::atomic<size_t> nextErrorId_;
    mutable std::mutex errorsMutex_;
    std::vector<ErrorInfo> errorHistory_;
    std::unordered_map<size_t, ErrorInfo> errorMap_;

    // Recovery configuration
    mutable std::mutex recoveryMutex_;
    std::unordered_map<ErrorCategory, RecoveryStrategy> categoryStrategies_;
    std::unordered_map<std::string, RecoveryHandler> recoveryHandlers_;

    // Callbacks and notifications
    mutable std::mutex callbacksMutex_;
    std::vector<ErrorCallback> errorCallbacks_;

    // Statistics
    mutable std::mutex statsMutex_;
    ErrorStats stats_;

    // Configuration
    ErrorSeverity loggingLevel_;
    bool stackTraceEnabled_;
    size_t maxErrorHistory_;
    bool errorRateMonitoringEnabled_;
    double errorRateThreshold_;

    /**
     * @brief Log error to system
     * @param errorInfo Error information to log
     */
    void logError(const ErrorInfo &errorInfo);
};

}  // namespace SCXML