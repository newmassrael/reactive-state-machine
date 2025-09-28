#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace RSM {

/**
 * @brief Comprehensive event descriptor for SCXML event system
 *
 * Contains all information needed to send an event according to SCXML specification.
 * Supports both internal and external events with full parameter support.
 */
struct EventDescriptor {
    std::string eventName;                      // Event name (required)
    std::string target = "#_internal";          // Target URI (default: internal)
    std::string data;                           // Event data payload
    std::string sendId;                         // Unique send identifier
    std::string sessionId;                      // Session ID that created this event (for cancellation)
    std::string type = "scxml";                 // Event type (scxml, platform, etc.)
    std::chrono::milliseconds delay{0};         // Delivery delay
    std::map<std::string, std::string> params;  // Additional parameters

    // Evaluation expressions (for dynamic values)
    std::string eventExpr;   // Dynamic event name expression
    std::string targetExpr;  // Dynamic target expression
    std::string delayExpr;   // Dynamic delay expression

    /**
     * @brief Check if this is an internal event
     * @return true if target indicates internal delivery
     */
    bool isInternal() const {
        return target == "#_internal" || target.empty();
    }

    /**
     * @brief Check if this is a delayed event
     * @return true if delay is greater than zero
     */
    bool isDelayed() const {
        return delay.count() > 0;
    }

    /**
     * @brief Validate event descriptor
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<std::string> validate() const {
        std::vector<std::string> errors;

        if (eventName.empty() && eventExpr.empty()) {
            errors.push_back("Event must have either name or eventexpr");
        }

        if (!eventName.empty() && !eventExpr.empty()) {
            errors.push_back("Event cannot have both name and eventexpr");
        }

        if (delay.count() < 0) {
            errors.push_back("Delay cannot be negative");
        }

        return errors;
    }
};

/**
 * @brief Result of event sending operation
 */
struct SendResult {
    bool isSuccess = false;
    std::string sendId;        // Assigned send ID (if successful)
    std::string errorMessage;  // Error description (if failed)

    enum class ErrorType {
        NONE,
        VALIDATION_ERROR,
        TARGET_NOT_FOUND,
        NETWORK_ERROR,
        TIMEOUT,
        CANCELLED,
        INTERNAL_ERROR
    } errorType = ErrorType::NONE;

    /**
     * @brief Create successful result
     * @param assignedSendId Send ID assigned to the event
     * @return Success result
     */
    static SendResult success(const std::string &assignedSendId) {
        SendResult result;
        result.isSuccess = true;
        result.sendId = assignedSendId;
        return result;
    }

    /**
     * @brief Create error result
     * @param error Error message
     * @param type Error type
     * @return Error result
     */
    static SendResult error(const std::string &error, ErrorType type = ErrorType::INTERNAL_ERROR) {
        SendResult result;
        result.isSuccess = false;
        result.errorMessage = error;
        result.errorType = type;
        return result;
    }
};

}  // namespace RSM