#include "core/actions/CancelActionNode.h"
#include "common/Logger.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"
#include "runtime/ActionExecutor.h"

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
    // Use Executor pattern - create static factory
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        SCXML::Common::Logger::error("CancelActionNode::execute - No executor available for action type: " + getActionType());
        return false;
    }

    return executor->execute(*this, context);
}

std::shared_ptr<SCXML::Model::IActionNode> CancelActionNode::clone() const {
    auto cloned = std::make_shared<CancelActionNode>(getId());
    cloned->setSendId(sendId_);
    cloned->setSendIdExpr(sendIdExpr_);
    return cloned;
}

std::vector<std::string> CancelActionNode::validate() const {
    // Use Executor pattern - delegate to CancelActionExecutor
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        return {"No executor available for action type: " + getActionType()};
    }

    return executor->validate(*this);
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
