#include "actions/RaiseAction.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"

namespace RSM {

RaiseAction::RaiseAction(const std::string &event, const std::string &id) : BaseAction(id), event_(event) {}

bool RaiseAction::execute(IExecutionContext &context) {
    if (!context.isValid()) {
        return false;
    }

    try {
        return context.getActionExecutor().executeRaiseAction(*this);
    } catch (const std::exception &) {
        return false;
    }
}

std::string RaiseAction::getActionType() const {
    return "raise";
}

std::shared_ptr<IActionNode> RaiseAction::clone() const {
    auto cloned = std::make_shared<RaiseAction>(getId());
    cloned->setEvent(event_);
    cloned->setData(data_);
    return cloned;
}

const std::string &RaiseAction::getEvent() const {
    return event_;
}

void RaiseAction::setEvent(const std::string &event) {
    event_ = event;
}

const std::string &RaiseAction::getData() const {
    return data_;
}

void RaiseAction::setData(const std::string &data) {
    data_ = data;
}

std::vector<std::string> RaiseAction::validateSpecific() const {
    std::vector<std::string> errors;

    if (isEmptyString(event_)) {
        errors.push_back("Raise action must have an event name");
    }

    // Data is optional, empty data is valid

    return errors;
}

std::string RaiseAction::getSpecificDescription() const {
    std::string desc = "raise event=\"" + event_ + "\"";
    if (!data_.empty()) {
        desc += " data=\"" + data_ + "\"";
    }
    return desc;
}

}  // namespace RSM