#include "runtime/EventScheduler.h"
#include "common/Logger.h"
#include "events/Event.h"

namespace SCXML {
namespace Runtime {

void EventScheduler::scheduleEvent(std::shared_ptr<SCXML::Events::Event> event, uint64_t delayMs,
                                   const std::string &target, const std::string &sendId) {
    if (!event) {
        SCXML::Common::Logger::error("EventScheduler::scheduleEvent - Null event provided");
        return;
    }

    auto targetTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);

    SCXML::Common::Logger::debug("EventScheduler::scheduleEvent - Scheduling event '" + event->getName() +
                                 "' for delivery in " + std::to_string(delayMs) + "ms");

    delayedEvents_.emplace(targetTime, event, target, sendId);
}

std::vector<EventScheduler::DelayedEvent> EventScheduler::getReadyEvents() {
    std::vector<DelayedEvent> readyEvents;
    auto now = std::chrono::steady_clock::now();

    while (!delayedEvents_.empty() && delayedEvents_.top().targetTime <= now) {
        auto delayedEvent = delayedEvents_.top();
        delayedEvents_.pop();

        SCXML::Common::Logger::debug("EventScheduler::getReadyEvents - Event '" + delayedEvent.event->getName() +
                                     "' is ready for delivery");

        readyEvents.push_back(std::move(delayedEvent));
    }

    if (!readyEvents.empty()) {
        SCXML::Common::Logger::debug("EventScheduler::getReadyEvents - " + std::to_string(readyEvents.size()) +
                                     " events ready");
    }

    return readyEvents;
}

bool EventScheduler::cancelEvent(const std::string &sendId) {
    if (sendId.empty()) {
        return false;
    }

    // Since priority_queue doesn't support removal, we'll need to rebuild it
    std::vector<DelayedEvent> remainingEvents;
    bool found = false;

    while (!delayedEvents_.empty()) {
        auto event = delayedEvents_.top();
        delayedEvents_.pop();

        if (event.sendId == sendId) {
            found = true;
            SCXML::Common::Logger::debug("EventScheduler::cancelEvent - Cancelled event with sendId: " + sendId);
        } else {
            remainingEvents.push_back(event);
        }
    }

    // Rebuild the queue
    for (const auto &event : remainingEvents) {
        delayedEvents_.push(event);
    }

    return found;
}

bool EventScheduler::hasReadyEvents() const {
    if (delayedEvents_.empty()) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    return delayedEvents_.top().targetTime <= now;
}

std::chrono::milliseconds EventScheduler::getTimeUntilNextEvent() const {
    if (delayedEvents_.empty()) {
        return std::chrono::milliseconds::max();
    }

    auto now = std::chrono::steady_clock::now();
    auto nextTime = delayedEvents_.top().targetTime;

    if (nextTime <= now) {
        return std::chrono::milliseconds(0);
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(nextTime - now);
}

}  // namespace Runtime
}  // namespace SCXML