#include "core/actions/IfActionNode.h"
#include "common/Logger.h"
#include "runtime/GuardEvaluator.h"
#include "runtime/RuntimeContext.h"
#include "runtime/ActionExecutor.h"
#include "events/Event.h"
#include "model/IStateNode.h"
#include <algorithm>
#include <sstream>

namespace SCXML {
namespace Core {

IfActionNode::IfActionNode(const std::string &id) : ActionNode(id) {
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
    // Debug: Log current state
    SCXML::Common::Logger::debug("IfActionNode::addElseBranch - Current branch count: " + std::to_string(branches_.size()));
    SCXML::Common::Logger::debug("IfActionNode::addElseBranch - hasElseBranch() = " + std::string(hasElseBranch() ? "true" : "false"));
    
    // Check if else branch already exists
    if (hasElseBranch()) {
        SCXML::Common::Logger::warning("IfActionNode::addElseBranch - Else branch already exists");
        // Find and return the existing else branch
        for (auto& branch : branches_) {
            if (branch.isElseBranch) {
                SCXML::Common::Logger::debug("IfActionNode::addElseBranch - Returning existing else branch");
                return branch;
            }
        }
        // Fallback - should not reach here
        return branches_.back();
    }

    // SCXML W3C 사양 준수: <else>는 반드시 <if> 내부에 있어야 함
    if (branches_.empty()) {
        SCXML::Common::Logger::error("IfActionNode::addElseBranch - SCXML violation: Cannot add else branch without if condition");
        throw std::invalid_argument("Cannot add else branch without if condition. Call setIfCondition() first to comply with SCXML specification.");
    }

    branches_.emplace_back(true);  // true indicates else branch
    branches_.back().isElseBranch = true; // 명시적으로 else 브랜치로 설정
    SCXML::Common::Logger::debug("IfActionNode::addElseBranch - Added else branch (isElseBranch=true), new count: " + std::to_string(branches_.size()));
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
    // Use Executor pattern - create static factory
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        SCXML::Common::Logger::error("IfActionNode::execute - No executor available for action type: " + getActionType());
        return false;
    }

    return executor->execute(*this, context);
}

std::shared_ptr<SCXML::Model::IActionNode> IfActionNode::clone() const {
    auto cloned = std::make_shared<IfActionNode>(getId());

    // Clone all branches
    for (const auto &branch : branches_) {
        ConditionalBranch clonedBranch = branch;
        // Clear the actions first, then clone them properly
        clonedBranch.actions.clear();
        
        // Clone actions in the branch
        for (const auto &action : branch.actions) {
            if (action) {
                clonedBranch.actions.push_back(action->clone());
            }
        }
        cloned->branches_.push_back(clonedBranch);
    }

    SCXML::Common::Logger::debug("IfActionNode::clone - Cloned conditional action with " + std::to_string(branches_.size()) +
                  " branches");

    return cloned;
}

std::vector<std::string> IfActionNode::validate() const {
    // Use Executor pattern - delegate to IfActionExecutor
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        return {"No executor available for action type: " + getActionType()};
    }

    return executor->validate(*this);
}

}  // namespace Core
}  // namespace SCXML