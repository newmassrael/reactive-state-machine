#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace SCXML {
namespace Common {

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity { INFO, WARNING, ERROR, CRITICAL };

/**
 * @brief Structured error information
 */
struct ErrorInfo {
    ErrorSeverity severity;
    std::string code;      // Error code (e.g., "SCXML_001")
    std::string message;   // Human-readable message
    std::string context;   // Additional context information
    std::string location;  // File, line, function information

    ErrorInfo(ErrorSeverity sev, const std::string &cd, const std::string &msg, const std::string &ctx = "",
              const std::string &loc = "")
        : severity(sev), code(cd), message(msg), context(ctx), location(loc) {}
};

/**
 * @brief Result wrapper template for operations that can succeed or fail
 */
template <typename T> class Result {
private:
    std::optional<T> value_;
    std::vector<ErrorInfo> errors_;
    bool success_;

public:
    // Success constructor
    explicit Result(T &&value) : value_(std::move(value)), success_(true) {}

    explicit Result(const T &value) : value_(value), success_(true) {}

    // Failure constructor
    explicit Result(const ErrorInfo &error) : success_(false) {
        errors_.push_back(error);
    }

    explicit Result(std::vector<ErrorInfo> &&errors) : errors_(std::move(errors)), success_(false) {}

    // Default failure constructor
    Result() : success_(false) {}

    // Copy and move constructors
    Result(const Result &other) = default;
    Result(Result &&other) noexcept = default;
    Result &operator=(const Result &other) = default;
    Result &operator=(Result &&other) noexcept = default;

    // Status checking
    bool isSuccess() const {
        return success_;
    }

    bool isFailure() const {
        return !success_;
    }

    explicit operator bool() const {
        return success_;
    }

    // Value access (only valid if successful)
    const T &getValue() const {
        if (!success_) {
            throw std::runtime_error("Attempting to access value of failed Result");
        }
        return *value_;
    }

    T &getValue() {
        if (!success_) {
            throw std::runtime_error("Attempting to access value of failed Result");
        }
        return *value_;
    }

    // Safe value access with default
    T getValueOr(const T &defaultValue) const {
        return success_ ? *value_ : defaultValue;
    }

    // Error access
    const std::vector<ErrorInfo> &getErrors() const {
        return errors_;
    }

    bool hasErrors() const {
        return !errors_.empty();
    }

    bool hasCriticalErrors() const {
        return std::any_of(errors_.begin(), errors_.end(),
                           [](const ErrorInfo &err) { return err.severity == ErrorSeverity::CRITICAL; });
    }

    // Add error to failed result
    void addError(const ErrorInfo &error) {
        errors_.push_back(error);
        success_ = false;
    }

    // Create error results
    static Result<T> failure(const std::string &message, const std::string &code = "GENERIC_ERROR",
                             ErrorSeverity severity = ErrorSeverity::ERROR, const std::string &context = "") {
        return Result<T>(ErrorInfo(severity, code, message, context));
    }

    static Result<T> success(T &&value) {
        return Result<T>(std::move(value));
    }

    static Result<T> success(const T &value) {
        return Result<T>(value);
    }

    // Transform result (monadic map)
    template <typename U> Result<U> map(std::function<U(const T &)> transform) const {
        if (success_) {
            try {
                return Result<U>::success(transform(*value_));
            } catch (const std::exception &e) {
                return Result<U>::failure("Transformation failed: " + std::string(e.what()));
            }
        }

        // Copy errors to new result
        Result<U> result;
        result.errors_ = errors_;
        return result;
    }

    // Flat map (monadic bind)
    template <typename U> Result<U> flatMap(std::function<Result<U>(const T &)> transform) const {
        if (success_) {
            return transform(*value_);
        }

        // Copy errors to new result
        Result<U> result;
        result.errors_ = errors_;
        return result;
    }
};

// Specialization for void results
template <> class Result<void> {
private:
    std::vector<ErrorInfo> errors_;
    bool success_;

public:
    // Success constructor
    Result() : success_(true) {}

    // Failure constructor
    explicit Result(const ErrorInfo &error) : success_(false) {
        errors_.push_back(error);
    }

    explicit Result(std::vector<ErrorInfo> &&errors) : errors_(std::move(errors)), success_(false) {}

    // Status checking
    bool isSuccess() const {
        return success_;
    }

    bool isFailure() const {
        return !success_;
    }

    explicit operator bool() const {
        return success_;
    }

    // Error access
    const std::vector<ErrorInfo> &getErrors() const {
        return errors_;
    }

    bool hasErrors() const {
        return !errors_.empty();
    }

    bool hasCriticalErrors() const {
        return std::any_of(errors_.begin(), errors_.end(),
                           [](const ErrorInfo &err) { return err.severity == ErrorSeverity::CRITICAL; });
    }

    // Add error
    void addError(const ErrorInfo &error) {
        errors_.push_back(error);
        success_ = false;
    }

    // Create results
    static Result<void> failure(const std::string &message, const std::string &code = "GENERIC_ERROR",
                                ErrorSeverity severity = ErrorSeverity::ERROR, const std::string &context = "") {
        return Result<void>(ErrorInfo(severity, code, message, context));
    }

    static Result<void> success() {
        return Result<void>();
    }
};

// Utility functions for creating results
template <typename T> Result<T> Ok(T &&value) {
    return Result<T>::success(std::move(value));
}

template <typename T> Result<T> Ok(const T &value) {
    return Result<T>::success(value);
}

inline Result<void> Ok() {
    return Result<void>::success();
}

template <typename T> Result<T> Err(const std::string &message, const std::string &code = "ERROR") {
    return Result<T>::failure(message, code);
}

inline Result<void> ErrVoid(const std::string &message, const std::string &code = "ERROR") {
    return Result<void>::failure(message, code);
}

}  // namespace Common
}  // namespace SCXML