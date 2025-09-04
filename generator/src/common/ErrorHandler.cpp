#include "common/ErrorHandler.h"
#include "common/Logger.h"
#include <iostream>

namespace SCXML {
namespace Common {

ErrorHandler &ErrorHandler::getInstance() {
    static ErrorHandler instance;
    return instance;
}

void ErrorHandler::setLogger(std::shared_ptr<Logger> logger) {
    logger_ = logger;
}

void ErrorHandler::logLegacyError(const std::string &level, const std::string &message, const std::string &context) {
    ErrorSeverity severity = ErrorSeverity::ERROR;

    if (level == "WARNING" || level == "WARN") {
        severity = ErrorSeverity::WARNING;
    } else if (level == "CRITICAL" || level == "FATAL") {
        severity = ErrorSeverity::CRITICAL;
    } else if (level == "INFO") {
        severity = ErrorSeverity::INFO;
    }

    ErrorInfo errorInfo{severity, level, message, context};
    logError(errorInfo);
}

Result<void> ErrorHandler::fromBooleanSuccess(bool success, const std::string &errorMessage,
                                              const std::string &errorCode, const std::string &context) {
    if (success) {
        return ErrorHandler::success();
    } else {
        return error<void>(errorCode, errorMessage, context);
    }
}

void ErrorHandler::logError(const ErrorInfo &errorInfo) {
    if (logger_) {
        // Use structured logger if available - convert to Logger::Level enum
        Logger::Level logLevel = Logger::Level::INFO;
        if (errorInfo.severity == ErrorSeverity::ERROR || errorInfo.severity == ErrorSeverity::CRITICAL) {
            logLevel = Logger::Level::ERROR;
        } else if (errorInfo.severity == ErrorSeverity::WARNING) {
            logLevel = Logger::Level::WARNING;
        } else if (errorInfo.severity == ErrorSeverity::INFO) {
            logLevel = Logger::Level::INFO;
        }

        Logger::log(logLevel, errorInfo.message);
        if (!errorInfo.context.empty()) {
            Logger::log(Logger::Level::DEBUG, "Context: " + errorInfo.context);
        }
    } else {
        // Fallback to structured console output
        std::string output = "[" + getSeverityString(errorInfo.severity) + "] ";
        if (!errorInfo.code.empty()) {
            output += errorInfo.code + ": ";
        }
        output += errorInfo.message;

        if (!errorInfo.context.empty()) {
            output += " (Context: " + errorInfo.context + ")";
        }

        if (errorInfo.severity == ErrorSeverity::CRITICAL || errorInfo.severity == ErrorSeverity::ERROR) {
            Logger::error(output);
        } else if (errorInfo.severity == ErrorSeverity::WARNING) {
            Logger::warning(output);
        } else {
            Logger::info(output);
        }
    }
}

std::string ErrorHandler::getSeverityString(ErrorSeverity severity) const {
    switch (severity) {
    case ErrorSeverity::INFO:
        return "INFO";
    case ErrorSeverity::WARNING:
        return "WARNING";
    case ErrorSeverity::ERROR:
        return "ERROR";
    case ErrorSeverity::CRITICAL:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

}  // namespace Common
}  // namespace SCXML