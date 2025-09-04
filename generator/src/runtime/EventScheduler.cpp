#include "runtime/EventScheduler.h"
#include "common/GracefulJoin.h"
#include "events/Event.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>

using namespace SCXML;
using namespace SCXML::Runtime;

EventScheduler::EventScheduler() : running_(false), context_(nullptr) {}

EventScheduler::~EventScheduler() {
    stop();
}

void EventScheduler::start() {
    if (running_.load()) {
        return;  // Already running
    }

    running_.store(true);
    schedulerThread_ = std::thread(&EventScheduler::schedulerThreadMain, this);
}

void EventScheduler::stop() {
    if (!running_.load()) {
        return;  // Already stopped
    }

    running_.store(false);
    queueCondition_.notify_all();

    if (schedulerThread_.joinable()) {
        Common::GracefulJoin::joinWithTimeout(schedulerThread_, 3, "EventScheduler");
    }

    clearAllEvents();
}

bool EventScheduler::scheduleEvent(const std::string &sendId, SCXML::Events::EventPtr event, uint64_t delayMs,
                                   const std::string &target, SCXML::Runtime::RuntimeContext *context) {
    if (sendId.empty() || !event) {
        return false;
    }

    // Store context for event delivery
    if (!context_) {
        context_ = context;
    }

    auto deliveryTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);

    auto scheduledEvent = std::make_shared<ScheduledEvent>(sendId, event, deliveryTime, target);

    {
        std::lock_guard<std::mutex> lock(queueMutex_);

        // Check for duplicate sendId
        if (eventLookup_.find(sendId) != eventLookup_.end()) {
            // Per SCXML spec, duplicate sendids are allowed but should be unique
            // We'll allow it but log a warning
        }

        eventQueue_.push(scheduledEvent);
        eventLookup_[sendId] = scheduledEvent;
    }

    // Wake up scheduler thread
    queueCondition_.notify_one();

    return true;
}

bool EventScheduler::cancelEvent(const std::string &sendId) {
    std::lock_guard<std::mutex> lock(queueMutex_);

    auto it = eventLookup_.find(sendId);
    if (it != eventLookup_.end()) {
        // Mark as cancelled - don't remove from queue immediately
        // as that would be expensive for priority_queue
        it->second->cancelled = true;
        eventLookup_.erase(it);
        return true;
    }

    return false;  // Event not found (may have already been delivered)
}

bool EventScheduler::isEventScheduled(const std::string &sendId) const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    auto it = eventLookup_.find(sendId);
    return (it != eventLookup_.end()) && !it->second->cancelled;
}

size_t EventScheduler::getPendingEventCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return eventLookup_.size();
}

void EventScheduler::clearAllEvents() {
    std::lock_guard<std::mutex> lock(queueMutex_);

    // Clear priority queue by creating a new empty one
    std::priority_queue<std::shared_ptr<ScheduledEvent>, std::vector<std::shared_ptr<ScheduledEvent>>, EventComparator>
        empty;
    eventQueue_.swap(empty);

    eventLookup_.clear();
}

void EventScheduler::schedulerThreadMain() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(queueMutex_);

        // Wait for events or shutdown
        if (eventQueue_.empty()) {
            queueCondition_.wait(lock, [this] { return !running_.load() || !eventQueue_.empty(); });
            continue;
        }

        // Check if we should stop
        if (!running_.load()) {
            break;
        }

        auto now = std::chrono::steady_clock::now();
        auto nextEvent = eventQueue_.top();

        // Check if it's time to deliver the next event
        if (nextEvent->deliveryTime <= now) {
            eventQueue_.pop();

            // Remove from lookup if not already cancelled
            auto lookupIt = eventLookup_.find(nextEvent->sendId);
            if (lookupIt != eventLookup_.end() && lookupIt->second == nextEvent) {
                eventLookup_.erase(lookupIt);
            }

            lock.unlock();

            // Deliver event if not cancelled
            if (!nextEvent->cancelled) {
                deliverEvent(nextEvent, context_);
            }
        } else {
            // Wait until the next event is ready
            auto waitTime = nextEvent->deliveryTime - now;
            queueCondition_.wait_for(lock, waitTime);
        }
    }
}

void EventScheduler::deliverEvent(std::shared_ptr<ScheduledEvent> scheduledEvent,
                                  SCXML::Runtime::RuntimeContext *context) {
    if (!context || !scheduledEvent || !scheduledEvent->event) {
        return;
    }

    try {
        // Determine delivery method based on target
        if (scheduledEvent->target.empty() || scheduledEvent->target == "#_internal") {
            // Internal event - raise in current context
            context->raiseEvent(scheduledEvent->event);
        } else {
            // External event - send to target
            context->sendEvent(scheduledEvent->event, scheduledEvent->target, 0);
        }
    } catch (const std::exception &e) {
        // Log delivery error but continue processing
        // In a real implementation, this would use proper logging
    }
}