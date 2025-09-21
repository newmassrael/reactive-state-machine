#pragma once

#include "states/ConcurrentEventTypes.h"
#include "states/IConcurrentRegion.h"
#include <functional>
#include <memory>
#include <vector>

namespace RSM {

/**
 * @brief Interface for concurrent event broadcasting in parallel states
 *
 * This interface defines the contract for broadcasting events to multiple
 * concurrent regions according to SCXML parallel state semantics.
 *
 * SCXML Compliance:
 * - Events must be delivered to all active regions simultaneously
 * - Event processing should be non-blocking between regions
 * - Event order and timing must be preserved per region
 * - Error handling should not affect other regions
 */
class IConcurrentEventBroadcaster {
public:
    virtual ~IConcurrentEventBroadcaster() = default;

    /**
     * @brief Broadcast an event to regions according to configuration
     * @param request Complete event broadcasting request
     * @return Result of the broadcasting operation
     */
    virtual EventBroadcastResult broadcastEvent(const EventBroadcastRequest &request) = 0;

    /**
     * @brief Simple event broadcast to all active regions
     * @param event Event to broadcast
     * @return Result of the broadcasting operation
     */
    virtual EventBroadcastResult broadcastEvent(const EventDescriptor &event) = 0;

    /**
     * @brief Broadcast event to specific regions only
     * @param event Event to broadcast
     * @param targetRegions Specific regions to target
     * @return Result of the broadcasting operation
     */
    virtual EventBroadcastResult broadcastEventToRegions(const EventDescriptor &event,
                                                         const std::vector<std::string> &targetRegions) = 0;

    /**
     * @brief Broadcast event with priority
     * @param event Event to broadcast
     * @param priority Priority level for the event
     * @return Result of the broadcasting operation
     */
    virtual EventBroadcastResult broadcastEventWithPriority(const EventDescriptor &event,
                                                            EventBroadcastPriority priority) = 0;

    /**
     * @brief Register a region for event broadcasting
     * @param region Region to register
     * @return true if successfully registered
     */
    virtual bool registerRegion(std::shared_ptr<IConcurrentRegion> region) = 0;

    /**
     * @brief Unregister a region from event broadcasting
     * @param regionId ID of region to unregister
     * @return true if successfully unregistered
     */
    virtual bool unregisterRegion(const std::string &regionId) = 0;

    /**
     * @brief Get all currently registered regions
     * @return Vector of registered regions
     */
    virtual std::vector<std::shared_ptr<IConcurrentRegion>> getRegisteredRegions() const = 0;

    /**
     * @brief Get all currently active regions
     * @return Vector of active regions
     */
    virtual std::vector<std::shared_ptr<IConcurrentRegion>> getActiveRegions() const = 0;

    /**
     * @brief Set event broadcasting configuration
     * @param config New configuration
     */
    virtual void setConfiguration(const EventBroadcastConfig &config) = 0;

    /**
     * @brief Get current event broadcasting configuration
     * @return Current configuration
     */
    virtual const EventBroadcastConfig &getConfiguration() const = 0;

    /**
     * @brief Set callback for event broadcasting completion
     * @param callback Function to call when broadcasting completes
     */
    virtual void setEventBroadcastCallback(
        std::function<void(const EventBroadcastRequest &, const EventBroadcastResult &)> callback) = 0;

    /**
     * @brief Get event broadcasting statistics
     * @return Current statistics
     */
    virtual const EventBroadcastStatistics &getStatistics() const = 0;

    /**
     * @brief Reset event broadcasting statistics
     */
    virtual void resetStatistics() = 0;

    /**
     * @brief Check if a specific region is currently active
     * @param regionId ID of region to check
     * @return true if region is active
     */
    virtual bool isRegionActive(const std::string &regionId) const = 0;

    /**
     * @brief Get the number of currently active regions
     * @return Number of active regions
     */
    virtual size_t getActiveRegionCount() const = 0;

    /**
     * @brief Validate event broadcasting configuration
     * @return Vector of validation error messages (empty if valid)
     */
    virtual std::vector<std::string> validateConfiguration() const = 0;
};

}  // namespace RSM