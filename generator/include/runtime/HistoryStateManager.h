#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SCXML {

// Forward declarations
namespace Model {
class IStateNode;
class ITransitionNode;
}

namespace Core {
class ITransitionNode;
}

namespace Runtime {

/**
 * @brief History State Manager for SCXML history state functionality
 *
 * Implements W3C SCXML specification for history states:
 * - Shallow history: Remembers only the direct child state
 * - Deep history: Remembers the entire nested state configuration
 *
 * History states enable state machines to return to previously active
 * configurations when re-entering compound states.
 */
class HistoryStateManager {
public:
    /**
     * @brief Type of history state
     */
    enum class HistoryType {
        SHALLOW,  // Records only the immediate child state
        DEEP      // Records the complete nested state configuration
    };

    /**
     * @brief History entry information
     */
    struct HistoryEntry {
        std::string parentStateId;                        // Parent compound state
        HistoryType type;                                 // Shallow or deep history
        std::vector<std::string> recordedStates;          // States that were active
        std::chrono::steady_clock::time_point timestamp;  // When history was recorded
        bool isValid = true;                              // Whether this history is still valid
    };

    /**
     * @brief History restoration result
     */
    struct RestorationResult {
        bool success = false;                     // Whether restoration succeeded
        std::vector<std::string> restoredStates;  // States that were restored
        std::vector<std::string> defaultStates;   // States used when no history available
        std::string errorMessage;                 // Error description if failed
    };

public:
    /**
     * @brief Constructor
     */
    HistoryStateManager();

    /**
     * @brief Destructor
     */
    ~HistoryStateManager() = default;

    /**
     * @brief Register a history state
     * @param historyStateId ID of the history state
     * @param parentStateId ID of the parent compound state
     * @param type Type of history (shallow or deep)
     * @param defaultStateId Default state if no history exists
     * @return true if registered successfully
     */
    bool registerHistoryState(const std::string &historyStateId, const std::string &parentStateId, HistoryType type,
                              const std::string &defaultStateId = "");

    /**
     * @brief Record history when exiting a compound state
     * @param parentStateId ID of the compound state being exited
     * @param activeStates Currently active state configuration
     * @return true if history was recorded successfully
     */
    bool recordHistory(const std::string &parentStateId, const std::vector<std::string> &activeStates);

    /**
     * @brief Restore history when entering a history state
     * @param historyStateId ID of the history state being entered
     * @param currentActiveStates Currently active states (for validation)
     * @return Restoration result with target states
     */
    RestorationResult restoreHistory(const std::string &historyStateId,
                                     const std::vector<std::string> &currentActiveStates);

    /**
     * @brief Clear history for a specific parent state
     * @param parentStateId ID of the parent state
     * @return true if history was cleared
     */
    bool clearHistory(const std::string &parentStateId);

    /**
     * @brief Clear all recorded history
     */
    void clearAllHistory();

    /**
     * @brief Check if history exists for a parent state
     * @param parentStateId ID of the parent state
     * @return true if history exists and is valid
     */
    bool hasValidHistory(const std::string &parentStateId) const;

    /**
     * @brief Get history entry for a parent state
     * @param parentStateId ID of the parent state
     * @return History entry if found, nullptr otherwise
     */
    std::shared_ptr<const HistoryEntry> getHistoryEntry(const std::string &parentStateId) const;

    /**
     * @brief Get all registered history states
     * @return Map of history state ID to parent state ID
     */
    std::unordered_map<std::string, std::string> getRegisteredHistoryStates() const;

    /**
     * @brief Validate that states are valid targets for history restoration
     * @param states States to validate
     * @param parentStateId Parent state context
     * @return true if all states are valid
     */
    bool validateHistoryStates(const std::vector<std::string> &states, const std::string &parentStateId) const;

    /**
     * @brief Get history type for a history state
     * @param historyStateId ID of the history state
     * @return History type, or nullopt if not found
     */
    std::optional<HistoryType> getHistoryType(const std::string &historyStateId) const;

    /**
     * @brief Get default state for a history state
     * @param historyStateId ID of the history state
     * @return Default state ID, empty if not set
     */
    std::string getDefaultState(const std::string &historyStateId) const;

private:
    /**
     * @brief History state registration info
     */
    struct HistoryStateInfo {
        std::string historyStateId;
        std::string parentStateId;
        HistoryType type;
        std::string defaultStateId;
        std::chrono::steady_clock::time_point registrationTime;
    };

    /**
     * @brief Filter states for shallow history
     * @param allStates All currently active states
     * @param parentStateId Parent state to filter for
     * @return Direct child states only
     */
    std::vector<std::string> filterShallowStates(const std::vector<std::string> &allStates,
                                                 const std::string &parentStateId) const;

    /**
     * @brief Filter states for deep history
     * @param allStates All currently active states
     * @param parentStateId Parent state to filter for
     * @return All nested states within parent
     */
    std::vector<std::string> filterDeepStates(const std::vector<std::string> &allStates,
                                              const std::string &parentStateId) const;

    /**
     * @brief Check if a state is a direct child of parent
     * @param stateId State to check
     * @param parentStateId Parent state
     * @return true if direct child
     */
    bool isDirectChild(const std::string &stateId, const std::string &parentStateId) const;

    /**
     * @brief Check if a state is a descendant of parent (any nesting level)
     * @param stateId State to check
     * @param parentStateId Parent state
     * @return true if descendant
     */
    bool isDescendant(const std::string &stateId, const std::string &parentStateId) const;

    /**
     * @brief Check if a state is atomic (leaf state with no children)
     * @param stateId State to check
     * @return true if atomic state
     */
    bool isAtomicState(const std::string &stateId) const;

    /**
     * @brief Expand shallow history states to include parent state
     * @param originalStates Original recorded states
     * @param parentStateId Root parent state
     * @return States including parent state for proper state machine operation
     */
    std::vector<std::string> expandShallowHistoryStates(const std::vector<std::string> &originalStates,
                                                        const std::string &parentStateId) const;

    /**
     * @brief Expand deep history states to include all intermediate parent states
     * @param originalStates Original recorded states
     * @param parentStateId Root parent state
     * @return Complete state hierarchy including all intermediate parents
     */
    std::vector<std::string> expandDeepHistoryStates(const std::vector<std::string> &originalStates,
                                                     const std::string &parentStateId) const;

    /**
     * @brief Get parent state ID for a state
     * @param stateId State to get parent for
     * @return Parent state ID, empty if root or not found
     */
    std::string getParentStateId(const std::string &stateId) const;

    /**
     * @brief Validate history entry before restoration
     * @param entry History entry to validate
     * @return true if entry is valid for restoration
     */
    bool validateHistoryEntry(const HistoryEntry &entry) const;

    // Thread safety
    mutable std::mutex historyMutex_;

    // History state registrations
    std::unordered_map<std::string, HistoryStateInfo> historyStates_;    // historyStateId -> info
    std::unordered_map<std::string, std::string> parentToHistoryState_;  // parentStateId -> historyStateId

    // Recorded history
    std::unordered_map<std::string, std::shared_ptr<HistoryEntry>> recordedHistory_;  // parentStateId -> history

    // State hierarchy cache (for performance)
    mutable std::unordered_map<std::string, std::string> parentCache_;                 // stateId -> parentStateId
    mutable std::unordered_map<std::string, std::vector<std::string>> childrenCache_;  // parentId -> [childIds]
};

/**
 * @brief History State Helper utilities
 */
class HistoryStateHelper {
public:
    /**
     * @brief Extract history state information from SCXML model
     * @param stateNode State node to examine
     * @return History state info if this is a history state
     */
    struct HistoryStateInfo {
        bool isHistoryState = false;
        HistoryStateManager::HistoryType type;
        std::string defaultStateId;
        std::string parentStateId;
    };

    static HistoryStateInfo analyzeHistoryState(std::shared_ptr<const Model::IStateNode> stateNode);

    /**
     * @brief Check if a state ID represents a history state
     * @param stateId State ID to check
     * @return true if it's a history state based on naming convention or registration
     */
    static bool isHistoryStateId(const std::string &stateId);

    /**
     * @brief Register a history state ID for global tracking
     * @param stateId History state ID to register
     */
    static void registerHistoryStateId(const std::string &stateId);

    /**
     * @brief Unregister a history state ID from global tracking
     * @param stateId History state ID to unregister
     */
    static void unregisterHistoryStateId(const std::string &stateId);

    /**
     * @brief Generate history state ID for a parent state
     * @param parentStateId Parent state ID
     * @param type History type
     * @return Generated history state ID
     */
    static std::string generateHistoryStateId(const std::string &parentStateId, HistoryStateManager::HistoryType type);

    /**
     * @brief Parse history type from string
     * @param typeStr Type string ("shallow" or "deep")
     * @return History type enum
     */
    static HistoryStateManager::HistoryType parseHistoryType(const std::string &typeStr);

    /**
     * @brief Convert history type to string
     * @param type History type enum
     * @return Type string
     */
    static std::string historyTypeToString(HistoryStateManager::HistoryType type);
};

}  // namespace Runtime
}  // namespace SCXML