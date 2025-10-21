#pragma once

#include "events/EventDescriptor.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RSM {

// Forward declarations
class IStateNode;
class IConcurrentRegion;

/**
 * @brief SCXML parallel state completion criteria
 * SCXML specification: Parallel state completes when all child states reach final state
 */
enum class ParallelCompletionCriteria {
    ALL_REGIONS_FINAL,      // All regions in final state (SCXML standard)
    ANY_REGION_FINAL,       // Any region in final state (extension)
    MAJORITY_REGIONS_FINAL  // Majority of regions in final state (extension)
};

/**
 * @brief Region completion information
 * Per-region state tracking according to SCXML specification
 */
struct RegionCompletionInfo {
    std::string regionId;
    bool isInFinalState = false;
    std::vector<std::string> finalStateIds;  // Final states within region
    std::chrono::steady_clock::time_point completionTime;
    std::chrono::steady_clock::time_point lastUpdateTime;

    // SCXML additional information
    std::string currentStateId;               // Currently active state
    std::vector<std::string> activeStateIds;  // All active states (for compound states)
};

/**
 * @brief Parallel state completion information
 * Comprehensive information for generating SCXML done.state events
 */
struct ParallelStateCompletionInfo {
    std::string parallelStateId;
    bool isComplete = false;
    ParallelCompletionCriteria completionCriteria;
    size_t totalRegions = 0;
    size_t completedRegions = 0;
    std::vector<RegionCompletionInfo> regionCompletions;
    std::chrono::steady_clock::time_point completionTime;

    // SCXML done.state event data
    std::string doneEventName;                              // "done.state.{id}"
    std::unordered_map<std::string, std::string> doneData;  // done data
};

/**
 * @brief Completion event type
 * Event classification according to SCXML specification
 */
enum class CompletionEventType {
    PARALLEL_STATE_COMPLETED,  // Parallel state completed (done.state)
    REGION_COMPLETED,          // Individual region completed
    COMPLETION_ERROR           // Completion processing error
};

/**
 * @brief Completion event
 * Representation of SCXML done.state event
 */
struct CompletionEvent {
    CompletionEventType type;
    std::string parallelStateId;
    std::vector<std::string> completedRegions;
    std::chrono::steady_clock::time_point timestamp;
    std::string errorMessage;  // Used only for errors

    // Generate SCXML done.state event
    EventDescriptor toDoneStateEvent() const;
};

/**
 * @brief Parallel state monitoring configuration
 * Configuration for SCXML specification and extension features
 */
struct ParallelMonitoringConfig {
    ParallelCompletionCriteria criteria = ParallelCompletionCriteria::ALL_REGIONS_FINAL;

    // SCXML timing settings
    bool generateDoneEvents = true;        // Whether to generate done.state events
    bool validateStateConsistency = true;  // Validate state consistency

    // Performance and debugging
    bool collectDetailedStatistics = false;             // Collect detailed statistics
    std::chrono::milliseconds monitoringInterval{100};  // Monitoring interval

    // Extension features
    std::unordered_map<std::string, double> regionWeights;  // Per-region weights
    double weightedThreshold = 0.8;                         // Weight-based completion threshold
};

/**
 * @brief Monitoring statistics
 * Statistical information for SCXML performance analysis
 */
struct MonitoringStatistics {
    size_t totalRegionsRegistered = 0;
    size_t totalCompletionEvents = 0;
    size_t totalStatusQueries = 0;
    std::chrono::microseconds averageCompletionCheckTime{0};
    bool isCurrentlyComplete = false;

    // SCXML specification compliance statistics
    size_t doneEventsGenerated = 0;
    size_t stateConsistencyViolations = 0;
    std::chrono::steady_clock::time_point monitoringStartTime;
};

}  // namespace RSM