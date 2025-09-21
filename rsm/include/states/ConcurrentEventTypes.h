#pragma once

#include "events/EventDescriptor.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

// Forward declarations
class IConcurrentRegion;

/**
 * @brief Event broadcasting priority levels for concurrent regions
 *
 * SCXML Compliance:
 * - Events are processed in parallel but may have different priority levels
 * - Internal events typically have higher priority than external events
 * - Error events have highest priority for immediate propagation
 */
enum class EventBroadcastPriority {
    LOW = 1,      // Background/cleanup events
    NORMAL = 2,   // Standard external events
    HIGH = 3,     // Internal events, state changes
    CRITICAL = 4  // Error events, shutdown signals
};

/**
 * @brief Event broadcasting scope for controlling propagation
 *
 * SCXML Compliance:
 * - ALL_ACTIVE_REGIONS: Standard parallel state behavior
 * - SELECTED_REGIONS: For targeted event delivery
 * - CONDITIONAL_REGIONS: Based on region state/condition
 */
enum class EventBroadcastScope {
    ALL_ACTIVE_REGIONS,  // Broadcast to all currently active regions
    SELECTED_REGIONS,    // Broadcast to specifically selected regions
    CONDITIONAL_REGIONS  // Broadcast based on region conditions
};

/**
 * @brief Result of event broadcasting operation to multiple regions
 */
struct EventBroadcastResult {
    bool isSuccess = false;
    std::vector<std::string> successfulRegions;
    std::vector<std::string> failedRegions;
    std::vector<std::string> skippedRegions;  // Inactive or filtered regions
    std::string errorMessage;
    std::chrono::milliseconds processingTime{0};

    static EventBroadcastResult success(const std::vector<std::string> &successfulRegions,
                                        std::chrono::milliseconds processingTime = std::chrono::milliseconds{0});

    static EventBroadcastResult failure(const std::string &error,
                                        const std::vector<std::string> &successfulRegions = {},
                                        const std::vector<std::string> &failedRegions = {});

    static EventBroadcastResult partial(const std::vector<std::string> &successfulRegions,
                                        const std::vector<std::string> &failedRegions, const std::string &error);
};

/**
 * @brief Configuration for event broadcasting behavior
 */
struct EventBroadcastConfig {
    EventBroadcastPriority defaultPriority = EventBroadcastPriority::NORMAL;
    EventBroadcastScope defaultScope = EventBroadcastScope::ALL_ACTIVE_REGIONS;

    bool parallelProcessing = true;    // Process regions concurrently
    bool stopOnFirstFailure = false;   // Continue broadcasting even if some regions fail
    bool recordProcessingTime = true;  // Track performance metrics
    bool validateRegionState = true;   // Check region state before broadcasting

    std::chrono::milliseconds timeoutPerRegion{1000};  // Timeout for each region
    std::chrono::milliseconds totalTimeout{5000};      // Total broadcasting timeout
};

/**
 * @brief Event broadcasting request with full context
 */
struct EventBroadcastRequest {
    EventDescriptor event;
    EventBroadcastPriority priority = EventBroadcastPriority::NORMAL;
    EventBroadcastScope scope = EventBroadcastScope::ALL_ACTIVE_REGIONS;

    // For SELECTED_REGIONS scope
    std::vector<std::string> targetRegions;

    // For CONDITIONAL_REGIONS scope
    std::function<bool(const std::shared_ptr<IConcurrentRegion> &)> regionFilter;

    // Metadata
    std::string sourceRegion;  // Which region initiated this event
    std::chrono::system_clock::time_point timestamp;
    std::string correlationId;  // For tracking related events
};

/**
 * @brief Statistics for event broadcasting performance monitoring
 */
struct EventBroadcastStatistics {
    size_t totalEvents = 0;
    size_t successfulEvents = 0;
    size_t failedEvents = 0;
    size_t partialEvents = 0;  // Some regions succeeded, some failed

    std::chrono::milliseconds totalProcessingTime{0};
    std::chrono::milliseconds averageProcessingTime{0};
    std::chrono::milliseconds maxProcessingTime{0};
    std::chrono::milliseconds minProcessingTime{0};

    // Per-priority statistics
    std::vector<size_t> eventsByPriority{4, 0};  // Index by priority level

    void recordEvent(const EventBroadcastResult &result, EventBroadcastPriority priority);
    void reset();

    // Calculate derived metrics
    double getSuccessRate() const;
    double getAverageRegionsPerEvent() const;
};

}  // namespace RSM