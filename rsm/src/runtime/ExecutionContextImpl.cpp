#include "runtime/ExecutionContextImpl.h"
#include "common/Logger.h"

namespace RSM {

ExecutionContextImpl::ExecutionContextImpl(std::shared_ptr<IActionExecutor> executor, const std::string &sessionId)
    : executor_(executor), sessionId_(sessionId) {
    LOG_DEBUG("ExecutionContextImpl created for session: {}", sessionId_);
}

IActionExecutor &ExecutionContextImpl::getActionExecutor() {
    if (!executor_) {
        throw std::runtime_error("Action executor is null");
    }
    return *executor_;
}

std::string ExecutionContextImpl::getCurrentSessionId() const {
    return sessionId_;
}

std::string ExecutionContextImpl::getCurrentEventData() const {
    return currentEventData_;
}

std::string ExecutionContextImpl::getCurrentEventName() const {
    return currentEventName_;
}

std::string ExecutionContextImpl::getCurrentStateId() const {
    return currentStateId_;
}

bool ExecutionContextImpl::isValid() const {
    return executor_ != nullptr && !sessionId_.empty();
}

void ExecutionContextImpl::setCurrentEvent(const std::string &eventName, const std::string &eventData) {
    currentEventName_ = eventName;
    currentEventData_ = eventData;

    LOG_DEBUG("Current event set: {} with data: {}", eventName, eventData);
}

void ExecutionContextImpl::setCurrentStateId(const std::string &stateId) {
    currentStateId_ = stateId;
    LOG_DEBUG("Current state set: {}", stateId);
}

void ExecutionContextImpl::clearCurrentEvent() {
    currentEventName_.clear();
    currentEventData_.clear();
    LOG_DEBUG("{}", "$1");
}

}  // namespace RSM