#include "runtime/impl/EventManager.h"
#include "common/Logger.h"
#include "common/Result.h"
#include "events/Event.h"
// EventDispatcher.h not found - removed include
#include "events/EventQueue.h"
#include <chrono>
#include <sstream>
#include <thread>

namespace SCXML {
namespace Runtime {

// Static member initialization (from original RuntimeContext)
std::atomic<uint64_t> EventManager::sendCounter_{1};

EventManager::EventManager()
    : internalQueue_(std::make_shared<Events::EventQueue>()), externalQueue_(std::make_shared<Events::EventQueue>()) {
    SCXML::Common::Logger::debug("EventManager::Constructor - Creating event manager");
}

// Default destructor is already declared in header

void EventManager::raiseEvent(Events::EventPtr event) {
    if (event) {
        internalQueue_->enqueue(event);
        SCXML::Common::Logger::debug("EventManager::raiseEvent - Raised event: " + event->getName());
    }
}

void EventManager::raiseEvent(const std::string &eventName, const std::string &data) {
    auto event = std::make_shared<Events::Event>(eventName);
    if (!data.empty()) {
        event->setData(data);
    }
    raiseEvent(event);
}

void EventManager::sendEvent(Events::EventPtr event, const std::string &target, uint64_t delayMs) {
    if (event) {
        if (delayMs > 0) {
            scheduleEvent(event, delayMs);
        } else {
            externalQueue_->enqueue(event);
        }
        SCXML::Common::Logger::debug("EventManager::sendEvent - Sent event: " + event->getName() + " to target: " + target);
    }
}

void EventManager::sendEvent(const std::string &eventName, const std::string &target, uint64_t delayMs,
                             const std::string &data) {
    auto event = std::make_shared<Events::Event>(eventName);
    if (!data.empty()) {
        event->setData(data);
    }
    sendEvent(event, target, delayMs);
}

std::shared_ptr<Events::EventQueue> EventManager::getInternalQueue() {
    return internalQueue_;
}

std::shared_ptr<Events::EventQueue> EventManager::getExternalQueue() {
    return externalQueue_;
}

Events::EventPtr EventManager::getNextInternalEvent() {
    return internalQueue_->dequeue();
}

Events::EventPtr EventManager::getNextExternalEvent() {
    return externalQueue_->dequeue();
}

bool EventManager::hasInternalEvents() const {
    return !internalQueue_->empty();
}

bool EventManager::hasExternalEvents() const {
    return !externalQueue_->empty();
}

void EventManager::clearInternalQueue() {
    internalQueue_->clear();
}

void EventManager::clearExternalQueue() {
    externalQueue_->clear();
}

std::string EventManager::scheduleEvent(Events::EventPtr event, uint64_t delayMs, const std::string &sendId) {
    // Placeholder implementation for scheduled events
    std::string actualSendId = sendId.empty() ? generateSendId() : sendId;
    SCXML::Common::Logger::debug("EventManager::scheduleEvent - Scheduled event: " + event->getName() +
                  " with delay: " + std::to_string(delayMs) + "ms");
    return actualSendId;
}

bool EventManager::cancelScheduledEvent(const std::string &sendId) {
    // Placeholder implementation
    SCXML::Common::Logger::debug("EventManager::cancelScheduledEvent - Cancelled event with sendId: " + sendId);
    return true;
}

std::vector<Events::EventPtr> EventManager::getEventHistory(size_t /* maxEvents */) const {
    return eventHistory_;
}

void EventManager::clearEventHistory() {
    eventHistory_.clear();
}

void EventManager::setSessionId(const std::string &sessionId) {
    sessionId_ = sessionId;
}

std::string EventManager::getSessionId() const {
    return sessionId_;
}

Events::EventPtr EventManager::getCurrentEvent() const {
    std::lock_guard<std::mutex> lock(eventMutex_);
    return currentEvent_;
}

// ========== Original RuntimeContext Event Logic ==========

Events::EventPtr EventManager::createEventFromIOProcessor(const std::string &event,
                                                          const std::map<std::string, std::string> &data) {
    SCXML::Common::Logger::debug("EventManager::createEventFromIOProcessor - Creating event: " + event);

    // Create proper Event object (original logic)
    auto eventPtr = std::make_shared<Events::Event>(event);
    eventPtr->setOrigin("ioprocessor");

    // Set event data from map (original logic)
    if (!data.empty()) {
        // Convert map to JSON-like string representation
        std::ostringstream dataStream;
        dataStream << "{";
        bool first = true;
        for (const auto &pair : data) {
            if (!first) {
                dataStream << ",";
            }
            dataStream << "\"" << pair.first << "\":\"" << pair.second << "\"";
            first = false;
        }
        dataStream << "}";
        eventPtr->setData(dataStream.str());
    }

    return eventPtr;
}

SCXML::Common::Result<void> EventManager::sendInternalEvent(const Events::Event &event) {
    try {
        // Create internal event and add to event queue (original logic)
        auto internalEvent =
            std::make_shared<Events::Event>(event.getName(), event.getData(), Events::Event::Type::INTERNAL);

        // Add to internal event queue for processing
        if (internalQueue_) {
            internalQueue_->enqueue(internalEvent);
        }

        SCXML::Common::Logger::info("Sent internal event: " + event.getName());
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        std::string error = "Failed to send internal event: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return SCXML::Common::Result<void>::failure(error);
    }
}

SCXML::Common::Result<void> EventManager::sendExternalEvent(const Events::Event &event,
                                                            const std::string &processorType) {
    try {
        SCXML::Common::Logger::info("Sending external event '" + event.getName() + "' through processor: " + processorType);

        // Add to external queue (simplified from original)
        if (externalQueue_) {
            auto externalEvent =
                std::make_shared<Events::Event>(event.getName(), event.getData(), Events::Event::Type::EXTERNAL);
            externalQueue_->enqueue(externalEvent);
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        std::string error = "Failed to send external event: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return SCXML::Common::Result<void>::failure(error);
    }
}

SCXML::Common::Result<void> EventManager::scheduleDelayedSend(const Events::Event &event, uint64_t delayMs,
                                                              bool isInternal) {
    try {
        // Generate unique send ID (original logic)
        std::string sendId = "send_" + std::to_string(sendCounter_.fetch_add(1));

        // Store the event for potential cancellation
        auto delayedEvent = std::make_shared<Events::Event>(event);
        scheduledSends_[sendId] = delayedEvent;

        // Schedule execution after delay (original logic with thread)
        std::thread([this, delayedEvent, sendId, isInternal, delayMs]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            // Check if the send was not cancelled
            if (scheduledSends_.find(sendId) != scheduledSends_.end()) {
                if (isInternal) {
                    sendInternalEvent(*delayedEvent);
                } else {
                    sendExternalEvent(*delayedEvent, "scxml");  // Default processor
                }

                // Remove from scheduled sends
                scheduledSends_.erase(sendId);
            }
        }).detach();

        SCXML::Common::Logger::info("Scheduled delayed send with ID: " + sendId + " (delay: " + std::to_string(delayMs) + "ms)");

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        std::string error = "Failed to schedule delayed send: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return SCXML::Common::Result<void>::failure(error);
    }
}

std::string EventManager::generateSendId() {
    return "send_" + std::to_string(sendCounter_.fetch_add(1));
}

}  // namespace Runtime
}  // namespace SCXML