#pragma once

#include <algorithm>
#include <vector>

namespace RSM {

/**
 * @brief Helper for parallel state completion detection (W3C SCXML 3.4, 3.7.1)
 *
 * Single Source of Truth for "all regions in final state" logic.
 * Shared between Interpreter and AOT engines following Zero Duplication Principle.
 *
 * W3C SCXML 3.4: "When all of the children reach final states,
 * the <parallel> element itself is considered to be in a final state"
 *
 * W3C SCXML 3.7.1: "done.state.id event is generated upon completion"
 *
 * ARCHITECTURE.md Compliance:
 * - Zero Duplication: Single implementation shared by both engines
 * - Helper Pattern: Follows SendHelper, ForeachHelper, HistoryHelper patterns
 */
class ParallelCompletionHelper {
public:
    /**
     * @brief Check if all child regions of a parallel state are in final states
     *
     * W3C SCXML 3.4: Parallel state is complete when ALL child regions
     * have at least one active final state.
     *
     * Algorithm:
     * 1. Get all child regions of the parallel state
     * 2. For each region, check if any of its final states are active
     * 3. Return true only if ALL regions have an active final state
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class providing state information methods:
     *                    - getParallelRegions(StateType) -> vector of region IDs
     *                    - getParent(StateType) -> parent state ID
     *                    - isFinalState(StateType) -> bool
     * @param parallelState The parallel state to check for completion
     * @param activeStates Vector of currently active states
     * @return true if all regions have at least one active final state, false otherwise
     */
    template <typename StateType, typename PolicyType>
    static bool areAllRegionsInFinal(StateType parallelState, const std::vector<StateType> &activeStates) {
        // W3C SCXML 3.4: Get all child regions of this parallel state
        auto regions = PolicyType::getParallelRegions(parallelState);

        if (regions.empty()) {
            // No regions means not a valid parallel state
            return false;
        }

        // Check each region for completion
        for (auto region : regions) {
            bool regionHasFinalState = false;

            // Check if any final state child of this region is currently active
            for (StateType activeState : activeStates) {
                // W3C SCXML 3.4: A region is complete if any of its final children are active
                if (PolicyType::getParent(activeState) == region && PolicyType::isFinalState(activeState)) {
                    regionHasFinalState = true;
                    break;
                }
            }

            if (!regionHasFinalState) {
                // At least one region is not complete yet
                return false;
            }
        }

        // W3C SCXML 3.4: All regions have final states active
        return true;
    }
};

}  // namespace RSM
