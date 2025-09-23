#pragma once

#include "types.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

// Forward declarations
class IStateNode;

/**
 * @brief History restoration result
 */
struct HistoryRestorationResult {
    bool success = false;                     // Whether restoration succeeded
    std::vector<std::string> targetStateIds;  // States to enter after restoration
    std::string errorMessage;                 // Error description if failed

    static HistoryRestorationResult createSuccess(const std::vector<std::string> &states) {
        HistoryRestorationResult result;
        result.success = true;
        result.targetStateIds = states;
        return result;
    }

    static HistoryRestorationResult createError(const std::string &error) {
        HistoryRestorationResult result;
        result.success = false;
        result.errorMessage = error;
        return result;
    }
};

/**
 * @brief History entry representing a saved state configuration
 */
struct HistoryEntry {
    std::string parentStateId;                        // Parent compound state
    HistoryType type;                                 // Shallow or deep history
    std::vector<std::string> recordedStateIds;        // States that were active
    std::chrono::steady_clock::time_point timestamp;  // When history was recorded
    bool isValid = true;                              // Whether this history is still valid
};

/**
 * @brief Interface for history state management (Single Responsibility Principle)
 *
 * Provides a clear contract for history state operations following SCXML W3C specification:
 * - Shallow history: Records only immediate child states
 * - Deep history: Records complete nested state configuration
 */
class IHistoryManager {
public:
    virtual ~IHistoryManager() = default;

    /**
     * @brief Register a history state for tracking
     * @param historyStateId ID of the history state
     * @param parentStateId ID of the parent compound state
     * @param type History type (SHALLOW or DEEP)
     * @param defaultStateId Default state if no history available
     * @return true if registration succeeded
     */
    virtual bool registerHistoryState(const std::string &historyStateId, const std::string &parentStateId,
                                      HistoryType type, const std::string &defaultStateId = "") = 0;

    /**
     * @brief Record current state configuration when exiting a compound state
     * @param parentStateId ID of the compound state being exited
     * @param activeStateIds Currently active state configuration
     * @return true if history was recorded successfully
     */
    virtual bool recordHistory(const std::string &parentStateId, const std::vector<std::string> &activeStateIds) = 0;

    /**
     * @brief Restore history when entering a history state
     * @param historyStateId ID of the history state being entered
     * @return Restoration result with target states or error
     */
    virtual HistoryRestorationResult restoreHistory(const std::string &historyStateId) = 0;

    /**
     * @brief Check if a state ID represents a history state
     * @param stateId State ID to check
     * @return true if it's a history state
     */
    virtual bool isHistoryState(const std::string &stateId) const = 0;

    /**
     * @brief Clear all recorded history (for testing/reset purposes)
     */
    virtual void clearAllHistory() = 0;

    /**
     * @brief Get history information for debugging
     * @return Vector of all recorded history entries
     */
    virtual std::vector<HistoryEntry> getHistoryEntries() const = 0;
};

/**
 * @brief Interface for filtering states based on history type (Strategy Pattern)
 */
class IHistoryStateFilter {
public:
    virtual ~IHistoryStateFilter() = default;

    /**
     * @brief Filter active states based on history type and parent state
     * @param activeStateIds All currently active states
     * @param parentStateId Parent compound state
     * @return Filtered states to record
     */
    virtual std::vector<std::string> filterStates(const std::vector<std::string> &activeStateIds,
                                                  const std::string &parentStateId) const = 0;
};

/**
 * @brief Interface for validating history operations (Single Responsibility)
 */
class IHistoryValidator {
public:
    virtual ~IHistoryValidator() = default;

    /**
     * @brief Validate that a history state can be registered
     * @param historyStateId History state ID
     * @param parentStateId Parent state ID
     * @param type History type
     * @return true if valid
     */
    virtual bool validateRegistration(const std::string &historyStateId, const std::string &parentStateId,
                                      HistoryType type) const = 0;

    /**
     * @brief Validate that history can be recorded for a parent state
     * @param parentStateId Parent state ID
     * @param activeStateIds Active states
     * @return true if valid
     */
    virtual bool validateRecording(const std::string &parentStateId,
                                   const std::vector<std::string> &activeStateIds) const = 0;

    /**
     * @brief Validate that history can be restored for a history state
     * @param historyStateId History state ID
     * @return true if valid
     */
    virtual bool validateRestoration(const std::string &historyStateId) const = 0;
};

}  // namespace RSM