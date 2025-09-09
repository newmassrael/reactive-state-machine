#pragma once

#include "runtime/ActionExecutor.h"
#include "runtime/DataModelEngine.h"
#include <string>
#include <vector>

namespace SCXML {

// Forward declarations
namespace Core {
class AssignActionNode;
}

namespace Runtime {

/**
 * @brief Executor for Assign Actions
 *
 * Handles the execution of assignment operations in SCXML state machines.
 * Assigns values to data model locations using expressions or attributes.
 */
class AssignActionExecutor : public ActionExecutor {
public:
    /**
     * @brief Execute assign action
     * @param actionNode The assign action node containing configuration
     * @param context Runtime context for execution
     * @return true if assignment was successful
     */
    bool execute(const Core::ActionNode& actionNode, RuntimeContext& context) override;

    /**
     * @brief Get the action type that this executor handles
     * @return "assign"
     */
    std::string getActionType() const override {
        return "assign";
    }

    /**
     * @brief Validate assign action configuration
     * @param actionNode The assign action node to validate
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate(const Core::ActionNode& actionNode) const override;

private:
    /**
     * @brief Resolve the value to assign from expression or attribute
     * @param assignNode The assign action node
     * @param context Runtime context for evaluation
     * @return Resolved value as string, empty if resolution fails
     */
    SCXML::DataModelEngine::DataValue resolveValue(const Core::AssignActionNode& assignNode, RuntimeContext& context) const;
};

} // namespace Runtime
} // namespace SCXML