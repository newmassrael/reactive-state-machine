#pragma once
#include "common/Result.h"
#include <string>
#include <variant>

namespace SCXML {
namespace Model {

// Forward declarations

/**
 * @brief Abstract execution context interface to break circular dependencies
 *
 * This interface provides a clean abstraction layer between the Model and Runtime layers.
 * It allows Model components to execute actions without directly depending on Runtime classes.
 *
 * Design Principles:
 * - Single Responsibility: Provides execution context only
 * - Dependency Inversion: Model depends on abstraction, not concrete Runtime
 * - Interface Segregation: Only essential execution operations
 * - No Circular Dependencies: Breaks Model → Runtime cycles
 */
class IExecutionContext {
public:
    virtual ~IExecutionContext() = default;

    // ========== Data Model Operations ==========

    /**
     * @brief Set data value in execution context
     * @param name Variable name
     * @param value Variable value
     * @return Operation result
     */
    virtual SCXML::Common::Result<void> setDataValue(const std::string &name, const std::string &value) = 0;

    /**
     * @brief Get data value from execution context
     * @param name Variable name
     * @return Variable value or error
     */
    virtual SCXML::Common::Result<std::string> getDataValue(const std::string &name) const = 0;

    /**
     * @brief Check if data value exists
     * @param name Variable name
     * @return true if variable exists
     */
    virtual bool hasDataValue(const std::string &name) const = 0;

    // ========== Event Operations ==========

    /**
     * @brief Send event to state machine
     * @param eventName Name of the event
     * @param eventData Event data (optional)
     * @return Operation result
     */
    virtual SCXML::Common::Result<void> sendEvent(const std::string &eventName, const std::string &eventData = "") = 0;

    /**
     * @brief Raise internal event
     * @param eventName Name of the event
     * @param eventData Event data (optional)
     * @return Operation result
     */
    virtual SCXML::Common::Result<void> raiseEvent(const std::string &eventName, const std::string &eventData = "") = 0;

    /**
     * @brief Cancel delayed event
     * @param sendId Send ID of the event to cancel
     * @return Operation result
     */
    virtual SCXML::Common::Result<void> cancelEvent(const std::string &sendId) = 0;

    // ========== State Information ==========

    /**
     * @brief Get current active state ID
     * @return Current state ID
     */
    virtual std::string getCurrentStateId() const = 0;

    /**
     * @brief Check if state is active
     * @param stateId State ID to check
     * @return true if state is active
     */
    virtual bool isStateActive(const std::string &stateId) const = 0;

    // ========== Expression Evaluation ==========

    /**
     * @brief Evaluate expression in current context
     * @param expression Expression to evaluate
     * @return Evaluation result
     */
    virtual SCXML::Common::Result<std::string> evaluateExpression(const std::string &expression) = 0;

    /**
     * @brief Evaluate boolean condition
     * @param condition Condition to evaluate
     * @return Boolean evaluation result
     */
    virtual SCXML::Common::Result<bool> evaluateCondition(const std::string &condition) = 0;

    // ========== Logging and Diagnostics ==========

    /**
     * @brief Log message with context information
     * @param level Log level (debug, info, warning, error)
     * @param message Log message
     */
    virtual void log(const std::string &level, const std::string &message) = 0;

    /**
     * @brief Get session information
     * @return Session ID or name
     */
    virtual std::string getSessionInfo() const = 0;
};

}  // namespace Model
}  // namespace SCXML
