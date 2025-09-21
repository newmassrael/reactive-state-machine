#pragma once

#include "ConcurrentEventTypes.h"
#include "common/Logger.h"
#include "states/IConcurrentEventBroadcaster.h"
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace RSM {

/**
 * @brief Concrete implementation of concurrent event broadcasting
 *
 * This class implements the SCXML parallel state event broadcasting semantics,
 * ensuring that events are delivered to all active regions simultaneously
 * while maintaining proper error isolation and performance monitoring.
 *
 * SCXML Compliance:
 * - Events are broadcast to all active regions in parallel states
 * - Event processing is non-blocking between regions
 * - Failed regions don't affect event delivery to other regions
 * - Event order and timing are preserved per region
 */
class ConcurrentEventBroadcaster : public IConcurrentEventBroadcaster {
public:
    /**
     * @brief Construct event broadcaster with configuration
     * @param config Initial configuration
     */
    explicit ConcurrentEventBroadcaster(const EventBroadcastConfig &config = {});

    /**
     * @brief Destructor
     */
    virtual ~ConcurrentEventBroadcaster();

    // IConcurrentEventBroadcaster interface implementation
    EventBroadcastResult broadcastEvent(const EventBroadcastRequest &request) override;
    EventBroadcastResult broadcastEvent(const EventDescriptor &event) override;
    EventBroadcastResult broadcastEventToRegions(const EventDescriptor &event,
                                                 const std::vector<std::string> &targetRegions) override;
    EventBroadcastResult broadcastEventWithPriority(const EventDescriptor &event,
                                                    EventBroadcastPriority priority) override;

    bool registerRegion(std::shared_ptr<IConcurrentRegion> region) override;
    bool unregisterRegion(const std::string &regionId) override;
    std::vector<std::shared_ptr<IConcurrentRegion>> getRegisteredRegions() const override;
    std::vector<std::shared_ptr<IConcurrentRegion>> getActiveRegions() const override;

    void setConfiguration(const EventBroadcastConfig &config) override;
    const EventBroadcastConfig &getConfiguration() const override;

    void setEventBroadcastCallback(
        std::function<void(const EventBroadcastRequest &, const EventBroadcastResult &)> callback) override;

    const EventBroadcastStatistics &getStatistics() const override;
    void resetStatistics() override;

    bool isRegionActive(const std::string &regionId) const override;
    size_t getActiveRegionCount() const override;
    std::vector<std::string> validateConfiguration() const override;

private:
    /**
     * @brief Thread-safe region registry
     */
    mutable std::mutex regionsMutex_;
    std::unordered_map<std::string, std::shared_ptr<IConcurrentRegion>> regions_;

    /**
     * @brief Broadcasting configuration
     */
    mutable std::mutex configMutex_;
    EventBroadcastConfig config_;

    /**
     * @brief Performance statistics
     */
    mutable std::mutex statisticsMutex_;
    EventBroadcastStatistics statistics_;

    /**
     * @brief Event completion callback
     */
    std::function<void(const EventBroadcastRequest &, const EventBroadcastResult &)> eventCallback_;

    /**
     * @brief Internal implementation methods
     */

    /**
     * @brief Get target regions based on broadcast scope
     * @param request Event broadcast request
     * @return Vector of target regions
     */
    std::vector<std::shared_ptr<IConcurrentRegion>> getTargetRegions(const EventBroadcastRequest &request) const;

    /**
     * @brief Broadcast event to specific regions with parallel processing
     * @param event Event to broadcast
     * @param targetRegions Regions to target
     * @param config Broadcasting configuration
     * @return Result of broadcasting operation
     */
    EventBroadcastResult
    broadcastToRegionsParallel(const EventDescriptor &event,
                               const std::vector<std::shared_ptr<IConcurrentRegion>> &targetRegions,
                               const EventBroadcastConfig &config);

    /**
     * @brief Broadcast event to specific regions sequentially
     * @param event Event to broadcast
     * @param targetRegions Regions to target
     * @param config Broadcasting configuration
     * @return Result of broadcasting operation
     */
    EventBroadcastResult
    broadcastToRegionsSequential(const EventDescriptor &event,
                                 const std::vector<std::shared_ptr<IConcurrentRegion>> &targetRegions,
                                 const EventBroadcastConfig &config);

    /**
     * @brief Process event in a single region with timeout
     * @param region Target region
     * @param event Event to process
     * @param timeout Timeout for processing
     * @return Future containing operation result
     */
    std::future<ConcurrentOperationResult> processEventInRegion(std::shared_ptr<IConcurrentRegion> region,
                                                                const EventDescriptor &event,
                                                                std::chrono::milliseconds timeout);

    /**
     * @brief Update statistics with broadcast result
     * @param result Broadcasting result
     * @param priority Event priority
     * @param startTime Start time of operation
     */
    void updateStatistics(const EventBroadcastResult &result, EventBroadcastPriority priority,
                          std::chrono::system_clock::time_point startTime);

    /**
     * @brief Validate region before broadcasting
     * @param region Region to validate
     * @return true if region is valid for broadcasting
     */
    bool validateRegion(const std::shared_ptr<IConcurrentRegion> &region) const;

    /**
     * @brief Create correlation ID for event tracking
     * @return Unique correlation ID
     */
    std::string generateCorrelationId() const;

    /**
     * @brief Log broadcasting operation details
     * @param request Event request
     * @param result Broadcasting result
     * @param duration Operation duration
     */
    void logBroadcastOperation(const EventBroadcastRequest &request, const EventBroadcastResult &result,
                               std::chrono::milliseconds duration) const;
};

}  // namespace RSM