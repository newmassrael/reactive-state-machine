#include "actions/SendAction.h"
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
    auto cloned = std::make_shared<SendAction>(event_, generateUniqueId("send"));
    cloned->eventExpr_ = eventExpr_;
    cloned->target_ = target_;
    cloned->targetExpr_ = targetExpr_;
    cloned->data_ = data_;
    cloned->delay_ = delay_;
    cloned->delayExpr_ = delayExpr_;
    cloned->sendId_ = sendId_;
    cloned->type_ = type_;
    cloned->params_ = params_;
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

void SendAction::setType(const std::string &type) {
    type_ = type;
}

const std::string &SendAction::getType() const {
    return type_;
}

void SendAction::addParam(const std::string &name, const std::string &value) {
    params_[name] = value;
}

const std::map<std::string, std::string> &SendAction::getParams() const {
    return params_;
}

void SendAction::clearParams() {
    params_.clear();
}

std::chrono::milliseconds SendAction::parseDelayString(const std::string &delayStr) const {
    if (delayStr.empty()) {
        return std::chrono::milliseconds{0};
    }

    // Parse delay formats: "5s", "100ms", "2min", "1h"
    std::regex delayPattern(R"((\d+(?:\.\d+)?)\s*(ms|s|min|h|sec|seconds?|minutes?|hours?)?)");
    std::smatch match;

    if (!std::regex_match(delayStr, match, delayPattern)) {
        return std::chrono::milliseconds{0};  // Invalid format
    }

    double value = std::stod(match[1].str());
    std::string unit = match[2].str();

    // Convert to milliseconds
    if (unit.empty() || unit == "ms") {
        return std::chrono::milliseconds{static_cast<long long>(value)};
    } else if (unit == "s" || unit == "sec" || unit == "seconds" || unit == "second") {
        return std::chrono::milliseconds{static_cast<long long>(value * 1000)};
    } else if (unit == "min" || unit == "minutes" || unit == "minute") {
        return std::chrono::milliseconds{static_cast<long long>(value * 60 * 1000)};
    } else if (unit == "h" || unit == "hours" || unit == "hour") {
        return std::chrono::milliseconds{static_cast<long long>(value * 60 * 60 * 1000)};
    }

    return std::chrono::milliseconds{0};  // Unknown unit
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

    // Validate delay format if provided
    if (!delay_.empty()) {
        auto delayMs = parseDelayString(delay_);
        if (delayMs.count() < 0) {
            errors.push_back("Invalid delay format: " + delay_);
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

    if (!params_.empty()) {
        desc += " params=" + std::to_string(params_.size());
    }

    return desc;
}

}  // namespace RSM