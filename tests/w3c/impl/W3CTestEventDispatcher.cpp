#include "W3CTestEventDispatcher.h"
#include "events/EventDescriptor.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>
#include <thread>

namespace RSM::W3C {

W3CTestEventDispatcher::W3CTestEventDispatcher(const std::string &sessionId) : sessionId_(sessionId) {
    LOG_DEBUG("W3CTestEventDispatcher created for session: {} (W3C compliance mode with delay support)", sessionId_);
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

        // Process any ready events before executing new ones
        processReadyEvents();

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
    std::lock_guard<std::mutex> lock(schedulerMutex_);

    auto it = scheduledEvents_.find(sendId);
    if (it != scheduledEvents_.end() && !it->second->cancelled) {
        it->second->cancelled = true;
        LOG_DEBUG("W3CTestEventDispatcher: Successfully cancelled event with sendId: {} (W3C SCXML 6.2 compliance)",
                  sendId);
        return true;
    }

    LOG_DEBUG("W3CTestEventDispatcher: Event with sendId '{}' not found or already cancelled", sendId);
    return false;
}

std::future<SendResult> W3CTestEventDispatcher::sendEventDelayed(const EventDescriptor &event,
                                                                 std::chrono::milliseconds delay) {
    LOG_DEBUG("W3CTestEventDispatcher: Scheduling delayed event '{}' with {}ms delay (W3C compliance mode)",
              event.eventName, delay.count());

    try {
        std::lock_guard<std::mutex> lock(schedulerMutex_);

        // Generate unique sendId
        std::string sendId = generateSendId();

        // Calculate execution time
        auto executeAt = std::chrono::steady_clock::now() + delay;

        // W3C SCXML 6.2: Store evaluated parameters immediately (mandatory compliance)
        // Parameters MUST be evaluated at send time, not at dispatch time
        lastEventParams_[event.eventName] = event.params;

        // W3C SCXML 6.2: Create scheduled event with evaluated parameters (mandatory compliance)
        // Parameters MUST be evaluated at send time, not at dispatch time
        auto scheduledEvent = std::make_unique<ScheduledTestEvent>(event, executeAt, sendId, event.params);
        scheduledEvents_[sendId] = std::move(scheduledEvent);

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
    std::lock_guard<std::mutex> lock(schedulerMutex_);

    auto it = scheduledEvents_.find(sendId);
    if (it != scheduledEvents_.end() && !it->second->cancelled) {
        auto now = std::chrono::steady_clock::now();
        return it->second->executeAt > now;
    }

    return false;
}

std::string W3CTestEventDispatcher::getStatistics() const {
    std::lock_guard<std::mutex> lock(schedulerMutex_);

    size_t pendingCount = 0;
    size_t cancelledCount = 0;

    for (const auto &pair : scheduledEvents_) {
        if (pair.second->cancelled) {
            cancelledCount++;
        } else {
            auto now = std::chrono::steady_clock::now();
            if (pair.second->executeAt > now) {
                pendingCount++;
            }
        }
    }

    std::ostringstream stats;
    stats << "W3CTestEventDispatcher [Session: " << sessionId_
          << "] - Status: Active, Mode: W3C Compliance, Pending: " << pendingCount << ", Cancelled: " << cancelledCount;
    return stats.str();
}

void W3CTestEventDispatcher::shutdown() {
    LOG_DEBUG("W3CTestEventDispatcher: Shutting down for session: {} (W3C SCXML 6.2: cancelling all pending events)",
              sessionId_);

    std::lock_guard<std::mutex> lock(schedulerMutex_);

    // W3C SCXML 6.2: Cancel all pending events on shutdown
    size_t cancelledCount = 0;
    for (auto &pair : scheduledEvents_) {
        if (!pair.second->cancelled) {
            pair.second->cancelled = true;
            cancelledCount++;
        }
    }

    LOG_INFO("W3CTestEventDispatcher: Shutdown complete - cancelled {} pending events for W3C compliance",
             cancelledCount);
}

size_t W3CTestEventDispatcher::cancelEventsForSession(const std::string &sessionId) {
    std::lock_guard<std::mutex> lock(schedulerMutex_);

    size_t cancelledCount = 0;

    // W3C SCXML 6.2: Cancel events for specific session
    for (auto &pair : scheduledEvents_) {
        if (!pair.second->cancelled && pair.second->event.sessionId == sessionId) {
            pair.second->cancelled = true;
            cancelledCount++;
            LOG_DEBUG("W3CTestEventDispatcher: Cancelled event '{}' with sendId '{}' for session '{}'",
                      pair.second->event.eventName, pair.first, sessionId);
        }
    }

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

        // Generate a unique sendId for this event execution
        std::string sendId = generateSendId();

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

void W3CTestEventDispatcher::processReadyEvents() {
    std::lock_guard<std::mutex> lock(schedulerMutex_);

    auto now = std::chrono::steady_clock::now();

    // Find events ready for execution
    std::vector<std::string> readyEventIds;
    for (const auto &pair : scheduledEvents_) {
        if (!pair.second->cancelled && pair.second->executeAt <= now) {
            readyEventIds.push_back(pair.first);
        }
    }

    // Execute ready events
    for (const std::string &sendId : readyEventIds) {
        auto it = scheduledEvents_.find(sendId);
        if (it != scheduledEvents_.end() && !it->second->cancelled) {
            LOG_INFO("W3CTestEventDispatcher: Executing scheduled event '{}' with sendId '{}' (W3C compliance)",
                     it->second->event.eventName, sendId);

            // W3C SCXML 6.2: Use stored evaluated parameters (mandatory compliance)
            // Parameters were evaluated at send time, not at dispatch time
            lastEventParams_[it->second->event.eventName] = it->second->evaluatedParams;

            // In a real implementation, we would deliver the event to its target
            // For W3C test environment, we just mark it as executed

            // Remove executed event
            scheduledEvents_.erase(it);
        }
    }
}

std::string W3CTestEventDispatcher::generateSendId() {
    return "w3c_test_" + sessionId_ + "_" + std::to_string(sendIdCounter_++);
}
}  // namespace RSM::W3C