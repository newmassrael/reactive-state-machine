#include "core/actions/IfActionNode.h"
#include "common/Logger.h"
#include "runtime/GuardEvaluator.h"
#include "runtime/RuntimeContext.h"
#include "events/Event.h"
#include "model/IStateNode.h"
#include <algorithm>
#include <sstream>

namespace SCXML {
namespace Core {

IfActionNode::IfActionNode(const std::string &id) : ActionNode(id) {
    initializeGuardEvaluator();
    SCXML::Common::Logger::debug("IfActionNode::Constructor - Created conditional action: " + id);
}

void IfActionNode::setIfCondition(const std::string &condition) {
    if (branches_.empty()) {
        branches_.emplace_back(condition);
    } else {
        branches_[0].condition = condition;
        branches_[0].isElseBranch = false;
    }

    SCXML::Common::Logger::debug("IfActionNode::setIfCondition - Set if condition: " + condition);
}

const std::string &IfActionNode::getIfCondition() const {
    if (!branches_.empty() && !branches_[0].isElseBranch) {
        return branches_[0].condition;
    }

    static std::string empty;
    return empty;
}

void IfActionNode::addIfAction(std::shared_ptr<SCXML::Model::IActionNode> action) {
    if (!action) {
        SCXML::Common::Logger::warning("IfActionNode::addIfAction - Null action provided");
        return;
    }

    // Ensure if branch exists
    if (branches_.empty()) {
        branches_.emplace_back("");  // Empty condition will be set later
    }

    branches_[0].actions.push_back(action);
    SCXML::Common::Logger::debug("IfActionNode::addIfAction - Added action to if branch: " + action->getId());
}

IfActionNode::ConditionalBranch &IfActionNode::addElseIfBranch(const std::string &condition) {
    if (condition.empty()) {
        SCXML::Common::Logger::warning("IfActionNode::addElseIfBranch - Empty condition provided");
    }

    // Check if we already have an else branch - elseif must come before else
    if (!branches_.empty() && branches_.back().isElseBranch) {
        SCXML::Common::Logger::error("IfActionNode::addElseIfBranch - Cannot add elseif after else branch");
        return branches_.back();  // Return else branch as error case
    }

    branches_.emplace_back(condition);
    SCXML::Common::Logger::debug("IfActionNode::addElseIfBranch - Added elseif branch with condition: " + condition);
    return branches_.back();
}

IfActionNode::ConditionalBranch &IfActionNode::addElseBranch() {
    // Check if else branch already exists
    if (hasElseBranch()) {
        SCXML::Common::Logger::warning("IfActionNode::addElseBranch - Else branch already exists");
        return branches_.back();
    }

    branches_.emplace_back(true);  // true indicates else branch
    SCXML::Common::Logger::debug("IfActionNode::addElseBranch - Added else branch");
    return branches_.back();
}

void IfActionNode::addActionToBranch(size_t branchIndex, std::shared_ptr<SCXML::Model::IActionNode> action) {
    if (!action) {
        SCXML::Common::Logger::warning("IfActionNode::addActionToBranch - Null action provided");
        return;
    }

    if (branchIndex >= branches_.size()) {
        SCXML::Common::Logger::error("IfActionNode::addActionToBranch - Branch index out of range: " + std::to_string(branchIndex));
        return;
    }

    branches_[branchIndex].actions.push_back(action);
    SCXML::Common::Logger::debug("IfActionNode::addActionToBranch - Added action " + action->getId() + " to branch " +
                  std::to_string(branchIndex));
}

bool IfActionNode::hasElseBranch() const {
    return !branches_.empty() && branches_.back().isElseBranch;
}

bool IfActionNode::execute(::SCXML::Runtime::RuntimeContext &context) {
    if (branches_.empty()) {
        SCXML::Common::Logger::warning("IfActionNode::execute - No branches defined");
        return true;  // Empty if statement is valid but does nothing
    }

    try {
        SCXML::Common::Logger::debug("IfActionNode::execute - Evaluating conditional branches");

        // Find the first matching branch
        size_t matchingBranch = findMatchingBranch(context);

        if (matchingBranch == SIZE_MAX) {
            SCXML::Common::Logger::debug("IfActionNode::execute - No conditions matched, no else branch");
            return true;  // No matching condition and no else branch
        }

        // Execute the matching branch
        const auto &branch = branches_[matchingBranch];

        if (branch.isElseBranch) {
            SCXML::Common::Logger::debug("IfActionNode::execute - Executing else branch");
        } else {
            SCXML::Common::Logger::debug("IfActionNode::execute - Executing branch " + std::to_string(matchingBranch) +
                          " with condition: " + branch.condition);
        }

        bool result = executeBranch(branch, context);

        if (result) {
            SCXML::Common::Logger::debug("IfActionNode::execute - Conditional execution completed successfully");
            return true;
        } else {
            SCXML::Common::Logger::error("IfActionNode::execute - Conditional execution failed");
            return false;
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("IfActionNode::execute - Exception during execution: " + std::string(e.what()));

        // W3C SCXML spec: execution exception should generate error event
        SCXML::Events::Event errorEvent("error.execution", SCXML::Events::Event::Type::PLATFORM);
        errorEvent.setData("If action exception: " + std::string(e.what()));

        // Send error event to internal queue (fire-and-forget)
        context.raiseEvent(std::make_shared<SCXML::Events::Event>(errorEvent));
        return false;
    }
}

std::shared_ptr<SCXML::Model::IActionNode> IfActionNode::clone() const {
    auto cloned = std::make_shared<IfActionNode>(getId());

    // Clone all branches
    for (const auto &branch : branches_) {
        cloned->branches_.push_back(cloneBranch(branch));
    }

    SCXML::Common::Logger::debug("IfActionNode::clone - Cloned conditional action with " + std::to_string(branches_.size()) +
                  " branches");

    return cloned;
}

std::vector<std::string> IfActionNode::validate() const {
    std::vector<std::string> errors;

    // Check if we have at least one branch
    if (branches_.empty()) {
        errors.push_back("If action '" + getId() + "' has no branches defined");
        return errors;
    }

    // Check if first branch has condition (unless it's an else-only case)
    if (!branches_[0].isElseBranch && branches_[0].condition.empty()) {
        errors.push_back("If action '" + getId() + "' has empty condition for if branch");
    }

    // Check for multiple else branches
    size_t elseBranchCount = 0;
    for (size_t i = 0; i < branches_.size(); ++i) {
        const auto &branch = branches_[i];

        if (branch.isElseBranch) {
            elseBranchCount++;

            // Else branch must be last
            if (i != branches_.size() - 1) {
                errors.push_back("If action '" + getId() + "' has else branch not at end");
            }
        } else {
            // Non-else branches after else
            if (elseBranchCount > 0) {
                errors.push_back("If action '" + getId() + "' has conditional branch after else");
            }

            // Check for empty conditions in elseif
            if (i > 0 && branch.condition.empty()) {
                errors.push_back("If action '" + getId() + "' has empty condition for elseif branch " +
                                 std::to_string(i));
            }
        }
    }

    // Check for multiple else branches
    if (elseBranchCount > 1) {
        errors.push_back("If action '" + getId() + "' has multiple else branches");
    }

    // Validate each branch's actions
    for (size_t i = 0; i < branches_.size(); ++i) {
        const auto &branch = branches_[i];

        for (const auto &action : branch.actions) {
            if (!action) {
                errors.push_back("If action '" + getId() + "' has null action in branch " + std::to_string(i));
            }
        }
    }

    return errors;
}

bool IfActionNode::evaluateCondition(const std::string &condition, ::SCXML::Runtime::RuntimeContext &context) {
    if (condition.empty()) {
        SCXML::Common::Logger::warning("IfActionNode::evaluateCondition - Empty condition");
        return false;
    }

    if (!guardEvaluator_) {
        SCXML::Common::Logger::error("IfActionNode::evaluateCondition - Guard evaluator not initialized");
        return false;
    }

    try {
        // Use evaluateExpression instead of evaluateCondition
        SCXML::GuardEvaluator::GuardContext guardContext;
        // Populate guardContext from RuntimeContext
        // Use the provided context parameter
        guardContext.runtimeContext = &context;
        guardContext.currentEvent = context.getCurrentEvent();
        // Get state name instead of state node pointer
        auto currentStateNode = context.getCurrentStateNode();
        guardContext.sourceState = currentStateNode ? currentStateNode->getId() : "";
        // targetState is not applicable for if actions
        auto guardResult = guardEvaluator_->evaluateExpression(condition, guardContext);
        bool result = guardResult.satisfied;
        SCXML::Common::Logger::debug("IfActionNode::evaluateCondition - Condition '" + condition +
                      "' evaluated to: " + (result ? "true" : "false"));
        return result;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("IfActionNode::evaluateCondition - Exception evaluating condition '" + condition +
                      "': " + std::string(e.what()));
        return false;
    }
}

bool IfActionNode::executeBranch(const ConditionalBranch &branch, ::SCXML::Runtime::RuntimeContext &context) {
    if (branch.actions.empty()) {
        SCXML::Common::Logger::debug("IfActionNode::executeBranch - Branch has no actions");
        return true;  // Empty branch is valid
    }

    SCXML::Common::Logger::debug("IfActionNode::executeBranch - Executing " + std::to_string(branch.actions.size()) + " actions");

    for (size_t i = 0; i < branch.actions.size(); ++i) {
        const auto &action = branch.actions[i];

        if (!action) {
            SCXML::Common::Logger::error("IfActionNode::executeBranch - Null action at index " + std::to_string(i));
            return false;
        }

        SCXML::Common::Logger::debug("IfActionNode::executeBranch - Executing action: " + action->getId());

        // Execute the action
        bool result = action->execute(context);
        if (!result) {
            SCXML::Common::Logger::warning("IfActionNode::executeBranch - Action failed: " + action->getId());

            // W3C SCXML spec: action failure should generate error event
            SCXML::Events::Event errorEvent("error.execution", SCXML::Events::Event::Type::PLATFORM);
            errorEvent.setData("If action execution failed: " + action->getId());

            // Send error event to internal queue
            context.raiseEvent(std::make_shared<SCXML::Events::Event>(errorEvent));
            return false;
        }
    }

    SCXML::Common::Logger::debug("IfActionNode::executeBranch - All actions executed successfully");
    return true;
}

size_t IfActionNode::findMatchingBranch(::SCXML::Runtime::RuntimeContext &context) {
    for (size_t i = 0; i < branches_.size(); ++i) {
        const auto &branch = branches_[i];

        if (branch.isElseBranch) {
            // Else branch always matches if reached
            SCXML::Common::Logger::debug("IfActionNode::findMatchingBranch - Else branch matched");
            return i;
        }

        // Evaluate condition for if/elseif branches
        if (evaluateCondition(branch.condition, context)) {
            SCXML::Common::Logger::debug("IfActionNode::findMatchingBranch - Branch " + std::to_string(i) + " matched");
            return i;
        }
    }

    SCXML::Common::Logger::debug("IfActionNode::findMatchingBranch - No branch matched");
    return SIZE_MAX;  // No matching branch found
}

void IfActionNode::initializeGuardEvaluator() {
    try {
        guardEvaluator_ = std::make_shared<SCXML::GuardEvaluator>();
        SCXML::Common::Logger::debug("IfActionNode::initializeGuardEvaluator - Guard evaluator initialized");

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("IfActionNode::initializeGuardEvaluator - Failed to initialize: " + std::string(e.what()));
        guardEvaluator_ = nullptr;
    }
}

IfActionNode::ConditionalBranch IfActionNode::cloneBranch(const ConditionalBranch &original) const {
    ConditionalBranch cloned;
    cloned.condition = original.condition;
    cloned.isElseBranch = original.isElseBranch;

    // Deep clone all actions
    for (const auto &action : original.actions) {
        if (action) {
            // Clone the action properly
            auto clonedAction = action->clone();
            cloned.actions.push_back(clonedAction);
        }
    }

    return cloned;
}

}  // namespace Core
}  // namespace SCXML
