#pragma once

#include <memory>
#include <string>

namespace RSM {

class IActionExecutor;

/**
 * @brief Interface for execution context during action processing
 *
 * This interface provides access to the current execution environment
 * when processing SCXML executable content. It follows the SOLID
 * principle of interface segregation by providing only what's needed
 * for action execution.
 */
class IExecutionContext {
public:
    virtual ~IExecutionContext() = default;

    /**
     * @brief Get action executor for performing operations
     * @return Reference to action executor
     */
    virtual IActionExecutor &getActionExecutor() = 0;

    /**
     * @brief Get current session identifier
     * @return Session ID string
     */
    virtual std::string getCurrentSessionId() const = 0;

    /**
     * @brief Get current event data (from _event variable)
     * @return Current event data as JSON string
     */
    virtual std::string getCurrentEventData() const = 0;

    /**
     * @brief Get current event name
     * @return Current event name string
     */
    virtual std::string getCurrentEventName() const = 0;

    /**
     * @brief Get current active state ID
     * @return Current state identifier
     */
    virtual std::string getCurrentStateId() const = 0;

    /**
     * @brief Check if execution context is in valid state
     * @return true if context is properly initialized
     */
    virtual bool isValid() const = 0;
};

}  // namespace RSM