#pragma once

#include "runtime/IHistoryManager.h"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace RSM {

// Forward declarations
class IHistoryStateFilter;
class IHistoryValidator;
class IStateNode;

/**
 * @brief Main history manager implementation (SOLID compliant)
 *
 * Implements the IHistoryManager interface following SOLID principles:
 * - Single Responsibility: Manages history state operations only
 * - Open/Closed: Extensible through filter and validator injection
 * - Liskov Substitution: Fully implements IHistoryManager contract
 * - Interface Segregation: Uses focused interfaces for filters/validators
 * - Dependency Inversion: Depends on abstractions, not concretions
 */
class HistoryManager : public IHistoryManager {
public:
    /**
     * @brief Constructor with dependency injection
     * @param stateProvider Function to get state by ID
     * @param shallowFilter Filter for shallow history operations
     * @param deepFilter Filter for deep history operations
     * @param validator Validator for history operations
     */
    HistoryManager(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider,
                   std::unique_ptr<IHistoryStateFilter> shallowFilter, std::unique_ptr<IHistoryStateFilter> deepFilter,
                   std::unique_ptr<IHistoryValidator> validator);

    // IHistoryManager interface implementation
    bool registerHistoryState(const std::string &historyStateId, const std::string &parentStateId, HistoryType type,
                              const std::string &defaultStateId = "") override;

    bool recordHistory(const std::string &parentStateId, const std::vector<std::string> &activeStateIds) override;

    HistoryRestorationResult restoreHistory(const std::string &historyStateId) override;

    bool isHistoryState(const std::string &stateId) const override;

    void clearAllHistory() override;

    std::vector<HistoryEntry> getHistoryEntries() const override;

private:
    // Dependencies (Dependency Inversion Principle)
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider_;
    std::unique_ptr<IHistoryStateFilter> shallowFilter_;
    std::unique_ptr<IHistoryStateFilter> deepFilter_;
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
     * @brief Get appropriate filter for history type
     * @param type History type
     * @return Reference to filter
     */
    IHistoryStateFilter &getFilter(HistoryType type) const;

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