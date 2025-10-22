#pragma once

#include <algorithm>
#include <optional>
#include <vector>

namespace RSM {

/**
 * @brief Helper functions for parallel state operations (W3C SCXML 3.4)
 *
 * Shared between Interpreter and AOT engines following Zero Duplication Principle.
 * Provides utilities for parallel state structure analysis and document order.
 */
class ParallelStateHelper {
public:
    /**
     * @brief Check if a state is a parallel state
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with state information
     * @param state State to check
     * @return true if state is parallel, false otherwise
     */
    template <typename StateType, typename PolicyType> static bool isParallelState(StateType state) {
        return PolicyType::isParallelState(state);
    }

    /**
     * @brief Get all child regions of a parallel state
     *
     * W3C SCXML 3.4: Parallel states have multiple child regions that are active simultaneously.
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with state hierarchy information
     * @param parallelState The parallel state
     * @return Vector of child region states in document order
     */
    template <typename StateType, typename PolicyType>
    static std::vector<StateType> getParallelRegions(StateType parallelState) {
        return PolicyType::getParallelRegions(parallelState);
    }

    /**
     * @brief Get document order index for a state
     *
     * W3C SCXML 3.13: Document order is used for tie-breaking in exit order.
     * States appearing earlier in SCXML document have lower indices.
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with document order information
     * @param state State to get document order for
     * @return Document order index (0-based)
     */
    template <typename StateType, typename PolicyType> static int getDocumentOrder(StateType state) {
        return PolicyType::getDocumentOrder(state);
    }

    /**
     * @brief Compare two states by document order
     *
     * W3C SCXML 3.13: Used for exit order tie-breaking (reverse document order).
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with document order information
     * @param state1 First state
     * @param state2 Second state
     * @return true if state1 appears before state2 in document order
     */
    template <typename StateType, typename PolicyType>
    static bool compareDocumentOrder(StateType state1, StateType state2) {
        return getDocumentOrder<StateType, PolicyType>(state1) < getDocumentOrder<StateType, PolicyType>(state2);
    }

    /**
     * @brief Get initial states for all child regions of a parallel state
     *
     * W3C SCXML 3.4: When entering a parallel state, all child regions are entered
     * simultaneously, each to their initial state.
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with state hierarchy information
     * @param parallelState The parallel state
     * @return Vector of initial states (one per region) in document order
     */
    template <typename StateType, typename PolicyType>
    static std::vector<StateType> getParallelInitialStates(StateType parallelState) {
        auto regions = getParallelRegions<StateType, PolicyType>(parallelState);
        std::vector<StateType> initialStates;
        initialStates.reserve(regions.size());

        for (auto region : regions) {
            // Get initial child of each region
            if (PolicyType::isCompoundState(region)) {
                initialStates.push_back(PolicyType::getInitialChild(region));
            } else {
                // Atomic state - use itself
                initialStates.push_back(region);
            }
        }

        return initialStates;
    }

    /**
     * @brief Check if all child regions of a parallel state are in final states
     *
     * W3C SCXML 3.4: A parallel state is considered complete when all its child
     * regions are in final states.
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with state information
     * @tparam ConfigurationType Configuration type holding active states
     * @param parallelState The parallel state
     * @param configuration Current configuration
     * @return true if all regions are in final states
     */
    template <typename StateType, typename PolicyType, typename ConfigurationType>
    static bool areAllRegionsFinal(StateType parallelState, const ConfigurationType &configuration) {
        auto regions = getParallelRegions<StateType, PolicyType>(parallelState);

        for (auto region : regions) {
            // Get active state in this region
            auto activeState = configuration.getRegionState(region);
            if (!activeState.has_value() || !PolicyType::isFinalState(activeState.value())) {
                return false;
            }
        }

        return true;
    }
};

}  // namespace RSM
