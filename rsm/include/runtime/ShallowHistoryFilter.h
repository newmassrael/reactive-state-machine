#pragma once

#include "runtime/HistoryManager.h"
#include <functional>
#include <memory>

namespace RSM {

// Forward declarations
class IStateNode;

/**
 * @brief Shallow history filter implementation (Strategy Pattern)
 *
 * Filters states to record only the immediate child states of the parent state.
 * According to SCXML W3C specification, shallow history remembers only the
 * direct child state that was active when the compound state was last exited.
 */
class ShallowHistoryFilter : public IHistoryStateFilter {
public:
    /**
     * @brief Constructor with state hierarchy access
     * @param stateHierarchy Function to get state by ID (Dependency Injection)
     */
    explicit ShallowHistoryFilter(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider);

    /**
     * @brief Filter active states for shallow history recording
     * @param activeStateIds All currently active states
     * @param parentStateId Parent compound state
     * @return Only the immediate child states of parent that are active
     */
    std::vector<std::string> filterStates(const std::vector<std::string> &activeStateIds,
                                          const std::string &parentStateId) const override;

private:
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider_;

    /**
     * @brief Check if a state is an immediate child of the parent state
     * @param stateId Candidate state ID
     * @param parentStateId Parent state ID
     * @return true if stateId is immediate child of parentStateId
     */
    bool isImmediateChild(const std::string &stateId, const std::string &parentStateId) const;
};

}  // namespace RSM