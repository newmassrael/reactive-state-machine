#include "states/ConcurrentEventBroadcaster.h"
#include "events/EventDescriptor.h"
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>

namespace RSM {

ConcurrentEventBroadcaster::ConcurrentEventBroadcaster(const EventBroadcastConfig &config) : config_(config) {
    Logger::debug("ConcurrentEventBroadcaster::Constructor - Creating event broadcaster");
}

ConcurrentEventBroadcaster::~ConcurrentEventBroadcaster() {
    Logger::debug("ConcurrentEventBroadcaster::Destructor - Destroying event broadcaster");
}

EventBroadcastResult ConcurrentEventBroadcaster::broadcastEvent(const EventBroadcastRequest &request) {
    auto startTime = std::chrono::system_clock::now();

    Logger::debug("ConcurrentEventBroadcaster::broadcastEvent() - Broadcasting event: " + request.event.eventName +
                  " with priority: " + std::to_string(static_cast<int>(request.priority)));

    // Get target regions based on scope
    auto targetRegions = getTargetRegions(request);

    if (targetRegions.empty()) {
        auto result = EventBroadcastResult::failure("No target regions available for broadcasting");
        updateStatistics(result, request.priority, startTime);
        return result;
    }

    EventBroadcastResult result;

    // Choose broadcasting strategy based on configuration
    if (config_.parallelProcessing && targetRegions.size() > 1) {
        result = broadcastToRegionsParallel(request.event, targetRegions, config_);
    } else {
        result = broadcastToRegionsSequential(request.event, targetRegions, config_);
    }

    // Update statistics and log operation
    updateStatistics(result, request.priority, startTime);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime);
    result.processingTime = duration;

    logBroadcastOperation(request, result, duration);

    // Call completion callback if set
    if (eventCallback_) {
        eventCallback_(request, result);
    }

    return result;
}

EventBroadcastResult ConcurrentEventBroadcaster::broadcastEvent(const EventDescriptor &event) {
    EventBroadcastRequest request;
    request.event = event;
    request.priority = config_.defaultPriority;
    request.scope = config_.defaultScope;
    request.timestamp = std::chrono::system_clock::now();
    request.correlationId = generateCorrelationId();

    return broadcastEvent(request);
}

EventBroadcastResult
ConcurrentEventBroadcaster::broadcastEventToRegions(const EventDescriptor &event,
                                                    const std::vector<std::string> &targetRegions) {
    EventBroadcastRequest request;
    request.event = event;
    request.priority = config_.defaultPriority;
    request.scope = EventBroadcastScope::SELECTED_REGIONS;
    request.targetRegions = targetRegions;
    request.timestamp = std::chrono::system_clock::now();
    request.correlationId = generateCorrelationId();

    return broadcastEvent(request);
}

EventBroadcastResult ConcurrentEventBroadcaster::broadcastEventWithPriority(const EventDescriptor &event,
                                                                            EventBroadcastPriority priority) {
    EventBroadcastRequest request;
    request.event = event;
    request.priority = priority;
    request.scope = config_.defaultScope;
    request.timestamp = std::chrono::system_clock::now();
    request.correlationId = generateCorrelationId();

    return broadcastEvent(request);
}

bool ConcurrentEventBroadcaster::registerRegion(std::shared_ptr<IConcurrentRegion> region) {
    if (!region) {
        Logger::warn("ConcurrentEventBroadcaster::registerRegion() - Cannot register null region");
        return false;
    }

    std::lock_guard<std::mutex> lock(regionsMutex_);

    const std::string &regionId = region->getId();

    if (regions_.find(regionId) != regions_.end()) {
        Logger::warn("ConcurrentEventBroadcaster::registerRegion() - Region already registered: " + regionId);
        return false;
    }

    regions_[regionId] = region;
    Logger::debug("ConcurrentEventBroadcaster::registerRegion() - Registered region: " + regionId);

    return true;
}

bool ConcurrentEventBroadcaster::unregisterRegion(const std::string &regionId) {
    std::lock_guard<std::mutex> lock(regionsMutex_);

    auto it = regions_.find(regionId);
    if (it == regions_.end()) {
        Logger::warn("ConcurrentEventBroadcaster::unregisterRegion() - Region not found: " + regionId);
        return false;
    }

    regions_.erase(it);
    Logger::debug("ConcurrentEventBroadcaster::unregisterRegion() - Unregistered region: " + regionId);

    return true;
}

std::vector<std::shared_ptr<IConcurrentRegion>> ConcurrentEventBroadcaster::getRegisteredRegions() const {
    std::lock_guard<std::mutex> lock(regionsMutex_);

    std::vector<std::shared_ptr<IConcurrentRegion>> result;
    result.reserve(regions_.size());

    for (const auto &pair : regions_) {
        result.push_back(pair.second);
    }

    return result;
}

std::vector<std::shared_ptr<IConcurrentRegion>> ConcurrentEventBroadcaster::getActiveRegions() const {
    std::lock_guard<std::mutex> lock(regionsMutex_);

    std::vector<std::shared_ptr<IConcurrentRegion>> result;

    for (const auto &pair : regions_) {
        if (pair.second && pair.second->isActive()) {
            result.push_back(pair.second);
        }
    }

    return result;
}

void ConcurrentEventBroadcaster::setConfiguration(const EventBroadcastConfig &config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
    Logger::debug("ConcurrentEventBroadcaster::setConfiguration() - Configuration updated");
}

const EventBroadcastConfig &ConcurrentEventBroadcaster::getConfiguration() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_;
}

void ConcurrentEventBroadcaster::setEventBroadcastCallback(
    std::function<void(const EventBroadcastRequest &, const EventBroadcastResult &)> callback) {
    eventCallback_ = callback;
    Logger::debug("ConcurrentEventBroadcaster::setEventBroadcastCallback() - Callback set");
}

const EventBroadcastStatistics &ConcurrentEventBroadcaster::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    return statistics_;
}

void ConcurrentEventBroadcaster::resetStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.reset();
    Logger::debug("ConcurrentEventBroadcaster::resetStatistics() - Statistics reset");
}

bool ConcurrentEventBroadcaster::isRegionActive(const std::string &regionId) const {
    std::lock_guard<std::mutex> lock(regionsMutex_);

    auto it = regions_.find(regionId);
    if (it == regions_.end()) {
        return false;
    }

    return it->second && it->second->isActive();
}

size_t ConcurrentEventBroadcaster::getActiveRegionCount() const {
    return getActiveRegions().size();
}

std::vector<std::string> ConcurrentEventBroadcaster::validateConfiguration() const {
    std::vector<std::string> errors;

    std::lock_guard<std::mutex> lock(configMutex_);

    if (config_.timeoutPerRegion.count() <= 0) {
        errors.push_back("timeoutPerRegion must be positive");
    }

    if (config_.totalTimeout.count() <= 0) {
        errors.push_back("totalTimeout must be positive");
    }

    if (config_.timeoutPerRegion > config_.totalTimeout) {
        errors.push_back("timeoutPerRegion cannot be greater than totalTimeout");
    }

    return errors;
}

// Private implementation methods

std::vector<std::shared_ptr<IConcurrentRegion>>
ConcurrentEventBroadcaster::getTargetRegions(const EventBroadcastRequest &request) const {
    std::vector<std::shared_ptr<IConcurrentRegion>> targets;

    std::lock_guard<std::mutex> lock(regionsMutex_);

    switch (request.scope) {
    case EventBroadcastScope::ALL_ACTIVE_REGIONS: {
        for (const auto &pair : regions_) {
            if (pair.second && pair.second->isActive()) {
                targets.push_back(pair.second);
            }
        }
        break;
    }

    case EventBroadcastScope::SELECTED_REGIONS: {
        for (const std::string &regionId : request.targetRegions) {
            auto it = regions_.find(regionId);
            if (it != regions_.end() && it->second) {
                targets.push_back(it->second);
            }
        }
        break;
    }

    case EventBroadcastScope::CONDITIONAL_REGIONS: {
        if (request.regionFilter) {
            for (const auto &pair : regions_) {
                if (pair.second && request.regionFilter(pair.second)) {
                    targets.push_back(pair.second);
                }
            }
        }
        break;
    }
    }

    return targets;
}

EventBroadcastResult ConcurrentEventBroadcaster::broadcastToRegionsParallel(
    const EventDescriptor &event, const std::vector<std::shared_ptr<IConcurrentRegion>> &targetRegions,
    const EventBroadcastConfig &config) {
    Logger::debug("ConcurrentEventBroadcaster::broadcastToRegionsParallel() - Broadcasting to " +
                  std::to_string(targetRegions.size()) + " regions in parallel");

    std::vector<std::future<ConcurrentOperationResult>> futures;
    futures.reserve(targetRegions.size());

    // Start parallel processing
    for (const auto &region : targetRegions) {
        futures.push_back(processEventInRegion(region, event, config.timeoutPerRegion));
    }

    // Collect results
    std::vector<std::string> successfulRegions;
    std::vector<std::string> failedRegions;
    std::string combinedError;

    for (size_t i = 0; i < futures.size(); ++i) {
        try {
            auto result = futures[i].get();

            if (result.isSuccess) {
                successfulRegions.push_back(result.regionId);
            } else {
                failedRegions.push_back(result.regionId);
                if (!combinedError.empty()) {
                    combinedError += "; ";
                }
                combinedError += result.errorMessage;
            }
        } catch (const std::exception &e) {
            const std::string &regionId = targetRegions[i]->getId();
            failedRegions.push_back(regionId);
            if (!combinedError.empty()) {
                combinedError += "; ";
            }
            combinedError += "Exception in region " + regionId + ": " + e.what();
        }
    }

    // Determine overall result
    if (failedRegions.empty()) {
        return EventBroadcastResult::success(successfulRegions);
    } else if (successfulRegions.empty()) {
        return EventBroadcastResult::failure(combinedError, successfulRegions, failedRegions);
    } else {
        return EventBroadcastResult::partial(successfulRegions, failedRegions, combinedError);
    }
}

EventBroadcastResult ConcurrentEventBroadcaster::broadcastToRegionsSequential(
    const EventDescriptor &event, const std::vector<std::shared_ptr<IConcurrentRegion>> &targetRegions,
    const EventBroadcastConfig &config) {
    Logger::debug("ConcurrentEventBroadcaster::broadcastToRegionsSequential() - Broadcasting to " +
                  std::to_string(targetRegions.size()) + " regions sequentially");

    std::vector<std::string> successfulRegions;
    std::vector<std::string> failedRegions;
    std::string combinedError;

    for (const auto &region : targetRegions) {
        try {
            auto result = region->processEvent(event);

            if (result.isSuccess) {
                successfulRegions.push_back(result.regionId);
            } else {
                failedRegions.push_back(result.regionId);
                if (!combinedError.empty()) {
                    combinedError += "; ";
                }
                combinedError += result.errorMessage;

                if (config.stopOnFirstFailure) {
                    break;
                }
            }
        } catch (const std::exception &e) {
            const std::string &regionId = region->getId();
            failedRegions.push_back(regionId);
            if (!combinedError.empty()) {
                combinedError += "; ";
            }
            combinedError += "Exception in region " + regionId + ": " + e.what();

            if (config.stopOnFirstFailure) {
                break;
            }
        }
    }

    // Determine overall result
    if (failedRegions.empty()) {
        return EventBroadcastResult::success(successfulRegions);
    } else if (successfulRegions.empty()) {
        return EventBroadcastResult::failure(combinedError, successfulRegions, failedRegions);
    } else {
        return EventBroadcastResult::partial(successfulRegions, failedRegions, combinedError);
    }
}

std::future<ConcurrentOperationResult>
ConcurrentEventBroadcaster::processEventInRegion(std::shared_ptr<IConcurrentRegion> region,
                                                 const EventDescriptor &event, std::chrono::milliseconds timeout) {
    return std::async(std::launch::async, [region, event, timeout]() -> ConcurrentOperationResult {
        try {
            // Simple implementation without timeout for now
            // In a production system, you would implement proper timeout handling
            return region->processEvent(event);
        } catch (const std::exception &e) {
            return ConcurrentOperationResult::failure(region->getId(),
                                                      "Exception during event processing: " + std::string(e.what()));
        }
    });
}

void ConcurrentEventBroadcaster::updateStatistics(const EventBroadcastResult &result, EventBroadcastPriority priority,
                                                  std::chrono::system_clock::time_point /* startTime */
) {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.recordEvent(result, priority);
}

bool ConcurrentEventBroadcaster::validateRegion(const std::shared_ptr<IConcurrentRegion> &region) const {
    if (!region) {
        return false;
    }

    if (!config_.validateRegionState) {
        return true;
    }

    // Additional validation logic can be added here
    return true;
}

std::string ConcurrentEventBroadcaster::generateCorrelationId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; ++i) {
        ss << dis(gen);
    }

    return ss.str();
}

void ConcurrentEventBroadcaster::logBroadcastOperation(const EventBroadcastRequest &request,
                                                       const EventBroadcastResult &result,
                                                       std::chrono::milliseconds duration) const {
    std::stringstream logMessage;
    logMessage << "ConcurrentEventBroadcaster::broadcastEvent() - ";
    logMessage << "Event: " << request.event.eventName;
    logMessage << ", Success: " << (result.isSuccess ? "true" : "false");
    logMessage << ", Successful regions: " << result.successfulRegions.size();
    logMessage << ", Failed regions: " << result.failedRegions.size();
    logMessage << ", Duration: " << duration.count() << "ms";

    if (result.isSuccess) {
        Logger::debug(logMessage.str());
    } else {
        Logger::warn(logMessage.str() + ", Error: " + result.errorMessage);
    }
}

}  // namespace RSM