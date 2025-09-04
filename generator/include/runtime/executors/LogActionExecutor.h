#pragma once

#include "runtime/ActionExecutor.h"
#include <string>
#include <vector>

namespace SCXML {

// Forward declarations
namespace Core {
class LogActionNode;
}

namespace Runtime {

/**
 * @brief Executor for Log Actions
 *
 * Handles the execution of logging operations in SCXML state machines.
 * Logs messages with optional labels and severity levels.
 */
class LogActionExecutor : public ActionExecutor {
public:
    /**
     * @brief Execute log action
     * @param actionNode The log action node containing configuration
     * @param context Runtime context for execution
     * @return true if logging was successful
     */
    bool execute(const Core::ActionNode& actionNode, RuntimeContext& context) override;

    /**
     * @brief Get the action type that this executor handles
     * @return "log"
     */
    std::string getActionType() const override {
        return "log";
    }

    /**
     * @brief Validate log action configuration
     * @param actionNode The log action node to validate
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate(const Core::ActionNode& actionNode) const override;

private:
    /**
     * @brief Resolve the log message from expression
     * @param logNode The log action node
     * @param context Runtime context for evaluation
     * @return Resolved message as string, empty if resolution fails
     */
    std::string resolveLogMessage(const Core::LogActionNode& logNode, RuntimeContext& context) const;

    /**
     * @brief Format log message with label prefix
     * @param logNode The log action node containing label
     * @param message The resolved message to format
     * @return Formatted message with label prefix if applicable
     */
    std::string formatLogMessage(const Core::LogActionNode& logNode, const std::string& message) const;
};

} // namespace Runtime
} // namespace SCXML