#include "events/EventRaiserRegistry.h"
#include "common/Logger.h"
#include <algorithm>

namespace RSM {

bool EventRaiserRegistry::registerEventRaiser(const std::string &sessionId, std::shared_ptr<IEventRaiser> eventRaiser) {
    if (sessionId.empty()) {
        LOG_ERROR("EventRaiserRegistry: Cannot register EventRaiser - session ID is empty");
        return false;
    }

    if (!eventRaiser) {
        LOG_ERROR("EventRaiserRegistry: Cannot register EventRaiser - eventRaiser is null for session: {}", sessionId);
        return false;
    }

    std::lock_guard<std::mutex> lock(registryMutex_);

    // Check if EventRaiser is already registered
    auto it = eventRaisers_.find(sessionId);
    if (it != eventRaisers_.end()) {
        LOG_DEBUG("EventRaiserRegistry: EventRaiser already registered for session: {}", sessionId);
        return true;  // Already registered, not an error
    }

    // Register new EventRaiser
    eventRaisers_[sessionId] = eventRaiser;
    LOG_DEBUG("EventRaiserRegistry: Successfully registered EventRaiser for session: {} (total: {})", sessionId,
              eventRaisers_.size());

    return true;
}

std::shared_ptr<IEventRaiser> EventRaiserRegistry::getEventRaiser(const std::string &sessionId) const {
    if (sessionId.empty()) {
        LOG_DEBUG("EventRaiserRegistry: Cannot get EventRaiser - session ID is empty");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(registryMutex_);

    auto it = eventRaisers_.find(sessionId);
    if (it != eventRaisers_.end()) {
        LOG_DEBUG("EventRaiserRegistry: Found EventRaiser for session: {}", sessionId);
        return it->second;
    }

    LOG_DEBUG("EventRaiserRegistry: No EventRaiser found for session: {}", sessionId);
    return nullptr;
}

bool EventRaiserRegistry::unregisterEventRaiser(const std::string &sessionId) {
    if (sessionId.empty()) {
        LOG_ERROR("EventRaiserRegistry: Cannot unregister EventRaiser - session ID is empty");
        return false;
    }

    std::lock_guard<std::mutex> lock(registryMutex_);

    auto it = eventRaisers_.find(sessionId);
    if (it != eventRaisers_.end()) {
        eventRaisers_.erase(it);
        LOG_DEBUG("EventRaiserRegistry: Successfully unregistered EventRaiser for session: {} (remaining: {})",
                  sessionId, eventRaisers_.size());
        return true;
    }

    LOG_DEBUG("EventRaiserRegistry: EventRaiser not found for unregistration - session: {}", sessionId);
    return false;
}

bool EventRaiserRegistry::hasEventRaiser(const std::string &sessionId) const {
    if (sessionId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(registryMutex_);
    return eventRaisers_.find(sessionId) != eventRaisers_.end();
}

size_t EventRaiserRegistry::getRegistrySize() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    return eventRaisers_.size();
}

void EventRaiserRegistry::clear() {
    std::lock_guard<std::mutex> lock(registryMutex_);
    size_t clearedCount = eventRaisers_.size();
    eventRaisers_.clear();
    LOG_DEBUG("EventRaiserRegistry: Cleared {} EventRaiser registrations", clearedCount);
}

}  // namespace RSM