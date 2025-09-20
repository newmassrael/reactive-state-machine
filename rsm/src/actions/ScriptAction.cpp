#include "actions/ScriptAction.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"

namespace RSM {
namespace Actions {

ScriptAction::ScriptAction(const std::string &content, const std::string &id) : BaseAction(id), content_(content) {}

bool ScriptAction::execute(IExecutionContext &context) {
    if (!context.isValid()) {
        return false;
    }

    if (isEmptyString(content_)) {
        // Empty script is considered successful (no-op)
        return true;
    }

    try {
        return context.getActionExecutor().executeScript(content_);
    } catch (const std::exception &) {
        // Exception during execution indicates failure
        return false;
    }
}

std::string ScriptAction::getActionType() const {
    return "script";
}

std::shared_ptr<IActionNode> ScriptAction::clone() const {
    auto cloned = std::make_shared<ScriptAction>(content_, getId());
    return cloned;
}

const std::string &ScriptAction::getContent() const {
    return content_;
}

void ScriptAction::setContent(const std::string &content) {
    content_ = content;
}

bool ScriptAction::isEmpty() const {
    return isEmptyString(content_);
}

std::vector<std::string> ScriptAction::validateSpecific() const {
    std::vector<std::string> errors;

    // Script content validation
    if (isEmptyString(content_)) {
        // Empty script is valid but could be a warning
        // For now, we'll allow it as it's a valid no-op
    }

    // Basic JavaScript syntax validation could be added here
    // For now, we rely on runtime validation

    return errors;
}

std::string ScriptAction::getSpecificDescription() const {
    if (isEmpty()) {
        return "empty script";
    }

    std::string trimmed = trimString(content_);
    if (trimmed.length() > 50) {
        return "script: " + trimmed.substr(0, 47) + "...";
    } else {
        return "script: " + trimmed;
    }
}

}  // namespace Actions
}  // namespace RSM