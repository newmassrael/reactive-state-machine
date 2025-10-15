#include "actions/SendAction.h"
#include "common/SendSchedulingHelper.h"
#include "common/UniqueIdGenerator.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"
#include <regex>

namespace RSM {

SendAction::SendAction(const std::string &event, const std::string &id) : BaseAction(id), event_(event) {}

bool SendAction::execute(IExecutionContext &context) {
    if (!context.isValid()) {
        return false;
    }

    try {
        return context.getActionExecutor().executeSendAction(*this);
    } catch (const std::exception &) {
        return false;
    }
}

std::string SendAction::getActionType() const {
    return "send";
}

std::shared_ptr<IActionNode> SendAction::clone() const {
    // SCXML Compliance: Generate new unique ID for cloned action
    auto cloned = std::make_shared<SendAction>(event_, UniqueIdGenerator::generateActionId("send"));
    cloned->eventExpr_ = eventExpr_;
    cloned->target_ = target_;
    cloned->targetExpr_ = targetExpr_;
    cloned->data_ = data_;
    cloned->delay_ = delay_;
    cloned->delayExpr_ = delayExpr_;
    cloned->sendId_ = sendId_;
    cloned->type_ = type_;
    cloned->typeExpr_ = typeExpr_;
    cloned->paramsWithExpr_ = paramsWithExpr_;
    cloned->content_ = content_;
    return cloned;
}

void SendAction::setEvent(const std::string &event) {
    event_ = event;
}

const std::string &SendAction::getEvent() const {
    return event_;
}

void SendAction::setEventExpr(const std::string &eventExpr) {
    eventExpr_ = eventExpr;
}

const std::string &SendAction::getEventExpr() const {
    return eventExpr_;
}

void SendAction::setTarget(const std::string &target) {
    target_ = target;
}

const std::string &SendAction::getTarget() const {
    return target_;
}

void SendAction::setTargetExpr(const std::string &targetExpr) {
    targetExpr_ = targetExpr;
}

const std::string &SendAction::getTargetExpr() const {
    return targetExpr_;
}

void SendAction::setData(const std::string &data) {
    data_ = data;
}

const std::string &SendAction::getData() const {
    return data_;
}

void SendAction::setDelay(const std::string &delay) {
    delay_ = delay;
}

const std::string &SendAction::getDelay() const {
    return delay_;
}

void SendAction::setDelayExpr(const std::string &delayExpr) {
    delayExpr_ = delayExpr;
}

const std::string &SendAction::getDelayExpr() const {
    return delayExpr_;
}

void SendAction::setSendId(const std::string &sendId) {
    sendId_ = sendId;
}

const std::string &SendAction::getSendId() const {
    return sendId_;
}

void SendAction::setIdLocation(const std::string &idLocation) {
    idLocation_ = idLocation;
}

const std::string &SendAction::getIdLocation() const {
    return idLocation_;
}

void SendAction::setType(const std::string &type) {
    type_ = type;
}

const std::string &SendAction::getType() const {
    return type_;
}

void SendAction::setTypeExpr(const std::string &typeExpr) {
    typeExpr_ = typeExpr;
}

const std::string &SendAction::getTypeExpr() const {
    return typeExpr_;
}

void SendAction::setNamelist(const std::string &namelist) {
    namelist_ = namelist;
}

const std::string &SendAction::getNamelist() const {
    return namelist_;
}

void SendAction::addParamWithExpr(const std::string &name, const std::string &expr) {
    paramsWithExpr_.emplace_back(name, expr);
}

const std::vector<SendAction::SendParam> &SendAction::getParamsWithExpr() const {
    return paramsWithExpr_;
}

void SendAction::clearParams() {
    paramsWithExpr_.clear();
}

void SendAction::setContent(const std::string &content) {
    content_ = content;
}

const std::string &SendAction::getContent() const {
    return content_;
}

std::vector<std::string> SendAction::validateSpecific() const {
    std::vector<std::string> errors;

    // Must have either event or eventexpr
    if (event_.empty() && eventExpr_.empty()) {
        errors.push_back("Send action must have either 'event' or 'eventexpr' attribute");
    }

    // Cannot have both event and eventexpr
    if (!event_.empty() && !eventExpr_.empty()) {
        errors.push_back("Send action cannot have both 'event' and 'eventexpr' attributes");
    }

    // Cannot have both target and targetexpr
    if (!target_.empty() && !targetExpr_.empty() && target_ != "#_internal") {
        errors.push_back("Send action cannot have both 'target' and 'targetexpr' attributes");
    }

    // Cannot have both delay and delayexpr
    if (!delay_.empty() && !delayExpr_.empty()) {
        errors.push_back("Send action cannot have both 'delay' and 'delayexpr' attributes");
    }

    // W3C SCXML 6.2: Cannot have both type and typeexpr
    if (!type_.empty() && !typeExpr_.empty()) {
        errors.push_back("Send action cannot have both 'type' and 'typeexpr' attributes");
    }

    // Validate delay format if provided
    if (!delay_.empty()) {
        auto delayMs = SendSchedulingHelper::parseDelayString(delay_);
        if (delayMs.count() < 0) {
            errors.push_back("Invalid delay format: " + delay_);
        }
    }

    // W3C SCXML C.2: Validate content size to prevent DoS attacks
    if (!content_.empty()) {
        constexpr size_t MAX_CONTENT_SIZE = 10485760;  // 10MB
        if (content_.size() > MAX_CONTENT_SIZE) {
            errors.push_back("Content size exceeds maximum allowed: " + std::to_string(MAX_CONTENT_SIZE) + " bytes");
        }
    }

    return errors;
}

std::string SendAction::getSpecificDescription() const {
    std::string desc = "send";

    if (!event_.empty()) {
        desc += " event='" + event_ + "'";
    } else if (!eventExpr_.empty()) {
        desc += " eventexpr='" + eventExpr_ + "'";
    }

    if (!target_.empty() && target_ != "#_internal") {
        desc += " target='" + target_ + "'";
    } else if (!targetExpr_.empty()) {
        desc += " targetexpr='" + targetExpr_ + "'";
    }

    if (!delay_.empty()) {
        desc += " delay='" + delay_ + "'";
    } else if (!delayExpr_.empty()) {
        desc += " delayexpr='" + delayExpr_ + "'";
    }

    if (!sendId_.empty()) {
        desc += " sendid='" + sendId_ + "'";
    }

    if (!type_.empty()) {
        desc += " type='" + type_ + "'";
    } else if (!typeExpr_.empty()) {
        desc += " typeexpr='" + typeExpr_ + "'";
    }

    if (!paramsWithExpr_.empty()) {
        desc += " params=" + std::to_string(paramsWithExpr_.size());
    }

    // W3C SCXML C.2: Include content information for debugging
    if (!content_.empty()) {
        std::string contentPreview = content_.substr(0, 50);
        if (content_.size() > 50) {
            contentPreview += "...";
        }
        desc += " content='" + contentPreview + "'";
    }

    return desc;
}

}  // namespace RSM