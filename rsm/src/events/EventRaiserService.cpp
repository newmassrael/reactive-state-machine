#include "events/EventRaiserService.h"
#include "common/Logger.h"
#include "events/EventRaiserRegistry.h"
#include "runtime/IEventRaiser.h"
#include <stdexcept>

namespace RSM {

std::unique_ptr<EventRaiserService> EventRaiserService::instance_;
std::mutex EventRaiserService::initMutex_;

void EventRaiserService::initialize(std::shared_ptr<IEventRaiserRegistry> registry,
                                    std::shared_ptr<ISessionManager> sessionManager) {
    std::lock_guard<std::mutex> lock(initMutex_);

    if (!registry) {
        throw std::invalid_argument("EventRaiserService: registry cannot be null");
    }

    if (!sessionManager) {
        throw std::invalid_argument("EventRaiserService: sessionManager cannot be null");
    }

    instance_ = std::unique_ptr<EventRaiserService>(new EventRaiserService(registry, sessionManager));
    LOG_DEBUG("EventRaiserService: Initialized with dependency injection");
}

EventRaiserService &EventRaiserService::getInstance() {
    std::lock_guard<std::mutex> lock(initMutex_);

    if (!instance_) {
        throw std::runtime_error("EventRaiserService: Not initialized. Call initialize() first.");
    }

    return *instance_;
}

void EventRaiserService::reset() {
    std::lock_guard<std::mutex> lock(initMutex_);
    instance_.reset();
    LOG_DEBUG("EventRaiserService: Reset for testing");
}

EventRaiserService::EventRaiserService(std::shared_ptr<IEventRaiserRegistry> registry,
                                       std::shared_ptr<ISessionManager> sessionManager)
    : registry_(std::move(registry)), sessionManager_(std::move(sessionManager)) {
    LOG_DEBUG("EventRaiserService: Created with injected dependencies");
}

bool EventRaiserService::registerEventRaiser(const std::string &sessionId, std::shared_ptr<IEventRaiser> eventRaiser) {
    if (sessionId.empty()) {
        LOG_ERROR("EventRaiserService: Cannot register EventRaiser - session ID is empty");
        return false;
    }

    if (!eventRaiser) {
        LOG_ERROR("EventRaiserService: Cannot register EventRaiser - eventRaiser is null for session: {}", sessionId);
        return false;
    }

    // Check if session exists before registration
    if (!sessionManager_->hasSession(sessionId)) {
        LOG_DEBUG("EventRaiserService: Session '{}' does not exist yet, deferring EventRaiser registration", sessionId);
        return false;  // Not an error, just deferred
    }

    // Check if already registered to avoid duplicates
    if (registry_->hasEventRaiser(sessionId)) {
        LOG_DEBUG("EventRaiserService: EventRaiser already registered for session: {}", sessionId);
        return true;  // Already registered, success
    }

    // Perform the registration
    bool success = registry_->registerEventRaiser(sessionId, eventRaiser);
    if (success) {
        LOG_DEBUG("EventRaiserService: Successfully registered EventRaiser for session: {}", sessionId);
    } else {
        LOG_ERROR("EventRaiserService: Failed to register EventRaiser for session: {}", sessionId);
    }

    return success;
}

std::shared_ptr<IEventRaiser> EventRaiserService::getEventRaiser(const std::string &sessionId) const {
    if (!registry_) {
        LOG_ERROR("EventRaiserService: Cannot get EventRaiser - registry is null");
        return nullptr;
    }

    return registry_->getEventRaiser(sessionId);
}

bool EventRaiserService::unregisterEventRaiser(const std::string &sessionId) {
    if (!registry_) {
        LOG_ERROR("EventRaiserService: Cannot unregister EventRaiser - registry is null");
        return false;
    }

    bool success = registry_->unregisterEventRaiser(sessionId);
    if (success) {
        LOG_DEBUG("EventRaiserService: Successfully unregistered EventRaiser for session: {}", sessionId);
    } else {
        LOG_DEBUG("EventRaiserService: EventRaiser not found for unregistration - session: {}", sessionId);
    }

    return success;
}

std::shared_ptr<IEventRaiserRegistry> EventRaiserService::getRegistry() const {
    return registry_;
}

void EventRaiserService::clearAll() {
    if (!registry_) {
        LOG_WARN("EventRaiserService: Cannot clear - registry is null");
        return;
    }

    // Safe clearing that works with any IEventRaiserRegistry implementation
    // This method should be added to IEventRaiserRegistry interface
    auto concreteRegistry = std::dynamic_pointer_cast<EventRaiserRegistry>(registry_);
    if (concreteRegistry) {
        concreteRegistry->clear();
        LOG_DEBUG("EventRaiserService: Cleared all EventRaiser registrations");
    } else {
        LOG_WARN("EventRaiserService: Registry does not support clearing");
    }
}

}  // namespace RSM