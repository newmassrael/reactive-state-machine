#pragma once

#include "types.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

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
    bool isRestoredFromRecording = false;     // True if restored from recorded history, false if using default

    static HistoryRestorationResult createSuccess(const std::vector<std::string> &states, bool fromRecording = false) {
        HistoryRestorationResult result;
        result.success = true;
        result.targetStateIds = states;
        result.isRestoredFromRecording = fromRecording;
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

class IHistoryValidator;

/**
 * @brief Main history manager implementation
 *
 * W3C SCXML 3.11: Manages history state operations using shared HistoryHelper
 * for filtering logic (Zero Duplication with AOT engine)
 *
 * Responsibilities:
 * - Register history states for parent compound states
 * - Record active state configurations before exit
 * - Restore recorded configurations or use default transitions
 * - Validate history operations
 */
class HistoryManager {
public:
    /**
     * @brief Constructor with dependency injection
     *
     * W3C SCXML 3.11: Uses shared HistoryHelper for filtering (Zero Duplication with AOT)
     *
     * @param stateProvider Function to get state by ID
     * @param validator Validator for history operations
     */
    HistoryManager(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider,
                   std::unique_ptr<IHistoryValidator> validator);

    // History manager interface
    virtual ~HistoryManager() = default;
    /**
     * @brief Register a history state for tracking
     * @param historyStateId ID of the history state
     * @param parentStateId ID of the parent compound state
     * @param type History type (SHALLOW or DEEP)
     * @param defaultStateId Default state if no history available
     * @return true if registration succeeded
     */
    virtual bool registerHistoryState(const std::string &historyStateId, const std::string &parentStateId,
                                      HistoryType type, const std::string &defaultStateId = "");

    /**
     * @brief Record current state configuration when exiting a compound state
     * @param parentStateId ID of the compound state being exited
     * @param activeStateIds Currently active state configuration
     * @return true if history was recorded successfully
     */
    virtual bool recordHistory(const std::string &parentStateId, const std::vector<std::string> &activeStateIds);

    /**
     * @brief Restore history when entering a history state
     * @param historyStateId ID of the history state being entered
     * @return Restoration result with target states or error
     */
    virtual HistoryRestorationResult restoreHistory(const std::string &historyStateId);

    /**
     * @brief Check if a state ID represents a history state
     * @param stateId State ID to check
     * @return true if it's a history state
     */
    virtual bool isHistoryState(const std::string &stateId) const;

    /**
     * @brief Clear all recorded history (for testing/reset purposes)
     */
    virtual void clearAllHistory();

    /**
     * @brief Get history information for debugging
     * @return Vector of all recorded history entries
     */
    virtual std::vector<HistoryEntry> getHistoryEntries() const;

private:
    // Dependencies
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider_;
    std::unique_ptr<IHistoryValidator> validator_;

    // Thread safety
    mutable std::mutex historyMutex_;

    // Data structures
    struct HistoryStateInfo {
        std::string historyStateId;
        std::string parentStateId;
        HistoryType type;
        std::string defaultStateId;
        std::chrono::steady_clock::time_point registrationTime;
    };

    std::unordered_map<std::string, HistoryStateInfo> historyStates_;  // historyStateId -> info
    std::unordered_map<std::string, HistoryEntry> recordedHistory_;    // historyStateId -> entry

    /**
     * @brief Find history states for a parent state
     * @param parentStateId Parent state ID
     * @return Vector of history state infos for the parent
     */
    std::vector<HistoryStateInfo> findHistoryStatesForParent(const std::string &parentStateId) const;

    /**
     * @brief Get default states if no history is available
     * @param historyStateInfo History state information
     * @return Vector of default state IDs
     */
    std::vector<std::string> getDefaultStates(const HistoryStateInfo &historyStateInfo) const;
};

}  // namespace RSM