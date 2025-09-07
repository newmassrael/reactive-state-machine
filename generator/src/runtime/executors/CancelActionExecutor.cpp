#include "runtime/executors/CancelActionExecutor.h"
#include "core/actions/CancelActionNode.h"
#include "runtime/RuntimeContext.h"
#include "common/Logger.h"

namespace SCXML {
namespace Runtime {

bool CancelActionExecutor::execute(const Core::ActionNode& actionNode, RuntimeContext& context) {
    (void)context; // Suppress unused parameter warning
    const auto* cancelNode = safeCast<Core::CancelActionNode>(actionNode);
    if (!cancelNode) {
        SCXML::Common::Logger::error("CancelActionExecutor::execute - Invalid action node type");
        return false;
    }

    try {
        SCXML::Common::Logger::debug("CancelActionExecutor::execute - Processing cancel action: " + cancelNode->getId());
        
        // TODO: Implement actual cancel logic
        // For now, just log the action
        SCXML::Common::Logger::info("CancelActionExecutor::execute - Cancel action executed: " + cancelNode->getId());
        
        return true;
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("CancelActionExecutor::execute - Exception: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> CancelActionExecutor::validate(const Core::ActionNode& actionNode) const {
    std::vector<std::string> errors;

    const auto* cancelNode = safeCast<Core::CancelActionNode>(actionNode);
    if (!cancelNode) {
        errors.push_back("Invalid action node type for CancelActionExecutor");
        return errors;
    }

    // SCXML W3C specification: <cancel> must have either sendid or sendidexpr
    const std::string& sendId = cancelNode->getSendId();
    const std::string& sendIdExpr = cancelNode->getSendIdExpr();
    
    if (sendId.empty() && sendIdExpr.empty()) {
        errors.push_back("Cancel action must have either 'sendid' or 'sendidexpr' attribute");
    } else if (!sendId.empty() && !sendIdExpr.empty()) {
        // Note: SCXML allows both but sendidexpr takes precedence
        // This is not an error, just log it
        SCXML::Common::Logger::debug("CancelActionExecutor::validate - Both sendid and sendidexpr specified, sendidexpr takes precedence");
    }
    
    // sendid cannot be empty string if specified (this check is redundant with above, but kept for clarity)
    // The real check is above: both sendId and sendIdExpr cannot be empty

    return errors;
}

} // namespace Runtime
} // namespace SCXML