#include "states/ConcurrentEventTypes.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace RSM {

// EventBroadcastResult static methods

EventBroadcastResult EventBroadcastResult::success(const std::vector<std::string> &successfulRegions,
                                                   std::chrono::milliseconds processingTime) {
    EventBroadcastResult result;
    result.isSuccess = true;
    result.successfulRegions = successfulRegions;
    result.processingTime = processingTime;
    return result;
}

EventBroadcastResult EventBroadcastResult::failure(const std::string &error,
                                                   const std::vector<std::string> &successfulRegions,
                                                   const std::vector<std::string> &failedRegions) {
    EventBroadcastResult result;
    result.isSuccess = false;
    result.errorMessage = error;
    result.successfulRegions = successfulRegions;
    result.failedRegions = failedRegions;
    return result;
}

EventBroadcastResult EventBroadcastResult::partial(const std::vector<std::string> &successfulRegions,
                                                   const std::vector<std::string> &failedRegions,
                                                   const std::string &error) {
    EventBroadcastResult result;
    result.isSuccess = !successfulRegions.empty();
    result.successfulRegions = successfulRegions;
    result.failedRegions = failedRegions;
    result.errorMessage = error;
    return result;
}

// EventBroadcastStatistics methods

void EventBroadcastStatistics::recordEvent(const EventBroadcastResult &result, EventBroadcastPriority priority) {
    totalEvents++;

    if (result.isSuccess) {
        if (result.failedRegions.empty()) {
            successfulEvents++;
        } else {
            partialEvents++;
        }
    } else {
        failedEvents++;
    }

    // Update timing statistics
    totalProcessingTime += result.processingTime;

    if (totalEvents == 1) {
        minProcessingTime = result.processingTime;
        maxProcessingTime = result.processingTime;
    } else {
        minProcessingTime = std::min(minProcessingTime, result.processingTime);
        maxProcessingTime = std::max(maxProcessingTime, result.processingTime);
    }

    // Calculate average
    if (totalEvents > 0) {
        averageProcessingTime = totalProcessingTime / totalEvents;
    }

    // Update priority statistics
    size_t priorityIndex = static_cast<size_t>(priority) - 1;
    if (priorityIndex < eventsByPriority.size()) {
        eventsByPriority[priorityIndex]++;
    }
}

void EventBroadcastStatistics::reset() {
    totalEvents = 0;
    successfulEvents = 0;
    failedEvents = 0;
    partialEvents = 0;

    totalProcessingTime = std::chrono::milliseconds{0};
    averageProcessingTime = std::chrono::milliseconds{0};
    maxProcessingTime = std::chrono::milliseconds{0};
    minProcessingTime = std::chrono::milliseconds{0};

    std::fill(eventsByPriority.begin(), eventsByPriority.end(), 0);
}

double EventBroadcastStatistics::getSuccessRate() const {
    if (totalEvents == 0) {
        return 0.0;
    }

    return static_cast<double>(successfulEvents + partialEvents) / totalEvents;
}

double EventBroadcastStatistics::getAverageRegionsPerEvent() const {
    if (totalEvents == 0) {
        return 0.0;
    }

    // This would need to be tracked separately
    // For now, return a placeholder
    return 0.0;
}

}  // namespace RSM