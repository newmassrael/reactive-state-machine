#include "W3CTestEventDispatcher.h"
#include "events/EventDescriptor.h"
#include "events/EventSchedulerImpl.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>
#include <thread>

namespace RSM::W3C {

W3CTestEventDispatcher::W3CTestEventDispatcher(const std::string &sessionId, std::shared_ptr<IEventScheduler> scheduler)
    : sessionId_(sessionId), scheduler_(scheduler) {
    LOG_DEBUG("W3CTestEventDispatcher created for session: {} (W3C compliance mode with delay support)", sessionId_);

    // Create EventScheduler if not provided
    if (!scheduler_) {
        auto executionCallback = [this](const EventDescriptor &event, std::shared_ptr<IEventTarget> target,
                                        const std::string &sendId) -> bool {
            // W3C test environment: store parameters and execute immediately
            lastEventParams_[event.eventName] = event.params;

            // If target is provided, execute the event on the target
            if (target) {
                try {
                    auto result = target->send(event);
                    // For W3C tests, we don't need to wait for the future
                    LOG_INFO("W3CTestEventDispatcher: Scheduled event '{}' sent to target with sendId '{}'",
                             event.eventName, sendId);
                } catch (const std::exception &e) {
                    LOG_ERROR("W3CTestEventDispatcher: Error sending event '{}' to target: {}", event.eventName,
                              e.what());
                    return false;
                }
            } else {
                LOG_INFO("W3CTestEventDispatcher: Scheduled event '{}' executed without target (sendId: '{}')",
                         event.eventName, sendId);
            }

            return true;
        };
        scheduler_ = std::make_shared<EventSchedulerImpl>(executionCallback);
    }
}

std::future<SendResult> W3CTestEventDispatcher::sendEvent(const EventDescriptor &event) {
    LOG_DEBUG("W3CTestEventDispatcher: Sending event '{}' with target '{}'", event.eventName, event.target);

    try {
        // W3C SCXML 6.2: Check if this is a delayed event
        if (event.delay.count() > 0) {
            LOG_DEBUG("W3CTestEventDispatcher: Event '{}' has delay {}ms - scheduling for W3C compliance",
                      event.eventName, event.delay.count());
            return sendEventDelayed(event, event.delay);
        }

        // Execute immediately for non-delayed events
        return executeEventImmediately(event);

    } catch (const std::exception &e) {
        LOG_ERROR("W3CTestEventDispatcher: Error sending event '{}': {}", event.eventName, e.what());

        std::promise<SendResult> errorPromise;
        errorPromise.set_value(SendResult::error("W3C test event dispatch failed: " + std::string(e.what()),
                                                 SendResult::ErrorType::INTERNAL_ERROR));
        return errorPromise.get_future();
    }
}

bool W3CTestEventDispatcher::cancelEvent(const std::string &sendId) {
    // REFACTOR: Delegate to EventScheduler instead of duplicate logic
    bool cancelled = scheduler_->cancelEvent(sendId);

    if (cancelled) {
        LOG_DEBUG("W3CTestEventDispatcher: Successfully cancelled event with sendId: {} (W3C SCXML 6.2 compliance)",
                  sendId);
    } else {
        LOG_DEBUG("W3CTestEventDispatcher: Event with sendId '{}' not found or already cancelled", sendId);
    }

    return cancelled;
}

std::future<SendResult> W3CTestEventDispatcher::sendEventDelayed(const EventDescriptor &event,
                                                                 std::chrono::milliseconds delay) {
    LOG_DEBUG("W3CTestEventDispatcher: Scheduling delayed event '{}' with {}ms delay (W3C compliance mode)",
              event.eventName, delay.count());

    try {
        // W3C SCXML 6.2: Store evaluated parameters immediately (mandatory compliance)
        // Parameters MUST be evaluated at send time, not at dispatch time
        lastEventParams_[event.eventName] = event.params;

        // REFACTOR: Use EventScheduler instead of duplicate implementation
        // W3C tests can use nullptr target as EventScheduler handles it via callback
        auto future = scheduler_->scheduleEvent(event, delay, nullptr, "", event.sessionId);
        std::string sendId = future.get();  // Get the assigned sendId

        LOG_DEBUG("W3CTestEventDispatcher: Event '{}' scheduled with sendId '{}' for W3C compliance testing",
                  event.eventName, sendId);

        // Return success immediately (W3C SCXML fire-and-forget semantics)
        std::promise<SendResult> successPromise;
        successPromise.set_value(SendResult::success(sendId));
        return successPromise.get_future();

    } catch (const std::exception &e) {
        LOG_ERROR("W3CTestEventDispatcher: Error scheduling delayed event '{}': {}", event.eventName, e.what());

        std::promise<SendResult> errorPromise;
        errorPromise.set_value(SendResult::error("Delayed event scheduling failed: " + std::string(e.what()),
                                                 SendResult::ErrorType::INTERNAL_ERROR));
        return errorPromise.get_future();
    }
}

bool W3CTestEventDispatcher::isEventPending(const std::string &sendId) const {
    // REFACTOR: Use EventScheduler instead of duplicate logic
    return scheduler_->hasEvent(sendId);
}

std::string W3CTestEventDispatcher::getStatistics() const {
    // REFACTOR: Use EventScheduler statistics instead of duplicate logic
    size_t pendingCount = scheduler_->getScheduledEventCount();

    std::ostringstream stats;
    stats << "W3CTestEventDispatcher [Session: " << sessionId_
          << "] - Status: Active, Mode: W3C Compliance, Pending: " << pendingCount
          << ", Scheduler: " << (scheduler_->isRunning() ? "Running" : "Stopped");
    return stats.str();
}

void W3CTestEventDispatcher::shutdown() {
    LOG_DEBUG("W3CTestEventDispatcher: Shutting down for session: {} (W3C SCXML 6.2: cancelling all pending events)",
              sessionId_);

    // REFACTOR: Use EventScheduler shutdown instead of duplicate logic
    if (scheduler_) {
        scheduler_->shutdown(true);
    }

    LOG_INFO("W3CTestEventDispatcher: Shutdown complete for W3C compliance");
}

size_t W3CTestEventDispatcher::cancelEventsForSession(const std::string &sessionId) {
    // REFACTOR: Use EventScheduler instead of duplicate logic
    size_t cancelledCount = scheduler_->cancelEventsForSession(sessionId);

    LOG_INFO("W3CTestEventDispatcher: Cancelled {} events for session '{}' (W3C SCXML 6.2 compliance)", cancelledCount,
             sessionId);

    return cancelledCount;
}

std::map<std::string, std::string> W3CTestEventDispatcher::getLastEventParams(const std::string &eventName) const {
    auto it = lastEventParams_.find(eventName);
    if (it != lastEventParams_.end()) {
        return it->second;
    }
    return {};
}

std::future<SendResult> W3CTestEventDispatcher::executeEventImmediately(const EventDescriptor &event) {
    LOG_DEBUG("W3CTestEventDispatcher: Executing immediate event '{}' for W3C test", event.eventName);

    try {
        // Store event parameters for W3C test access
        lastEventParams_[event.eventName] = event.params;

        // REFACTOR: Use EventScheduler for immediate execution (0ms delay)
        auto future = scheduler_->scheduleEvent(event, std::chrono::milliseconds(0), nullptr, "", event.sessionId);
        std::string sendId = future.get();  // Get the assigned sendId from EventScheduler

        LOG_INFO("W3CTestEventDispatcher: Event '{}' dispatched successfully with sendId '{}'", event.eventName,
                 sendId);

        // Create successful result
        std::promise<SendResult> successPromise;
        successPromise.set_value(SendResult::success(sendId));
        return successPromise.get_future();

    } catch (const std::exception &e) {
        LOG_ERROR("W3CTestEventDispatcher: Exception executing event '{}': {}", event.eventName, e.what());

        std::promise<SendResult> errorPromise;
        errorPromise.set_value(SendResult::error("Event execution failed: " + std::string(e.what()),
                                                 SendResult::ErrorType::INTERNAL_ERROR));
        return errorPromise.get_future();
    }
}

// REFACTOR: processReadyEvents() removed - EventScheduler handles this automatically

// REFACTOR: generateSendId() removed - EventScheduler handles sendId generation
}  // namespace RSM::W3C