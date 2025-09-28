#include "StateMachineEventRaiser.h"
#include "common/Logger.h"

namespace RSM {

StateMachineEventRaiser::StateMachineEventRaiser(
    std::function<bool(const std::string &, const std::string &)> eventProcessor)
    : eventProcessor_(std::move(eventProcessor)) {
    LOG_DEBUG("StateMachineEventRaiser: Created with event processor");
}

bool StateMachineEventRaiser::raiseEvent(const std::string &eventName, const std::string &eventData) {
    LOG_DEBUG("StateMachineEventRaiser: Raising event '{}' with data '{}'", eventName, eventData);

    if (eventName.empty()) {
        LOG_ERROR("StateMachineEventRaiser: Cannot raise event with empty name");
        return false;
    }

    if (!eventProcessor_) {
        LOG_ERROR("StateMachineEventRaiser: No event processor available");
        return false;
    }

    try {
        bool result = eventProcessor_(eventName, eventData);
        if (result) {
            LOG_DEBUG("StateMachineEventRaiser: Successfully raised event '{}'", eventName);
        } else {
            LOG_WARN("StateMachineEventRaiser: Event processor returned false for event '{}'", eventName);
        }
        return result;
    } catch (const std::exception &e) {
        LOG_ERROR("StateMachineEventRaiser: Exception while raising event '{}': {}", eventName, e.what());
        return false;
    }
}

bool StateMachineEventRaiser::isReady() const {
    return eventProcessor_ != nullptr;
}

void StateMachineEventRaiser::setImmediateMode(bool immediate) {
    // StateMachineEventRaiser always processes events immediately through the callback
    LOG_DEBUG("StateMachineEventRaiser: setImmediateMode({}) - always immediate", immediate);
}

void StateMachineEventRaiser::processQueuedEvents() {
    // StateMachineEventRaiser doesn't queue events, so this is a no-op
    LOG_DEBUG("StateMachineEventRaiser: processQueuedEvents() - no queue to process");
}

}  // namespace RSM