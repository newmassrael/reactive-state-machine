#pragma once

#include "common/HierarchicalStateHelper.h"
#include <optional>
#include <unordered_set>
#include <vector>

namespace RSM {

/**
 * @brief Helper functions for parallel state transition conflict detection
 *
 * W3C SCXML Appendix C.1: Algorithm for SCXML Interpretation
 * - Optimal enabled transition set: Select non-conflicting transitions
 * - Conflict detection: Two transitions conflict if they exit the same state
 *
 * Shared between Interpreter and AOT engines following Zero Duplication Principle.
 */
class ParallelTransitionHelper {
public:
    /**
     * @brief Transition descriptor for conflict detection
     */
    template <typename StateType> struct Transition {
        StateType source;                       // Source state
        std::vector<StateType> targets;         // Target states
        std::unordered_set<StateType> exitSet;  // States exited by this transition

        // W3C SCXML 3.13: Additional metadata for AOT engine compatibility
        int transitionIndex = 0;  // Index for executeTransitionActions
        bool hasActions = false;  // Whether transition has executable content

        Transition() = default;

        Transition(StateType src, std::vector<StateType> tgts) : source(src), targets(std::move(tgts)) {}

        // Constructor with full metadata (for AOT engine)
        Transition(StateType src, std::vector<StateType> tgts, int idx, bool actions)
            : source(src), targets(std::move(tgts)), transitionIndex(idx), hasActions(actions) {}
    };

    /**
     * @brief Compute exit set for a transition
     *
     * W3C SCXML 3.13: Exit set = all states exited when taking this transition
     * = source state + ancestors up to (but not including) LCA with targets
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with state hierarchy
     * @param transition Transition to compute exit set for
     * @return Set of states that will be exited
     */
    template <typename StateType, typename PolicyType>
    static std::unordered_set<StateType> computeExitSet(const Transition<StateType> &transition) {
        std::unordered_set<StateType> exitSet;

        // Find LCA of source and all targets
        std::optional<StateType> lca = std::nullopt;
        for (const auto &target : transition.targets) {
            auto currentLca = RSM::Common::HierarchicalStateHelper<PolicyType>::findLCA(transition.source, target);

            if (!lca.has_value()) {
                lca = currentLca;
            } else if (currentLca.has_value()) {
                // If we have multiple LCAs, find their common ancestor
                lca = RSM::Common::HierarchicalStateHelper<PolicyType>::findLCA(lca.value(), currentLca.value());
            }
        }

        // Collect all states from source up to (but not including) LCA
        auto current = transition.source;
        while (true) {
            exitSet.insert(current);

            auto parent = PolicyType::getParent(current);
            if (!parent.has_value()) {
                break;
            }

            // Stop before LCA
            if (lca.has_value() && parent.value() == lca.value()) {
                break;
            }

            current = parent.value();
        }

        return exitSet;
    }

    /**
     * @brief Check if two transitions conflict
     *
     * W3C SCXML Algorithm C.1: Two transitions conflict if their exit sets intersect
     * (they would exit the same state, which is invalid).
     *
     * @tparam StateType State enum or identifier type
     * @param t1 First transition
     * @param t2 Second transition
     * @return true if transitions conflict
     */
    template <typename StateType>
    static bool hasConflict(const Transition<StateType> &t1, const Transition<StateType> &t2) {
        // Check if exit sets intersect
        for (const auto &state : t1.exitSet) {
            if (t2.exitSet.find(state) != t2.exitSet.end()) {
                return true;  // Conflict: both exit the same state
            }
        }
        return false;
    }

    /**
     * @brief Select optimal enabled transition set (non-conflicting)
     *
     * W3C SCXML Algorithm C.1: From all enabled transitions, select maximal
     * non-conflicting subset. Preemption rule: Transitions in child states
     * have priority over parent states.
     *
     * Algorithm:
     * 1. Sort transitions by state hierarchy depth (deeper first)
     * 2. Greedily select transitions that don't conflict with already selected
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with state hierarchy
     * @param enabledTransitions All enabled transitions for current event
     * @return Non-conflicting subset of transitions to execute
     */
    template <typename StateType, typename PolicyType>
    static std::vector<Transition<StateType>>
    selectOptimalTransitions(std::vector<Transition<StateType>> &enabledTransitions) {
        // Compute exit sets for all transitions
        for (auto &transition : enabledTransitions) {
            transition.exitSet = computeExitSet<StateType, PolicyType>(transition);
        }

        // Sort by state hierarchy depth (deeper states first - preemption)
        std::sort(enabledTransitions.begin(), enabledTransitions.end(),
                  [](const Transition<StateType> &a, const Transition<StateType> &b) {
                      return getDepth<StateType, PolicyType>(a.source) > getDepth<StateType, PolicyType>(b.source);
                  });

        // Greedy selection: Pick transitions that don't conflict with already selected
        std::vector<Transition<StateType>> selectedTransitions;

        for (const auto &transition : enabledTransitions) {
            bool conflicts = false;

            // Check if this transition conflicts with any already selected
            for (const auto &selectedTransition : selectedTransitions) {
                if (hasConflict<StateType>(transition, selectedTransition)) {
                    conflicts = true;
                    break;
                }
            }

            if (!conflicts) {
                selectedTransitions.push_back(transition);
            }
        }

        return selectedTransitions;
    }

    /**
     * @brief Get hierarchy depth of a state
     *
     * Depth = number of ancestors (0 for root states)
     * Used for preemption: deeper states have priority
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with state hierarchy
     * @param state State to get depth for
     * @return Depth (0 = root)
     */
    template <typename StateType, typename PolicyType> static int getDepth(StateType state) {
        int depth = 0;
        auto current = state;

        while (true) {
            auto parent = PolicyType::getParent(current);
            if (!parent.has_value()) {
                break;
            }
            depth++;
            current = parent.value();
        }

        return depth;
    }

    /**
     * @brief Check if a transition is enabled for an event
     *
     * A transition is enabled if:
     * 1. Source state is active
     * 2. Event matches transition's event descriptor
     * 3. Condition evaluates to true (if present)
     *
     * @tparam StateType State enum or identifier type
     * @tparam EventType Event enum or identifier type
     * @param sourceState Source state of transition
     * @param transitionEvent Event descriptor of transition
     * @param currentEvent Current event being processed
     * @param isActive Predicate to check if source state is active
     * @return true if transition is enabled
     */
    /**
     * @brief Compute and sort states to exit for microstep execution
     *
     * ARCHITECTURE.MD: Zero Duplication Principle - Shared exit computation logic
     * W3C SCXML Appendix D.2 Step 1: Collect unique source states from transitions
     * W3C SCXML 3.13: Sort by reverse document order (deepest/rightmost first)
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with getDocumentOrder()
     * @param transitions Transitions to execute
     * @param activeStates Current active states
     * @return States to exit in reverse document order
     */
    template <typename StateType, typename PolicyType>
    static std::vector<StateType> computeStatesToExit(const std::vector<Transition<StateType>> &transitions,
                                                      const std::vector<StateType> &activeStates) {
        std::vector<StateType> statesToExit;

        // Collect unique source states that are active
        for (const auto &trans : transitions) {
            bool found = false;
            for (StateType s : statesToExit) {
                if (s == trans.source) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (StateType active : activeStates) {
                    if (active == trans.source) {
                        statesToExit.push_back(trans.source);
                        break;
                    }
                }
            }
        }

        // W3C SCXML 3.13: Sort by REVERSE document order (exit deepest first)
        std::sort(statesToExit.begin(), statesToExit.end(), [](StateType a, StateType b) {
            return PolicyType::getDocumentOrder(a) > PolicyType::getDocumentOrder(b);
        });

        return statesToExit;
    }

    /**
     * @brief Sort transitions by source state document order
     *
     * ARCHITECTURE.MD: Zero Duplication Principle - Shared sorting logic
     * W3C SCXML Appendix D.2 Step 3: Execute transition content in document order
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with getDocumentOrder()
     * @param transitions Transitions to sort
     * @return Sorted transitions (by source state document order)
     */
    template <typename StateType, typename PolicyType>
    static std::vector<Transition<StateType>> sortTransitionsBySource(std::vector<Transition<StateType>> transitions) {
        std::sort(transitions.begin(), transitions.end(),
                  [](const Transition<StateType> &a, const Transition<StateType> &b) {
                      return PolicyType::getDocumentOrder(a.source) < PolicyType::getDocumentOrder(b.source);
                  });

        return transitions;
    }

    /**
     * @brief Sort transitions by target state document order
     *
     * ARCHITECTURE.MD: Zero Duplication Principle - Shared sorting logic
     * W3C SCXML Appendix D.2 Step 4-5: Enter target states in document order
     *
     * @tparam StateType State enum or identifier type
     * @tparam PolicyType Policy class with getDocumentOrder()
     * @param transitions Transitions to sort
     * @return Sorted transitions (by target state document order)
     */
    template <typename StateType, typename PolicyType>
    static std::vector<Transition<StateType>> sortTransitionsByTarget(std::vector<Transition<StateType>> transitions) {
        std::sort(transitions.begin(), transitions.end(),
                  [](const Transition<StateType> &a, const Transition<StateType> &b) {
                      StateType targetA = a.targets.empty() ? a.source : a.targets[0];
                      StateType targetB = b.targets.empty() ? b.source : b.targets[0];
                      return PolicyType::getDocumentOrder(targetA) < PolicyType::getDocumentOrder(targetB);
                  });

        return transitions;
    }

    /**
     * @brief Sort states for exit by depth and document order
     *
     * ARCHITECTURE.MD: Zero Duplication Principle - Shared exit ordering logic
     * W3C SCXML 3.13: States exit in order (deepest first, then reverse document order)
     * Shared between Interpreter and AOT engines.
     *
     * @tparam StateType State identifier type (string or enum)
     * @tparam GetDepthFunc Callable that returns depth for a state
     * @tparam GetDocOrderFunc Callable that returns document order for a state
     * @param states States to sort
     * @param getDepth Function to get state depth (0 = root)
     * @param getDocOrder Function to get document order
     * @return Sorted states (deepest first, reverse document order for same depth)
     */
    template <typename StateType, typename GetDepthFunc, typename GetDocOrderFunc>
    static std::vector<StateType> sortStatesForExit(std::vector<StateType> states, GetDepthFunc getDepth,
                                                    GetDocOrderFunc getDocOrder) {
        std::sort(states.begin(), states.end(), [&](const StateType &a, const StateType &b) {
            // W3C SCXML 3.13: Primary sort by depth (deepest first)
            int depthA = getDepth(a);
            int depthB = getDepth(b);

            if (depthA != depthB) {
                return depthA > depthB;  // Deeper states exit first
            }

            // W3C SCXML 3.13: Secondary sort by reverse document order
            return getDocOrder(a) > getDocOrder(b);  // Later states exit first
        });

        return states;
    }

    /**
     * @brief Check if a transition is enabled for an event
     *
     * A transition is enabled if:
     * 1. Source state is active
     * 2. Event matches transition's event descriptor
     * 3. Condition evaluates to true (if present)
     *
     * @tparam StateType State enum or identifier type
     * @tparam EventType Event enum or identifier type
     * @param sourceState Source state of transition
     * @param transitionEvent Event descriptor of transition
     * @param currentEvent Current event being processed
     * @param isActive Predicate to check if source state is active
     * @return true if transition is enabled
     */
    template <typename StateType, typename EventType>
    static bool isTransitionEnabled(StateType sourceState, EventType transitionEvent, EventType currentEvent,
                                    std::function<bool(StateType)> isActive) {
        // Check if source state is active
        if (!isActive(sourceState)) {
            return false;
        }

        // Check if event matches (event matching logic is in EventMatchingHelper)
        // For now, simple equality check
        return transitionEvent == currentEvent;
    }
};

}  // namespace RSM
