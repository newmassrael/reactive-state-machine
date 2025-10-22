#pragma once

#include "common/HierarchicalStateHelper.h"
#include <algorithm>
#include <unordered_set>
#include <vector>

namespace RSM {

/**
 * @brief Helper functions for parallel state exit/entry order computation
 *
 * W3C SCXML 3.13: Exit order (children before parents, reverse document order for ties)
 * W3C SCXML 3.4: Parallel states exit all children simultaneously (in proper order)
 *
 * Shared between Interpreter and AOT engines following Zero Duplication Principle.
 */
class ParallelExitEntryHelper {
public:
    /**
     * @brief Compute exit order for states
     *
     * W3C SCXML 3.13: States are exited in exit order:
     * 1. Children before parents
     * 2. Reverse document order for tie-breaking (states appearing later exit first)
     *
     * For parallel states: All child regions are exited before the parallel state itself.
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with state hierarchy and document order
     * @param activeStates Currently active states (may be multiple in parallel)
     * @param targetStates Target states of transition
     * @return Vector of states in exit order
     */
    template <typename StateType, typename PolicyType>
    static std::vector<StateType> computeExitOrder(const std::vector<StateType> &activeStates,
                                                   const std::vector<StateType> &targetStates) {
        std::vector<StateType> exitSet;
        std::unordered_set<StateType> targetSet(targetStates.begin(), targetStates.end());

        // Collect all states to exit (those not ancestors of any target)
        for (auto activeState : activeStates) {
            bool shouldExit = true;

            // Check if this state is ancestor of any target (if so, don't exit)
            for (auto targetState : targetStates) {
                if (isAncestor<StateType, PolicyType>(activeState, targetState)) {
                    shouldExit = false;
                    break;
                }
            }

            if (shouldExit) {
                // Add state and all its ancestors up to LCA
                auto current = activeState;
                while (true) {
                    if (std::find(exitSet.begin(), exitSet.end(), current) == exitSet.end()) {
                        exitSet.push_back(current);
                    }

                    auto parent = PolicyType::getParent(current);
                    if (!parent.has_value()) {
                        break;
                    }

                    // Stop if we reach a target state's ancestor
                    bool isTargetAncestor = false;
                    for (auto targetState : targetStates) {
                        if (isAncestor<StateType, PolicyType>(parent.value(), targetState)) {
                            isTargetAncestor = true;
                            break;
                        }
                    }
                    if (isTargetAncestor) {
                        break;
                    }

                    current = parent.value();
                }
            }
        }

        // Sort by exit order: Children before parents, reverse document order for ties
        std::sort(exitSet.begin(), exitSet.end(), [](StateType a, StateType b) {
            // If a is ancestor of b, b exits first (child before parent)
            if (isAncestor<StateType, PolicyType>(a, b)) {
                return false;
            }
            if (isAncestor<StateType, PolicyType>(b, a)) {
                return true;
            }

            // Neither is ancestor of the other - use reverse document order
            return PolicyType::getDocumentOrder(a) > PolicyType::getDocumentOrder(b);
        });

        return exitSet;
    }

    /**
     * @brief Compute entry order for states
     *
     * W3C SCXML 3.13: States are entered in entry order:
     * 1. Parents before children
     * 2. Document order for tie-breaking (states appearing earlier enter first)
     *
     * For parallel states: The parallel state is entered, then all child regions simultaneously.
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with state hierarchy and document order
     * @param targetStates Target states of transition
     * @param currentStates Currently active states
     * @return Vector of states in entry order
     */
    template <typename StateType, typename PolicyType>
    static std::vector<StateType> computeEntryOrder(const std::vector<StateType> &targetStates,
                                                    const std::vector<StateType> &currentStates) {
        std::vector<StateType> entrySet;
        std::unordered_set<StateType> currentSet(currentStates.begin(), currentStates.end());

        // Collect all states to enter (targets and their ancestors)
        for (auto targetState : targetStates) {
            auto current = targetState;

            // Add target and all ancestors that are not already active
            std::vector<StateType> pathToRoot;
            while (true) {
                if (currentSet.find(current) == currentSet.end()) {
                    pathToRoot.push_back(current);
                }

                auto parent = PolicyType::getParent(current);
                if (!parent.has_value()) {
                    break;
                }
                current = parent.value();
            }

            // Add in reverse order (root to leaf)
            for (auto it = pathToRoot.rbegin(); it != pathToRoot.rend(); ++it) {
                if (std::find(entrySet.begin(), entrySet.end(), *it) == entrySet.end()) {
                    entrySet.push_back(*it);
                }
            }
        }

        // Sort by entry order: Parents before children, document order for ties
        std::sort(entrySet.begin(), entrySet.end(), [](StateType a, StateType b) {
            // If a is ancestor of b, a enters first (parent before child)
            if (isAncestor<StateType, PolicyType>(a, b)) {
                return true;
            }
            if (isAncestor<StateType, PolicyType>(b, a)) {
                return false;
            }

            // Neither is ancestor of the other - use document order
            return PolicyType::getDocumentOrder(a) < PolicyType::getDocumentOrder(b);
        });

        return entrySet;
    }

    /**
     * @brief Compute exit order for parallel state children
     *
     * W3C SCXML 3.13 + 3.4: When exiting a parallel state, all child regions exit
     * in reverse document order (children of later regions exit first).
     *
     * This is specifically for test 404 scenario where parallel state has multiple
     * child regions, and each region's onexit actions must fire in correct order.
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with parallel state information
     * @param parallelState The parallel state being exited
     * @param activeRegionStates Currently active states in each region
     * @return Vector of states in exit order (reverse document order)
     */
    template <typename StateType, typename PolicyType>
    static std::vector<StateType> computeParallelExitOrder(StateType parallelState,
                                                           const std::vector<StateType> &activeRegionStates) {
        std::vector<StateType> exitOrder = activeRegionStates;

        // Sort by reverse document order (later states exit first)
        std::sort(exitOrder.begin(), exitOrder.end(), [](StateType a, StateType b) {
            return PolicyType::getDocumentOrder(a) > PolicyType::getDocumentOrder(b);
        });

        return exitOrder;
    }

private:
    /**
     * @brief Check if state1 is an ancestor of state2
     *
     * W3C SCXML 3.3: Traverse parent chain of state2 to check if state1 appears
     */
    template <typename StateType, typename PolicyType> static bool isAncestor(StateType state1, StateType state2) {
        auto current = state2;
        while (true) {
            auto parent = PolicyType::getParent(current);
            if (!parent.has_value()) {
                return false;  // Reached root without finding state1
            }
            if (parent.value() == state1) {
                return true;  // Found state1 in ancestor chain
            }
            current = parent.value();
        }
    }
};

}  // namespace RSM
