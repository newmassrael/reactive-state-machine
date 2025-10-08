#include "events/EventDispatcherImpl.h"
#include "common/Logger.h"
#include "common/StringUtils.h"
#include <sstream>
#include <stdexcept>

namespace RSM {

EventDispatcherImpl::EventDispatcherImpl(std::shared_ptr<IEventScheduler> scheduler,
                                         std::shared_ptr<IEventTargetFactory> targetFactory)
    : scheduler_(std::move(scheduler)), targetFactory_(std::move(targetFactory)) {
    if (!scheduler_) {
        throw std::invalid_argument("EventDispatcherImpl requires a valid scheduler");
    }

    if (!targetFactory_) {
        throw std::invalid_argument("EventDispatcherImpl requires a valid target factory");
    }

    LOG_DEBUG("Dispatcher created with scheduler and target factory");
}

std::future<SendResult> EventDispatcherImpl::sendEvent(const EventDescriptor &event) {
    try {
        // Create appropriate target for the event
        auto target = targetFactory_->createTarget(event.target, event.sessionId);
        if (!target) {
            std::promise<SendResult> errorPromise;
            errorPromise.set_value(SendResult::error("Failed to create target for: " + event.target,
                                                     SendResult::ErrorType::TARGET_NOT_FOUND));
            return errorPromise.get_future();
        }

        // W3C SCXML Test 230: Platform events (done.*, error.*) must be queued
        // to prevent nested processing issues when child completes during parent transition
        bool isPlatform = isPlatformEvent(event.eventName);

        // Check if this is a delayed event or platform event that needs queueing
        if (event.delay.count() > 0 || isPlatform) {
            auto effectiveDelay = event.delay;
            if (isPlatform && effectiveDelay.count() == 0) {
                // Platform events queue immediately (0ms) to prevent nested processing
                effectiveDelay = std::chrono::milliseconds(0);
                LOG_DEBUG("Platform event '{}' queued immediately (0ms)", event.eventName);
            }

            LOG_DEBUG("Scheduling delayed event '{}' with {}ms delay in session '{}' (sendId: '{}')", event.eventName,
                      effectiveDelay.count(), event.sessionId, event.sendId);

            // Schedule the event for delayed execution
            auto sendIdFuture = scheduler_->scheduleEvent(event, effectiveDelay, target, event.sendId, event.sessionId);

            // Convert sendId future to SendResult future
            std::promise<SendResult> resultPromise;
            auto resultFuture = resultPromise.get_future();

            // Handle the sendId asynchronously
            std::thread([sendIdFuture = std::move(sendIdFuture), resultPromise = std::move(resultPromise)]() mutable {
                try {
                    std::string assignedSendId = sendIdFuture.get();
                    resultPromise.set_value(SendResult::success(assignedSendId));
                } catch (const std::exception &e) {
                    resultPromise.set_value(SendResult::error("Failed to schedule event: " + std::string(e.what()),
                                                              SendResult::ErrorType::INTERNAL_ERROR));
                }
            }).detach();

            return resultFuture;
        } else {
            // Execute immediately
            LOG_DEBUG("Executing immediate event '{}'", event.eventName);
            return executeEventImmediately(event, target);
        }

    } catch (const std::exception &e) {
        LOG_ERROR("Error sending event '{}': {}", event.eventName, e.what());
        std::promise<SendResult> errorPromise;
        errorPromise.set_value(
            SendResult::error("Failed to send event: " + std::string(e.what()), SendResult::ErrorType::INTERNAL_ERROR));
        return errorPromise.get_future();
    }
}

bool EventDispatcherImpl::cancelEvent(const std::string &sendId, const std::string &sessionId) {
    if (sendId.empty()) {
        LOG_WARN("Cannot cancel event with empty sendId");
        return false;
    }

    LOG_DEBUG("EventDispatcherImpl: Cancelling event with sendId: {}", sendId);
    return scheduler_->cancelEvent(sendId, sessionId);
}

std::future<SendResult> EventDispatcherImpl::sendEventDelayed(const EventDescriptor &event,
                                                              std::chrono::milliseconds delay) {
    // This is handled by the main sendEvent method based on the delay in the event descriptor
    EventDescriptor delayedEvent = event;
    delayedEvent.delay = delay;
    return sendEvent(delayedEvent);
}

bool EventDispatcherImpl::isEventPending(const std::string &sendId) const {
    return scheduler_->hasEvent(sendId);
}

std::string EventDispatcherImpl::getStatistics() const {
    size_t pendingCount = scheduler_->getScheduledEventCount();
    bool isRunning = scheduler_->isRunning();

    std::ostringstream stats;
    stats << "EventDispatcher Status: " << (isRunning ? "Running" : "Stopped") << ", Pending Events: " << pendingCount;
    return stats.str();
}

void EventDispatcherImpl::shutdown() {
    LOG_DEBUG("EventDispatcherImpl: Shutting down dispatcher");

    if (scheduler_) {
        scheduler_->shutdown(true);
    }

    LOG_DEBUG("EventDispatcherImpl: Dispatcher shutdown complete");
}

size_t EventDispatcherImpl::cancelEventsForSession(const std::string &sessionId) {
    LOG_DEBUG("EventDispatcherImpl: Cancelling all events for session: {}", sessionId);

    // W3C SCXML 6.2: Cancel all scheduled events for the specified session
    if (scheduler_) {
        return scheduler_->cancelEventsForSession(sessionId);
    }

    LOG_WARN("EventDispatcherImpl: No scheduler available for session event cancellation");
    return 0;
}

std::future<SendResult> EventDispatcherImpl::executeEventImmediately(const EventDescriptor &event,
                                                                     std::shared_ptr<IEventTarget> target) {
    try {
        LOG_DEBUG("EventDispatcherImpl: Executing immediate event '{}' to target '{}'", event.eventName, event.target);

        // Validate target before sending
        if (!target) {
            LOG_ERROR("EventDispatcherImpl: Target is null for event '{}'", event.eventName);
            std::promise<SendResult> errorPromise;
            errorPromise.set_value(SendResult::error("Target is null", SendResult::ErrorType::TARGET_NOT_FOUND));
            return errorPromise.get_future();
        }

        LOG_DEBUG("EventDispatcherImpl: Calling target->send() for event '{}' with target type: {}", event.eventName,
                  target->getTargetType());

        // Execute the event directly on the target
        auto resultFuture = target->send(event);

        LOG_DEBUG("EventDispatcherImpl: target->send() called successfully for event '{}'", event.eventName);
        return resultFuture;

    } catch (const std::exception &e) {
        LOG_ERROR("EventDispatcherImpl: Error executing immediate event '{}': {}", event.eventName, e.what());

        std::promise<SendResult> errorPromise;
        errorPromise.set_value(SendResult::error("Failed to execute immediate event: " + std::string(e.what()),
                                                 SendResult::ErrorType::INTERNAL_ERROR));
        return errorPromise.get_future();
    }
}

std::future<SendResult> EventDispatcherImpl::onScheduledEventExecution(const EventDescriptor &event,
                                                                       std::shared_ptr<IEventTarget> target,
                                                                       const std::string &sendId) {
    try {
        LOG_DEBUG("EventDispatcherImpl: Executing scheduled event '{}' with sendId '{}'", event.eventName, sendId);

        // Execute the scheduled event on the target
        auto resultFuture = target->send(event);

        // Wrap the result to include the sendId in logging
        std::promise<SendResult> wrappedPromise;
        auto wrappedFuture = wrappedPromise.get_future();

        std::thread([resultFuture = std::move(resultFuture), wrappedPromise = std::move(wrappedPromise),
                     eventName = event.eventName, sendId]() mutable {
            try {
                auto result = resultFuture.get();
                if (result.isSuccess) {
                    LOG_DEBUG("EventDispatcherImpl: Scheduled event '{}' with sendId '{}' executed successfully",
                              eventName, sendId);
                } else {
                    LOG_WARN("EventDispatcherImpl: Scheduled event '{}' with sendId '{}' failed: {}", eventName, sendId,
                             result.errorMessage);
                }
                wrappedPromise.set_value(std::move(result));
            } catch (const std::exception &e) {
                LOG_ERROR("EventDispatcherImpl: Exception executing scheduled event '{}' with sendId '{}': {}",
                          eventName, sendId, e.what());
                wrappedPromise.set_value(SendResult::error("Scheduled event execution failed: " + std::string(e.what()),
                                                           SendResult::ErrorType::INTERNAL_ERROR));
            }
        }).detach();

        return wrappedFuture;

    } catch (const std::exception &e) {
        LOG_ERROR("EventDispatcherImpl: Error executing scheduled event '{}' with sendId '{}': {}", event.eventName,
                  sendId, e.what());

        std::promise<SendResult> errorPromise;
        errorPromise.set_value(SendResult::error("Failed to execute scheduled event: " + std::string(e.what()),
                                                 SendResult::ErrorType::INTERNAL_ERROR));
        return errorPromise.get_future();
    }
}

}  // namespace RSM
