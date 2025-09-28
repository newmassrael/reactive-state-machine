#pragma once

#include "runtime/HistoryManager.h"
#include <functional>
#include <memory>

namespace RSM {

// Forward declarations
class IStateNode;

/**
 * @brief Deep history filter implementation (Strategy Pattern)
 *
 * Filters states to record the complete nested configuration within the parent state.
 * According to SCXML W3C specification, deep history remembers the complete
 * state configuration that was active within the compound state.
 */
class DeepHistoryFilter : public IHistoryStateFilter {
public:
    /**
     * @brief Constructor with state hierarchy access
     * @param stateProvider Function to get state by ID (Dependency Injection)
     */
    explicit DeepHistoryFilter(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider);

    /**
     * @brief Filter active states for deep history recording
     * @param activeStateIds All currently active states
     * @param parentStateId Parent compound state
     * @return All descendant states of parent that are active
     */
    std::vector<std::string> filterStates(const std::vector<std::string> &activeStateIds,
                                          const std::string &parentStateId) const override;

private:
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider_;

    /**
     * @brief Check if a state is a descendant of the parent state
     * @param stateId Candidate state ID
     * @param parentStateId Parent state ID
     * @return true if stateId is descendant of parentStateId
     */
    bool isDescendant(const std::string &stateId, const std::string &parentStateId) const;

    /**
     * @brief Get the path from root to a state
     * @param stateId Target state ID
     * @return Vector of state IDs from root to target
     */
    std::vector<std::string> getStatePath(const std::string &stateId) const;
};

}  // namespace RSM