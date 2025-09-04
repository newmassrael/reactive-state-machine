#pragma once

#include "model/IHistoryNode.h"
#include "runtime/RuntimeContext.h"
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace SCXML {
namespace Runtime {

/**
 * @brief History Manager for SCXML History States
 *
 * Provides history state management according to W3C SCXML specification:
 * - History expiration and cleanup
 * - Conditional history restoration
 * - History state validation
 * - Performance optimizations
 * - History analytics and debugging
 */
class HistoryManager {
public:
    struct HistoryRecord {
        std::set<std::string> states;
        std::chrono::steady_clock::time_point timestamp;
        std::string parentState;
        HistoryType type;
        size_t entryCount = 0;
        bool isValid = true;

        // Additional metadata
        std::unordered_map<std::string, std::string> metadata;
    };

    struct HistoryMetrics {
        size_t totalRecords = 0;
        size_t expiredRecords = 0;
        size_t deepHistoryRecords = 0;
        size_t shallowHistoryRecords = 0;
        double averageStatesPerRecord = 0.0;
        std::chrono::milliseconds oldestRecord{0};
        std::chrono::milliseconds newestRecord{0};
    };

    /**
     * @brief Constructor
     * @param context Runtime context
     */
    explicit AdvancedHistoryManager(SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Record history with enhanced features
     * @param historyId History node ID
     * @param activeStates Current active states
     * @param type History type (shallow/deep)
     * @param conditions Optional conditions for when this history is valid
     * @return Success/failure result
     */
    SCXML::Common::Result<void>
    recordAdvancedHistory(const std::string &historyId, const std::set<std::string> &activeStates, HistoryType type,
                          const std::unordered_map<std::string, std::string> &conditions = {});

    /**
     * @brief Restore history with validation and conditions
     * @param historyId History node ID
     * @param validateConditions Whether to check restoration conditions
     * @return Set of states to restore, or empty if invalid/expired
     */
    SCXML::Common::Result<std::set<std::string>> restoreAdvancedHistory(const std::string &historyId,
                                                                        bool validateConditions = true);

    /**
     * @brief Set history expiration time
     * @param historyId History node ID
     * @param expirationMs Expiration time in milliseconds (0 = never expire)
     */
    void setHistoryExpiration(const std::string &historyId, std::chrono::milliseconds expirationMs);

    /**
     * @brief Clear expired history records
     * @return Number of records cleared
     */
    size_t clearExpiredHistory();

    /**
     * @brief Validate history record integrity
     * @param historyId History node ID
     * @return Validation result with details
     */
    SCXML::Common::Result<bool> validateHistory(const std::string &historyId);

    /**
     * @brief Get history metrics for debugging/analytics
     * @return Current metrics
     */
    HistoryMetrics getMetrics() const;

    /**
     * @brief Export history state for debugging
     * @param includeExpired Whether to include expired records
     * @return JSON string with history data
     */
    std::string exportHistoryState(bool includeExpired = false) const;

    /**
     * @brief Clear all history for a given parent state
     * @param parentStateId Parent state ID
     * @return Number of records cleared
     */
    size_t clearHistoryForState(const std::string &parentStateId);

    /**
     * @brief Check if history exists and is valid
     * @param historyId History node ID
     * @return True if valid history exists
     */
    bool hasValidHistory(const std::string &historyId) const;

    /**
     * @brief Set custom metadata for history record
     * @param historyId History node ID
     * @param key Metadata key
     * @param value Metadata value
     */
    void setHistoryMetadata(const std::string &historyId, const std::string &key, const std::string &value);

    /**
     * @brief Enable/disable history analytics
     * @param enabled Whether to collect detailed analytics
     */
    void enableAnalytics(bool enabled) {
        analyticsEnabled_ = enabled;
    }

private:
    SCXML::Runtime::RuntimeContext &context_;
    std::unordered_map<std::string, HistoryRecord> historyRecords_;
    std::unordered_map<std::string, std::chrono::milliseconds> expirationTimes_;
    bool analyticsEnabled_ = false;

    /**
     * @brief Filter states based on history type and parent
     * @param activeStates All active states
     * @param parentState Parent state ID
     * @param type History type
     * @return Filtered states
     */
    std::set<std::string> filterStatesForHistory(const std::set<std::string> &activeStates,
                                                 const std::string &parentState, HistoryType type) const;

    /**
     * @brief Check if history record has expired
     * @param historyId History node ID
     * @return True if expired
     */
    bool isHistoryExpired(const std::string &historyId) const;

    /**
     * @brief Validate restoration conditions
     * @param record History record to validate
     * @return True if conditions are met
     */
    bool validateRestorationConditions(const HistoryRecord &record) const;

    /**
     * @brief Update analytics when history is recorded/restored
     * @param historyId History node ID
     * @param operation Operation type ("record", "restore", "expire")
     */
    void updateAnalytics(const std::string &historyId, const std::string &operation);

    /**
     * @brief Get state hierarchy depth
     * @param stateId State ID
     * @param parentState Parent state ID
     * @return Depth level (0 = direct child)
     */
    int getStateDepth(const std::string &stateId, const std::string &parentState) const;

    /**
     * @brief Check if state is descendant of parent
     * @param stateId State to check
     * @param parentState Potential parent
     * @return True if stateId is descendant of parentState
     */
    bool isDescendantOf(const std::string &stateId, const std::string &parentState) const;
};

}  // namespace Runtime
}  // namespace SCXML