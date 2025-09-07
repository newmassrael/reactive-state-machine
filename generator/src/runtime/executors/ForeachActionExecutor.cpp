#include "runtime/executors/ForeachActionExecutor.h"
#include "core/actions/ForeachActionNode.h"
#include "runtime/RuntimeContext.h"
#include "common/Logger.h"

namespace SCXML {
namespace Runtime {

bool ForeachActionExecutor::execute(const Core::ActionNode& actionNode, RuntimeContext& context) {
    (void)context; // Suppress unused parameter warning
    const auto* foreachNode = safeCast<Core::ForeachActionNode>(actionNode);
    if (!foreachNode) {
        SCXML::Common::Logger::error("ForeachActionExecutor::execute - Invalid action node type");
        return false;
    }

    try {
        SCXML::Common::Logger::debug("ForeachActionExecutor::execute - Processing foreach action: " + foreachNode->getId());
        
        // TODO: Implement actual foreach logic
        // For now, just log the action
        SCXML::Common::Logger::info("ForeachActionExecutor::execute - Foreach action executed: " + foreachNode->getId());
        
        return true;
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("ForeachActionExecutor::execute - Exception: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> ForeachActionExecutor::validate(const Core::ActionNode& actionNode) const {
    std::vector<std::string> errors;

    const auto* foreachNode = safeCast<Core::ForeachActionNode>(actionNode);
    if (!foreachNode) {
        errors.push_back("Invalid action node type for ForeachActionExecutor");
        return errors;
    }

    // SCXML W3C specification: <foreach> must have 'array' attribute (required)
    if (foreachNode->getArray().empty()) {
        errors.push_back("Foreach action must have an 'array' attribute");
    }

    // SCXML W3C specification: <foreach> must have 'item' attribute (required)
    if (foreachNode->getItem().empty()) {
        errors.push_back("Foreach action must have an 'item' attribute");
    }

    // SCXML W3C specification: 'index' attribute is optional
    // No validation required for index attribute

    return errors;
}

} // namespace Runtime
} // namespace SCXML