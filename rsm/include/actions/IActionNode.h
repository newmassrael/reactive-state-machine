#pragma once

#include <memory>
#include <string>
#include <vector>

namespace RSM {

class IExecutionContext;

namespace Actions {

/**
 * @brief Interface for all SCXML executable content actions
 *
 * This interface represents the base contract for all executable content
 * elements in SCXML such as <script>, <assign>, <log>, <raise>, etc.
 * It follows the Command pattern for action execution.
 */
class IActionNode {
public:
    virtual ~IActionNode() = default;

    /**
     * @brief Execute this action in the given context
     * @param context Execution context providing access to state machine
     * @return true if action executed successfully
     */
    virtual bool execute(IExecutionContext &context) = 0;

    /**
     * @brief Get the type name of this action
     * @return Action type string (e.g., "script", "assign", "log")
     */
    virtual std::string getActionType() const = 0;

    /**
     * @brief Create a deep copy of this action node
     * @return Shared pointer to cloned action
     */
    virtual std::shared_ptr<IActionNode> clone() const = 0;

    /**
     * @brief Validate action configuration
     * @return Vector of validation error messages (empty if valid)
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Get action identifier (if any)
     * @return Action ID string, empty if no ID
     */
    virtual std::string getId() const = 0;

    /**
     * @brief Set action identifier
     * @param id Action identifier
     */
    virtual void setId(const std::string &id) = 0;

    /**
     * @brief Get human-readable description of action
     * @return Description string for debugging/logging
     */
    virtual std::string getDescription() const = 0;
};

/**
 * @brief Result of action execution
 */
struct ActionResult {
    bool success = false;
    std::string errorMessage;
    std::string actionType;
    std::string actionId;

    ActionResult() = default;

    ActionResult(bool s) : success(s) {}

    ActionResult(bool s, const std::string &error, const std::string &type = "", const std::string &id = "")
        : success(s), errorMessage(error), actionType(type), actionId(id) {}

    explicit operator bool() const {
        return success;
    }
};

}  // namespace Actions
}  // namespace RSM