#pragma once

#include "events/IEventRaiserRegistry.h"
#include <mutex>
#include <unordered_map>

namespace RSM {

/**
 * @brief Thread-safe implementation of EventRaiser registry (SOLID: Single Responsibility)
 *
 * This class provides concrete implementation of EventRaiser management
 * with thread safety and proper lifecycle management.
 * Follows SOLID principles:
 * - Single Responsibility: Only manages EventRaiser storage and retrieval
 * - Open/Closed: Can be extended without modification
 * - Liskov Substitution: Fully substitutable for IEventRaiserRegistry
 * - Dependency Inversion: Depends on IEventRaiser abstraction
 */
class EventRaiserRegistry : public IEventRaiserRegistry {
public:
    EventRaiserRegistry() = default;
    ~EventRaiserRegistry() override = default;

    // Non-copyable, non-movable for thread safety
    EventRaiserRegistry(const EventRaiserRegistry &) = delete;
    EventRaiserRegistry &operator=(const EventRaiserRegistry &) = delete;
    EventRaiserRegistry(EventRaiserRegistry &&) = delete;
    EventRaiserRegistry &operator=(EventRaiserRegistry &&) = delete;

    // IEventRaiserRegistry implementation
    bool registerEventRaiser(const std::string &sessionId, std::shared_ptr<IEventRaiser> eventRaiser) override;
    std::shared_ptr<IEventRaiser> getEventRaiser(const std::string &sessionId) const override;
    bool unregisterEventRaiser(const std::string &sessionId) override;
    bool hasEventRaiser(const std::string &sessionId) const override;

    /**
     * @brief Get number of registered EventRaisers (for testing/debugging)
     */
    size_t getRegistrySize() const;

    /**
     * @brief Clear all registrations (for testing cleanup)
     */
    void clear();

private:
    mutable std::mutex registryMutex_;
    std::unordered_map<std::string, std::shared_ptr<IEventRaiser>> eventRaisers_;
};

}  // namespace RSM