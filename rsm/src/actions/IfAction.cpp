#include "actions/IfAction.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"

namespace RSM {

IfAction::IfAction(const std::string &condition, const std::string &id) : BaseAction(id) {
    if (!condition.empty()) {
        addIfCondition(condition);
    }
}

bool IfAction::execute(IExecutionContext &context) {
    if (!context.isValid()) {
        return false;
    }

    try {
        return context.getActionExecutor().executeIfAction(*this);
    } catch (const std::exception &) {
        return false;
    }
}

std::string IfAction::getActionType() const {
    return "if";
}

std::shared_ptr<IActionNode> IfAction::clone() const {
    auto cloned = std::make_shared<IfAction>(getId());

    // Deep copy branches
    for (const auto &branch : branches_) {
        ConditionalBranch clonedBranch;
        clonedBranch.condition = branch.condition;
        clonedBranch.isElseBranch = branch.isElseBranch;

        // Clone all actions in this branch
        for (const auto &action : branch.actions) {
            if (action) {
                clonedBranch.actions.push_back(action->clone());
            }
        }

        cloned->branches_.push_back(clonedBranch);
    }

    return cloned;
}

void IfAction::addIfCondition(const std::string &condition) {
    ConditionalBranch branch;
    branch.condition = condition;
    branch.isElseBranch = false;
    branches_.push_back(branch);
}

void IfAction::addElseIfCondition(const std::string &condition) {
    ConditionalBranch branch;
    branch.condition = condition;
    branch.isElseBranch = false;
    branches_.push_back(branch);
}

IfAction::ConditionalBranch &IfAction::addElseBranch() {
    ConditionalBranch branch;
    branch.condition = "";
    branch.isElseBranch = true;
    branches_.push_back(branch);
    return branches_.back();
}

void IfAction::addIfAction(std::shared_ptr<IActionNode> action) {
    if (branches_.empty()) {
        // Create a default if branch if none exists
        addIfCondition("true");
    }
    branches_[0].actions.push_back(action);
}

size_t IfAction::getBranchCount() const {
    return branches_.size();
}

const IfAction::ConditionalBranch &IfAction::getBranch(size_t index) const {
    static ConditionalBranch emptyBranch;
    if (index >= branches_.size()) {
        return emptyBranch;
    }
    return branches_[index];
}

const std::vector<IfAction::ConditionalBranch> &IfAction::getBranches() const {
    return branches_;
}

void IfAction::addActionToBranch(size_t branchIndex, std::shared_ptr<IActionNode> action) {
    if (branchIndex < branches_.size() && action) {
        branches_[branchIndex].actions.push_back(action);
    }
}

std::vector<std::string> IfAction::validateSpecific() const {
    std::vector<std::string> errors;

    if (branches_.empty()) {
        errors.push_back("If action must have at least one branch");
        return errors;
    }

    bool hasElse = false;
    for (size_t i = 0; i < branches_.size(); ++i) {
        const auto &branch = branches_[i];

        if (branch.isElseBranch) {
            if (hasElse) {
                errors.push_back("If action can only have one else branch");
            }
            if (i != branches_.size() - 1) {
                errors.push_back("Else branch must be the last branch");
            }
            hasElse = true;
        } else if (isEmptyString(branch.condition)) {
            errors.push_back("Non-else branch must have a condition");
        }
    }

    return errors;
}

std::string IfAction::getSpecificDescription() const {
    std::string desc = "if with " + std::to_string(branches_.size()) + " branch(es)";

    for (size_t i = 0; i < branches_.size(); ++i) {
        const auto &branch = branches_[i];
        desc += " [" + std::to_string(i) + ": ";

        if (branch.isElseBranch) {
            desc += "else";
        } else {
            desc += "condition=\"" + branch.condition + "\"";
        }

        desc += " actions=" + std::to_string(branch.actions.size()) + "]";
    }

    return desc;
}

}  // namespace RSM