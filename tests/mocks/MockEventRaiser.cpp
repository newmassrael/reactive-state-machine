#include "MockEventRaiser.h"

namespace RSM {
namespace Test {

MockEventRaiser::MockEventRaiser(std::function<bool(const std::string &, const std::string &)> callback)
    : callback_(std::move(callback)) {}

bool MockEventRaiser::raiseEvent(const std::string &eventName, const std::string &eventData) {
    // Always record the event for testing
    raisedEvents_.emplace_back(eventName, eventData);

    // If callback is set, delegate to it
    if (callback_) {
        return callback_(eventName, eventData);
    }

    // Default behavior: succeed if event name is not empty
    return !eventName.empty();
}

bool MockEventRaiser::raiseEvent(const std::string &eventName, const std::string &eventData,
                                 const std::string & /*originSessionId*/) {
    // W3C SCXML 6.4: Delegate to 2-parameter version (mock doesn't care about origin)
    return raiseEvent(eventName, eventData);
}

bool MockEventRaiser::raiseEvent(const std::string &eventName, const std::string &eventData,
                                 const std::string & /*sendId*/, bool /*unused*/) {
    // W3C SCXML 5.10: Delegate to 2-parameter version (mock doesn't care about sendId)
    return raiseEvent(eventName, eventData);
}

bool MockEventRaiser::raiseEvent(const std::string &eventName, const std::string &eventData,
                                 const std::string & /*originSessionId*/, const std::string & /*invokeId*/) {
    // W3C SCXML 5.10 test 338: Delegate to 2-parameter version (mock doesn't care about invokeId)
    return raiseEvent(eventName, eventData);
}

bool MockEventRaiser::isReady() const {
    return ready_;
}

const std::vector<std::pair<std::string, std::string>> &MockEventRaiser::getRaisedEvents() const {
    return raisedEvents_;
}

void MockEventRaiser::clearEvents() {
    raisedEvents_.clear();
}

int MockEventRaiser::getEventCount() const {
    return static_cast<int>(raisedEvents_.size());
}

void MockEventRaiser::setCallback(std::function<bool(const std::string &, const std::string &)> callback) {
    callback_ = std::move(callback);
}

void MockEventRaiser::setReady(bool ready) {
    ready_ = ready;
}

void MockEventRaiser::setImmediateMode(bool /* immediate */) {
    // Mock implementation - just record for testing
    // Could be extended to track mode changes if needed
}

void MockEventRaiser::processQueuedEvents() {
    // Mock implementation - no actual queue to process
    // Could be extended to simulate queue processing if needed
}

bool MockEventRaiser::processNextQueuedEvent() {
    // Mock implementation - no actual queue to process
    // Return false to indicate no event was processed
    return false;
}

bool MockEventRaiser::hasQueuedEvents() const {
    // Mock implementation - no actual queue
    return false;
}

}  // namespace Test
}  // namespace RSM