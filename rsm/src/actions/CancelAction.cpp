#include "actions/CancelAction.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"

namespace RSM {

CancelAction::CancelAction(const std::string &sendId, const std::string &id) : BaseAction(id), sendId_(sendId) {}

bool CancelAction::execute(IExecutionContext &context) {
    if (!context.isValid()) {
        return false;
    }

    try {
        return context.getActionExecutor().executeCancelAction(*this);
    } catch (const std::exception &) {
        return false;
    }
}

std::string CancelAction::getActionType() const {
    return "cancel";
}

std::shared_ptr<IActionNode> CancelAction::clone() const {
    // SCXML Compliance: Generate new unique ID for cloned action
    auto cloned = std::make_shared<CancelAction>(sendId_, generateUniqueId("cancel"));
    cloned->sendIdExpr_ = sendIdExpr_;
    return cloned;
}

void CancelAction::setSendId(const std::string &sendId) {
    sendId_ = sendId;
}

const std::string &CancelAction::getSendId() const {
    return sendId_;
}

void CancelAction::setSendIdExpr(const std::string &expr) {
    sendIdExpr_ = expr;
}

const std::string &CancelAction::getSendIdExpr() const {
    return sendIdExpr_;
}

std::vector<std::string> CancelAction::validateSpecific() const {
    std::vector<std::string> errors;

    // Must have either sendid or sendidexpr
    if (sendId_.empty() && sendIdExpr_.empty()) {
        errors.push_back("Cancel action must have either 'sendid' or 'sendidexpr' attribute");
    }

    // Cannot have both sendid and sendidexpr
    if (!sendId_.empty() && !sendIdExpr_.empty()) {
        errors.push_back("Cancel action cannot have both 'sendid' and 'sendidexpr' attributes");
    }

    return errors;
}

std::string CancelAction::getSpecificDescription() const {
    std::string desc = "cancel";

    if (!sendId_.empty()) {
        desc += " sendid='" + sendId_ + "'";
    } else if (!sendIdExpr_.empty()) {
        desc += " sendidexpr='" + sendIdExpr_ + "'";
    }

    return desc;
}

}  // namespace RSM