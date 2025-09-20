#include "events/EventDispatcherImpl.h"
#include "common/Logger.h"
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

    Logger::debug("EventDispatcherImpl: Dispatcher created with scheduler and target factory");
}

std::future<SendResult> EventDispatcherImpl::sendEvent(const EventDescriptor &event) {
    try {
        // Create appropriate target for the event
        auto target = targetFactory_->createTarget(event.target);
        if (!target) {
            std::promise<SendResult> errorPromise;
            errorPromise.set_value(SendResult::error("Failed to create target for: " + event.target,
                                                     SendResult::ErrorType::TARGET_NOT_FOUND));
            return errorPromise.get_future();
        }

        // Check if this is a delayed event
        if (event.delay.count() > 0) {
            Logger::debug("EventDispatcherImpl: Scheduling delayed event '{}' with {}ms delay", event.eventName,
                          event.delay.count());

            // Schedule the event for delayed execution
            auto sendIdFuture = scheduler_->scheduleEvent(event, event.delay, target, event.sendId);

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
            Logger::debug("EventDispatcherImpl: Executing immediate event '{}'", event.eventName);
            return executeEventImmediately(event, target);
        }

    } catch (const std::exception &e) {
        Logger::error("EventDispatcherImpl: Error sending event '{}': {}", event.eventName, e.what());
        std::promise<SendResult> errorPromise;
        errorPromise.set_value(
            SendResult::error("Failed to send event: " + std::string(e.what()), SendResult::ErrorType::INTERNAL_ERROR));
        return errorPromise.get_future();
    }
}

bool EventDispatcherImpl::cancelEvent(const std::string &sendId) {
    if (sendId.empty()) {
        Logger::warn("EventDispatcherImpl: Cannot cancel event with empty sendId");
        return false;
    }

    Logger::debug("EventDispatcherImpl: Cancelling event with sendId: {}", sendId);
    return scheduler_->cancelEvent(sendId);
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
    Logger::debug("EventDispatcherImpl: Shutting down dispatcher");

    if (scheduler_) {
        scheduler_->shutdown(true);
    }

    Logger::debug("EventDispatcherImpl: Dispatcher shutdown complete");
}

std::future<SendResult> EventDispatcherImpl::executeEventImmediately(const EventDescriptor &event,
                                                                     std::shared_ptr<IEventTarget> target) {
    try {
        Logger::debug("EventDispatcherImpl: Executing immediate event '{}' to target '{}'", event.eventName,
                      event.target);

        // Execute the event directly on the target
        return target->send(event);

    } catch (const std::exception &e) {
        Logger::error("EventDispatcherImpl: Error executing immediate event '{}': {}", event.eventName, e.what());

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
        Logger::debug("EventDispatcherImpl: Executing scheduled event '{}' with sendId '{}'", event.eventName, sendId);

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
                    Logger::debug("EventDispatcherImpl: Scheduled event '{}' with sendId '{}' executed successfully",
                                  eventName, sendId);
                } else {
                    Logger::warn("EventDispatcherImpl: Scheduled event '{}' with sendId '{}' failed: {}", eventName,
                                 sendId, result.errorMessage);
                }
                wrappedPromise.set_value(std::move(result));
            } catch (const std::exception &e) {
                Logger::error("EventDispatcherImpl: Exception executing scheduled event '{}' with sendId '{}': {}",
                              eventName, sendId, e.what());
                wrappedPromise.set_value(SendResult::error("Scheduled event execution failed: " + std::string(e.what()),
                                                           SendResult::ErrorType::INTERNAL_ERROR));
            }
        }).detach();

        return wrappedFuture;

    } catch (const std::exception &e) {
        Logger::error("EventDispatcherImpl: Error executing scheduled event '{}' with sendId '{}': {}", event.eventName,
                      sendId, e.what());

        std::promise<SendResult> errorPromise;
        errorPromise.set_value(SendResult::error("Failed to execute scheduled event: " + std::string(e.what()),
                                                 SendResult::ErrorType::INTERNAL_ERROR));
        return errorPromise.get_future();
    }
}

}  // namespace RSM