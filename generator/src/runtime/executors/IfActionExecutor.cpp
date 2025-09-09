#include "runtime/executors/IfActionExecutor.h"
#include "common/Logger.h"
#include "core/actions/IfActionNode.h"
#include "runtime/RuntimeContext.h"
#include "runtime/executors/AssignActionExecutor.h"
#include "runtime/executors/LogActionExecutor.h"
#include "runtime/executors/RaiseActionExecutor.h"
#include "runtime/executors/SendActionExecutor.h"

namespace SCXML {
namespace Runtime {

bool IfActionExecutor::execute(const Core::ActionNode &actionNode, RuntimeContext &context) {
    // Debug: Check what type we actually received
    SCXML::Common::Logger::error("IfActionExecutor::execute - Received actionNode with ID: " + actionNode.getId());
    SCXML::Common::Logger::error("IfActionExecutor::execute - ActionNode type: " + actionNode.getActionType());

    const auto *ifNode = safeCast<Core::IfActionNode>(actionNode);
    if (!ifNode) {
        SCXML::Common::Logger::error("IfActionExecutor::execute - safeCast failed! Expected IfActionNode but got: " +
                                     actionNode.getActionType());
        return false;
    }

    try {
        SCXML::Common::Logger::debug("IfActionExecutor::execute - Processing if action: " + ifNode->getId());

        // Get all conditional branches (if, elseif, else)
        const auto &branches = ifNode->getBranches();
        if (branches.empty()) {
            SCXML::Common::Logger::warning("IfActionExecutor::execute - No branches found in if action");
            return true;  // Empty if is technically valid
        }

        // Evaluate conditions and execute first matching branch
        for (const auto &branch : branches) {
            bool shouldExecute = false;

            if (branch.isElseBranch) {
                // Else branch - always execute if reached
                shouldExecute = true;
                SCXML::Common::Logger::debug("IfActionExecutor::execute - Executing else branch");
            } else if (branch.condition.empty()) {
                // Empty condition - treat as true (shouldn't happen for non-else branches)
                shouldExecute = true;
                SCXML::Common::Logger::debug("IfActionExecutor::execute - Executing branch with empty condition");
            } else {
                // Evaluate condition using RuntimeContext
                shouldExecute = context.evaluateCondition(branch.condition);
                SCXML::Common::Logger::debug("IfActionExecutor::execute - Condition '" + branch.condition +
                                             "' evaluated to: " + (shouldExecute ? "true" : "false"));
            }

            if (shouldExecute) {
                // Execute all actions in this branch
                SCXML::Common::Logger::debug("IfActionExecutor::execute - Executing " +
                                             std::to_string(branch.actions.size()) + " actions in branch");

                bool allActionsSucceeded = true;
                for (const auto &action : branch.actions) {
                    if (action) {
                        // Execute the action using ActionProcessor or directly
                        // We need to get the ActionExecutor for this action type
                        if (!executeNestedAction(action, context)) {
                            SCXML::Common::Logger::error(
                                "IfActionExecutor::execute - Failed to execute nested action: " + action->getId());
                            allActionsSucceeded = false;
                            // Continue executing other actions but track failure
                        }
                    }
                }

                // Return result - we only execute the first matching branch
                std::string resultMsg = "IfActionExecutor::execute - If action completed, branch executed: ";
                resultMsg += (allActionsSucceeded ? "successfully" : "with errors");
                SCXML::Common::Logger::debug(resultMsg);
                return allActionsSucceeded;
            }
        }

        // No condition matched (no else branch)
        SCXML::Common::Logger::debug("IfActionExecutor::execute - No conditions matched, no actions executed");
        return true;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("IfActionExecutor::execute - Exception: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> IfActionExecutor::validate(const Core::ActionNode &actionNode) const {
    std::vector<std::string> errors;

    const auto *ifNode = safeCast<Core::IfActionNode>(actionNode);
    if (!ifNode) {
        errors.push_back("Invalid action node type for IfActionExecutor");
        return errors;
    }

    // SCXML W3C specification: <if> must have 'cond' attribute (condition is required)
    if (ifNode->getIfCondition().empty()) {
        errors.push_back("If action must have a 'cond' attribute with a valid condition");
    }

    // SCXML W3C specification allows <elseif> and <else> branches (no additional validation needed)
    // The structure validation (e.g., else comes last) is handled in the IfActionNode itself

    return errors;
}

bool IfActionExecutor::executeNestedAction(std::shared_ptr<SCXML::Model::IActionNode> action, RuntimeContext &context) {
    if (!action) {
        return false;
    }

    try {
        // Cast to ActionNode to use the ActionExecutor system
        auto actionNode = std::dynamic_pointer_cast<SCXML::Core::ActionNode>(action);
        if (!actionNode) {
            SCXML::Common::Logger::error("IfActionExecutor::executeNestedAction - Invalid action node type");
            return false;
        }

        // Get the action type and create appropriate executor
        std::string actionType = actionNode->getActionType();

        // Use factory pattern to get the right executor
        if (actionType == "assign") {
            AssignActionExecutor assignExecutor;
            return assignExecutor.execute(*actionNode, context);
        } else if (actionType == "raise") {
            RaiseActionExecutor raiseExecutor;
            return raiseExecutor.execute(*actionNode, context);
        } else if (actionType == "log") {
            LogActionExecutor logExecutor;
            return logExecutor.execute(*actionNode, context);
        } else if (actionType == "send") {
            SendActionExecutor sendExecutor;
            return sendExecutor.execute(*actionNode, context);
        } else if (actionType == "if") {
            // Nested if - recursive call
            IfActionExecutor ifExecutor;
            return ifExecutor.execute(*actionNode, context);
        } else {
            SCXML::Common::Logger::warning("IfActionExecutor::executeNestedAction - Unsupported action type: " +
                                           actionType);
            return false;
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("IfActionExecutor::executeNestedAction - Exception: " + std::string(e.what()));
        return false;
    }
}

}  // namespace Runtime
}  // namespace SCXML