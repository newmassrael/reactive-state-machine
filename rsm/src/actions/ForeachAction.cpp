#include "actions/ForeachAction.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"

namespace RSM {

ForeachAction::ForeachAction(const std::string &array, const std::string &item, const std::string &index,
                             const std::string &id)
    : BaseAction(id), array_(array), item_(item), index_(index) {}

bool ForeachAction::execute(IExecutionContext &context) {
    if (!context.isValid()) {
        return false;
    }

    try {
        return context.getActionExecutor().executeForeachAction(*this);
    } catch (const std::exception &) {
        return false;
    }
}

std::string ForeachAction::getActionType() const {
    return "foreach";
}

std::shared_ptr<IActionNode> ForeachAction::clone() const {
    auto cloned = std::make_shared<ForeachAction>(array_, item_, index_, getId());

    // Deep copy iteration actions
    cloned->iterationActions_ = cloneIterationActions(iterationActions_);

    return cloned;
}

void ForeachAction::setArray(const std::string &array) {
    array_ = array;
}

const std::string &ForeachAction::getArray() const {
    return array_;
}

void ForeachAction::setItem(const std::string &item) {
    item_ = item;
}

const std::string &ForeachAction::getItem() const {
    return item_;
}

void ForeachAction::setIndex(const std::string &index) {
    index_ = index;
}

const std::string &ForeachAction::getIndex() const {
    return index_;
}

void ForeachAction::addIterationAction(std::shared_ptr<IActionNode> action) {
    if (action) {
        iterationActions_.push_back(action);
    }
}

const std::vector<std::shared_ptr<IActionNode>> &ForeachAction::getIterationActions() const {
    return iterationActions_;
}

void ForeachAction::clearIterationActions() {
    iterationActions_.clear();
}

size_t ForeachAction::getIterationActionCount() const {
    return iterationActions_.size();
}

std::vector<std::string> ForeachAction::validateSpecific() const {
    std::vector<std::string> errors;

    // SCXML W3C specification: 'array' attribute is required
    if (isEmptyString(array_)) {
        errors.push_back("Foreach action must have an 'array' attribute with valid expression");
    }

    // SCXML W3C specification: 'item' attribute is required
    if (isEmptyString(item_)) {
        errors.push_back("Foreach action must have an 'item' attribute with valid variable name");
    }

    // Validate item variable name format (basic validation)
    if (!item_.empty()) {
        // Check if item starts with valid character (letter or underscore)
        char firstChar = item_[0];
        if (!std::isalpha(firstChar) && firstChar != '_') {
            errors.push_back("Item variable name must start with letter or underscore: " + item_);
        }

        // Check for invalid characters
        for (char c : item_) {
            if (!std::isalnum(c) && c != '_') {
                errors.push_back("Item variable name contains invalid characters: " + item_);
                break;
            }
        }
    }

    // Validate index variable name format if provided (optional)
    if (!index_.empty()) {
        // Check if index starts with valid character
        char firstChar = index_[0];
        if (!std::isalpha(firstChar) && firstChar != '_') {
            errors.push_back("Index variable name must start with letter or underscore: " + index_);
        }

        // Check for invalid characters
        for (char c : index_) {
            if (!std::isalnum(c) && c != '_') {
                errors.push_back("Index variable name contains invalid characters: " + index_);
                break;
            }
        }

        // Check that item and index are different
        if (item_ == index_) {
            errors.push_back("Item and index variable names must be different");
        }
    }

    // W3C SCXML Note: Empty foreach is allowed (see test150.txml)
    // foreach can be used for variable declaration without child actions

    // Validate each iteration action
    for (size_t i = 0; i < iterationActions_.size(); ++i) {
        const auto &action = iterationActions_[i];
        if (!action) {
            errors.push_back("Iteration action " + std::to_string(i) + " is null");
            continue;
        }

        // Validate the child action
        auto childErrors = action->validate();
        for (const auto &error : childErrors) {
            errors.push_back("Iteration action " + std::to_string(i) + ": " + error);
        }
    }

    return errors;
}

std::string ForeachAction::getSpecificDescription() const {
    std::string desc = "foreach over '" + array_ + "' as '" + item_ + "'";

    if (!index_.empty()) {
        desc += " with index '" + index_ + "'";
    }

    desc += " (" + std::to_string(iterationActions_.size()) + " actions per iteration)";

    if (!iterationActions_.empty()) {
        desc += " [";
        for (size_t i = 0; i < iterationActions_.size(); ++i) {
            if (i > 0) {
                desc += ", ";
            }
            const auto &action = iterationActions_[i];
            if (action) {
                desc += action->getActionType();
            } else {
                desc += "null";
            }
        }
        desc += "]";
    }

    return desc;
}

std::vector<std::shared_ptr<IActionNode>>
ForeachAction::cloneIterationActions(const std::vector<std::shared_ptr<IActionNode>> &source) const {
    std::vector<std::shared_ptr<IActionNode>> cloned;
    cloned.reserve(source.size());

    for (const auto &action : source) {
        if (action) {
            cloned.push_back(action->clone());
        } else {
            cloned.push_back(nullptr);  // Preserve null entries for consistency
        }
    }

    return cloned;
}

}  // namespace RSM