#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

// Forward declarations
class IConcurrentRegion;
class Event;

/**
 * @brief Result of concurrent region operations
 */
struct ConcurrentOperationResult {
    bool isSuccess = false;
    std::string errorMessage;
    std::string regionId;

    static ConcurrentOperationResult success(const std::string &regionId) {
        ConcurrentOperationResult result;
        result.isSuccess = true;
        result.regionId = regionId;
        return result;
    }

    static ConcurrentOperationResult failure(const std::string &regionId, const std::string &error) {
        ConcurrentOperationResult result;
        result.isSuccess = false;
        result.regionId = regionId;
        result.errorMessage = error;
        return result;
    }
};

/**
 * @brief Status of a concurrent region
 */
enum class ConcurrentRegionStatus {
    INACTIVE,  // Region is not active
    ACTIVE,    // Region is active and running
    FINAL,     // Region has reached a final state
    ERROR      // Region is in an error state
};

/**
 * @brief Configuration for concurrent state behavior (SCXML W3C compliant)
 *
 * SCXML specification mandates strict behavior for parallel states:
 * - Parallel states MUST have at least one region (section 3.4)
 * - ALL regions MUST complete for parallel state completion (section 3.4)
 * - Events MUST be broadcast to all active regions (section 3.4)
 */
struct ConcurrentStateConfig {
    // SCXML W3C compliance: parallel states must have regions
    // No fallback for empty regions - this violates SCXML specification

    // SCXML W3C compliance: ALL regions must complete for state completion
    // No configuration option - this is mandated by specification

    // SCXML W3C compliance: events must be broadcast to all regions
    // No configuration option - this is mandated by specification

    // Reserved for future SCXML-compliant extensions only
    bool _reserved_for_future_scxml_extensions = false;
};

/**
 * @brief Information about a concurrent region
 */
struct ConcurrentRegionInfo {
    std::string id;
    ConcurrentRegionStatus status;
    std::string currentState;
    bool isInFinalState;
    std::vector<std::string> activeStates;  // For compound regions
};

}  // namespace RSM