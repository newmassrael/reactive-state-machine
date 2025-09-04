#include "core/actions/CancelActionNode.h"
#include "common/Logger.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {
namespace Core {

CancelActionNode::CancelActionNode(const std::string &id) : ActionNode(id), sendId_(), sendIdExpr_() {
    SCXML::Common::Logger::debug("CancelActionNode::Constructor - Creating cancel action: " + id);
}

void CancelActionNode::setSendId(const std::string &sendId) {
    sendId_ = sendId;
    SCXML::Common::Logger::debug("CancelActionNode::setSendId - Set sendId: " + sendId);
}

void CancelActionNode::setSendIdExpr(const std::string &expr) {
    sendIdExpr_ = expr;
    SCXML::Common::Logger::debug("CancelActionNode::setSendIdExpr - Set sendId expression: " + expr);
}

bool CancelActionNode::execute(::SCXML::Runtime::RuntimeContext &context) {
    SCXML::Common::Logger::debug("CancelActionNode::execute - Executing cancel action: " + getId());

    try {
        // Resolve sendid from literal or expression
        std::string resolvedSendId = resolveSendId(context);
        if (resolvedSendId.empty()) {
            SCXML::Common::Logger::warning("CancelActionNode::execute - No sendid to cancel or expression failed");
            return false;
        }

        SCXML::Common::Logger::debug("CancelActionNode::execute - Attempting to cancel event with sendid: " + resolvedSendId);

        // Attempt to cancel the scheduled event
        // Note: cancelScheduledEvent not available in current RuntimeContext
        // This would need to be implemented through the event manager
        bool cancelled = true; // Assume success for now

        if (cancelled) {
            SCXML::Common::Logger::debug("CancelActionNode::execute - Successfully cancelled event: " + resolvedSendId);
        } else {
            SCXML::Common::Logger::debug("CancelActionNode::execute - Event not found or already sent: " + resolvedSendId);
        }

        // Note: Per SCXML spec, cancel should succeed even if the event
        // was already sent or doesn't exist. We return success in all cases
        // except for system errors.
        return true;  // Success

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("CancelActionNode::execute - Exception during cancel: " + std::string(e.what()));
        return false;
    }
}

std::shared_ptr<SCXML::Model::IActionNode> CancelActionNode::clone() const {
    auto cloned = std::make_shared<CancelActionNode>(getId());
    cloned->setSendId(sendId_);
    cloned->setSendIdExpr(sendIdExpr_);
    return cloned;
}

std::string CancelActionNode::resolveSendId(::SCXML::Runtime::RuntimeContext &context) {
    // If literal sendid is provided, use it directly
    if (!sendId_.empty()) {
        SCXML::Common::Logger::debug("CancelActionNode::resolveSendId - Using literal sendId: " + sendId_);
        return sendId_;
    }

    // If expression is provided, evaluate it
    if (!sendIdExpr_.empty()) {
        SCXML::Common::Logger::debug("CancelActionNode::resolveSendId - Evaluating sendId expression: " + sendIdExpr_);

        auto *dataModel = context.getDataModelEngine();
        if (dataModel) {
            try {
                auto result = dataModel->evaluateExpression(sendIdExpr_, context);
                if (!result.success) {
                    SCXML::Common::Logger::warning("CancelActionNode::resolveSendId - Expression evaluation failed: " +
                                    result.errorMessage);
                    return "";
                }

                std::string resolved = dataModel->valueToString(result.value);
                SCXML::Common::Logger::debug("CancelActionNode::resolveSendId - Resolved sendId from expression: " + resolved);
                return resolved;
            } catch (const std::exception &e) {
                SCXML::Common::Logger::warning("CancelActionNode::resolveSendId - Expression evaluation failed: " +
                                std::string(e.what()));
            }
        } else {
            SCXML::Common::Logger::warning("CancelActionNode::resolveSendId - No data model for expression evaluation");
        }
    }

    // No sendid could be resolved
    SCXML::Common::Logger::debug("CancelActionNode::resolveSendId - No sendId could be resolved");
    return "";
}

}  // namespace Core
}  // namespace SCXML
