#include "runtime/executors/ScriptActionExecutor.h"
#include "core/actions/ScriptActionNode.h"
#include "runtime/RuntimeContext.h"
#include "common/Logger.h"

namespace SCXML {
namespace Runtime {

bool ScriptActionExecutor::execute(const Core::ActionNode& actionNode, RuntimeContext& context) {
    (void)context; // Suppress unused parameter warning
    const auto* scriptNode = safeCast<Core::ScriptActionNode>(actionNode);
    if (!scriptNode) {
        SCXML::Common::Logger::error("ScriptActionExecutor::execute - Invalid action node type");
        return false;
    }

    try {
        SCXML::Common::Logger::debug("ScriptActionExecutor::execute - Processing script action: " + scriptNode->getId());
        
        // TODO: Implement actual script execution logic
        // For now, just log the action
        SCXML::Common::Logger::info("ScriptActionExecutor::execute - Script action executed: " + scriptNode->getId());
        
        return true;
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("ScriptActionExecutor::execute - Exception: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> ScriptActionExecutor::validate(const Core::ActionNode& actionNode) const {
    std::vector<std::string> errors;

    const auto* scriptNode = safeCast<Core::ScriptActionNode>(actionNode);
    if (!scriptNode) {
        errors.push_back("Invalid action node type for ScriptActionExecutor");
        return errors;
    }

    // SCXML W3C specification: <script> must have either 'src' or content (but not both)
    const std::string& src = scriptNode->getSrc();
    const std::string& content = scriptNode->getContent();
    
    if (src.empty() && content.empty()) {
        errors.push_back("Script action must have either 'src' or inline content");
    } else if (!src.empty() && !content.empty()) {
        errors.push_back("Script action cannot have both 'src' and inline content - they are mutually exclusive");
    }

    return errors;
}

} // namespace Runtime
} // namespace SCXML