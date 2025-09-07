#include "runtime/executors/IfActionExecutor.h"
#include "core/actions/IfActionNode.h"
#include "runtime/RuntimeContext.h"
#include "common/Logger.h"

namespace SCXML {
namespace Runtime {

bool IfActionExecutor::execute(const Core::ActionNode& actionNode, RuntimeContext& context) {
    (void)context; // Suppress unused parameter warning
    const auto* ifNode = safeCast<Core::IfActionNode>(actionNode);
    if (!ifNode) {
        SCXML::Common::Logger::error("IfActionExecutor::execute - Invalid action node type");
        return false;
    }

    try {
        SCXML::Common::Logger::debug("IfActionExecutor::execute - Processing if action: " + ifNode->getId());
        
        // TODO: Implement actual if condition logic
        // For now, just log the action
        SCXML::Common::Logger::info("IfActionExecutor::execute - If action executed: " + ifNode->getId());
        
        return true;
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("IfActionExecutor::execute - Exception: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> IfActionExecutor::validate(const Core::ActionNode& actionNode) const {
    std::vector<std::string> errors;

    const auto* ifNode = safeCast<Core::IfActionNode>(actionNode);
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

} // namespace Runtime
} // namespace SCXML