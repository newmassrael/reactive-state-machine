#pragma once

#include "common/HierarchicalStateHelper.h"
#include "common/Logger.h"
#include "common/ParallelTransitionHelper.h"
#include <algorithm>
#include <optional>
#include <vector>

namespace RSM::Common {

/**
 * @brief W3C SCXML Appendix D.2 Conflict Resolution Helper
 *
 * @details
 * Single Source of Truth for W3C SCXML Appendix D.2 optimal transition set selection.
 * Shared between:
 * - StaticExecutionEngine (AOT engine)
 * - StateMachine (Interpreter engine)
 *
 * ARCHITECTURE.md Compliance:
 * - Zero Duplication Principle: Shared logic between Interpreter and AOT engines
 * - Single Source of Truth: All conflict resolution logic centralized in this Helper
 * - W3C SCXML Perfect Compliance: Full implementation of Appendix D.2 algorithm
 * - All-or-Nothing Strategy: Pure compile-time helper for static generation
 *
 * W3C SCXML Appendix D.2 Algorithm:
 * 1. For each enabled transition t1 (in document order)
 * 2. Check against already-filtered transitions t2
 * 3. If exit sets intersect (conflict):
 *    - If t1.source is descendant of t2.source → t1 preempts t2 (remove t2)
 *    - Otherwise → t2 preempts t1 (skip t1, document order)
 * 4. Add t1 to filtered set if not preempted
 *
 * Ensures zero duplication per ARCHITECTURE.md principles.
 */
template <typename StatePolicy> class ConflictResolutionHelper {
public:
    using State = typename StatePolicy::State;

    /**
     * @brief Transition descriptor for conflict resolution
     *
     * @details
     * Minimal information needed for W3C SCXML Appendix D.2 algorithm:
     * - source: Source state of transition
     * - target: Target state of transition
     * - exitSet: States to be exited (computed from source to LCA)
     * - transitionIndex: Original index in document order
     */
    struct TransitionDescriptor {
        State source;
        State target;
        std::vector<State> exitSet;
        int transitionIndex = 0;
        bool hasActions = false;  // W3C SCXML 3.13: Transition action metadata
        bool isInternal = false;  // W3C SCXML 3.13: Whether transition is type="internal"

        TransitionDescriptor() = default;

        TransitionDescriptor(State src, State tgt, int idx = 0, bool actions = false, bool internal = false)
            : source(src), target(tgt), transitionIndex(idx), hasActions(actions), isInternal(internal) {}
    };

    /**
     * @brief Compute exit set for a single transition
     *
     * @details
     * W3C SCXML Appendix D.2: Exit set = states from source up to (but not including) LCA
     *
     * ARCHITECTURE.md Zero Duplication: Delegates to ParallelTransitionHelper for exit set computation.
     * Single Source of Truth - same algorithm used by AOT engine microstep execution.
     *
     * @param source Source state of transition
     * @param target Target state of transition
     * @return Exit set (states to be exited)
     *
     * @par Thread Safety
     * Thread-safe and reentrant.
     *
     * @par Performance
     * - Time Complexity: O(depth)
     * - Space Complexity: O(depth)
     *
     * @par Example
     * @code
     * // Given hierarchy: S0 → { S01 → S011, S02 }
     * // Transition from S011 to S02
     * auto exitSet = ConflictResolutionHelper<Policy>::computeExitSet(State::S011, State::S02);
     * // Returns: [S011, S01] (exit both S011 and S01 to reach LCA S0)
     * @endcode
     */
    static std::vector<State> computeExitSet(State source, State target) {
        // ARCHITECTURE.MD Zero Duplication: Delegate to ParallelTransitionHelper
        // Construct minimal Transition descriptor for exit set computation
        typename ParallelTransitionHelper::Transition<State> trans;
        trans.source = source;
        trans.targets = {target};

        // W3C SCXML Appendix D.2: Use shared Helper for exit set computation
        auto exitSetUnordered = ParallelTransitionHelper::computeExitSet<State, StatePolicy>(trans);

        // Convert unordered_set to vector for conflict resolution algorithm
        std::vector<State> exitSet(exitSetUnordered.begin(), exitSetUnordered.end());

        LOG_DEBUG("ConflictResolutionHelper::computeExitSet: Transition {} -> {} exits {} states",
                  static_cast<int>(source), static_cast<int>(target), exitSet.size());

        return exitSet;
    }

    /**
     * @brief Check if two exit sets have non-empty intersection
     *
     * @details
     * W3C SCXML Appendix D.2: Two transitions conflict if their exit sets intersect.
     * Exit set intersection means both transitions would exit at least one common state.
     *
     * @param set1 First exit set
     * @param set2 Second exit set
     * @return true if sets have common element, false otherwise
     *
     * @par Thread Safety
     * Thread-safe and reentrant.
     *
     * @par Performance
     * - Time Complexity: O(n * m) where n, m are set sizes
     * - Space Complexity: O(1)
     *
     * @par Example
     * @code
     * std::vector<State> exitSet1 = {State::S011, State::S01};
     * std::vector<State> exitSet2 = {State::S012, State::S01};
     * bool conflict = ConflictResolutionHelper<Policy>::hasIntersection(exitSet1, exitSet2);
     * // Returns: true (both exit S01)
     * @endcode
     */
    static bool hasIntersection(const std::vector<State> &set1, const std::vector<State> &set2) {
        for (State s1 : set1) {
            for (State s2 : set2) {
                if (s1 == s2) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Remove conflicting transitions (W3C SCXML Appendix D.2)
     *
     * @details
     * W3C SCXML Appendix D.2: Algorithm for optimal transition set selection.
     *
     * This is the core conflict resolution algorithm that ensures only non-conflicting
     * transitions are executed in a microstep.
     *
     * Algorithm:
     * 1. For each transition t1 in enabledTransitions (document order)
     * 2. Check against all already-filtered transitions t2
     * 3. If exit sets intersect (conflict):
     *    - If t1's source is descendant of t2's source → t1 preempts t2 (remove t2)
     *    - Otherwise → t2 preempts t1 (skip t1, document order rule)
     * 4. Add t1 to filtered set if not preempted
     *
     * Preemption Rule:
     * - Descendant transitions preempt ancestor transitions (deeper state wins)
     * - Document order breaks ties when neither is descendant (earlier wins)
     *
     * @param enabledTransitions All enabled transitions (in document order)
     * @return Filtered non-conflicting transition set (optimal transition set)
     *
     * @par Thread Safety
     * Thread-safe and reentrant.
     *
     * @par Performance
     * - Time Complexity: O(n²) where n is number of transitions
     * - Space Complexity: O(n)
     * - Typical case: Very few conflicts, close to O(n)
     *
     * @par Example
     * @code
     * // Parallel state with 3 transitions:
     * // - t1: S2p1 → fail (parent, unconditional)
     * // - t2: S2p112 → pass (child, conditional)
     * // - t3: S2p122 → pass (child, conditional)
     * std::vector<TransitionDescriptor> transitions = {t1, t2, t3};
     * auto filtered = ConflictResolutionHelper<Policy>::removeConflictingTransitions(transitions);
     * // Returns: {t2, t3} (children preempt parent)
     * @endcode
     *
     * @par W3C SCXML Appendix D.2 Compliance
     * Full implementation of specification algorithm:
     * - Exit set computation via LCA
     * - Conflict detection via intersection
     * - Preemption via descendant rule + document order
     */
    static std::vector<TransitionDescriptor>
    removeConflictingTransitions(const std::vector<TransitionDescriptor> &enabledTransitions) {
        std::vector<TransitionDescriptor> filteredTransitions;

        LOG_DEBUG("ConflictResolutionHelper::removeConflictingTransitions: Processing {} transitions",
                  enabledTransitions.size());

        // For each transition t1 in enabledTransitions
        for (const auto &t1 : enabledTransitions) {
            bool t1Preempted = false;
            std::vector<size_t> transitionsToRemove;

            // Check against all already-filtered transitions
            for (size_t i = 0; i < filteredTransitions.size(); ++i) {
                const auto &t2 = filteredTransitions[i];

                // W3C SCXML Appendix D.2: Check if exit sets intersect (conflict)
                if (hasIntersection(t1.exitSet, t2.exitSet)) {
                    LOG_DEBUG("ConflictResolutionHelper: Conflict detected: {} -> {} vs {} -> {}",
                              static_cast<int>(t1.source), static_cast<int>(t1.target), static_cast<int>(t2.source),
                              static_cast<int>(t2.target));

                    // W3C SCXML Appendix D.2: If t1's source is descendant of t2's source, t1 preempts t2
                    if (HierarchicalStateHelper<StatePolicy>::isDescendantOf(t1.source, t2.source)) {
                        LOG_DEBUG("ConflictResolutionHelper: {} preempts {} (descendant rule)",
                                  static_cast<int>(t1.source), static_cast<int>(t2.source));
                        transitionsToRemove.push_back(i);
                    } else {
                        // W3C SCXML Appendix D.2: Otherwise t2 preempts t1 (document order)
                        LOG_DEBUG("ConflictResolutionHelper: {} preempts {} (document order)",
                                  static_cast<int>(t2.source), static_cast<int>(t1.source));
                        t1Preempted = true;
                        break;
                    }
                }
            }

            // If t1 not preempted, remove transitions it preempts and add t1
            if (!t1Preempted) {
                // Remove in reverse order to maintain indices
                for (auto it = transitionsToRemove.rbegin(); it != transitionsToRemove.rend(); ++it) {
                    LOG_DEBUG("ConflictResolutionHelper: Removing preempted transition at index {}", *it);
                    filteredTransitions.erase(filteredTransitions.begin() + *it);
                }

                LOG_DEBUG("ConflictResolutionHelper: Adding transition {} -> {}", static_cast<int>(t1.source),
                          static_cast<int>(t1.target));
                filteredTransitions.push_back(t1);
            }
        }

        LOG_DEBUG("ConflictResolutionHelper::removeConflictingTransitions: Filtered to {} transitions",
                  filteredTransitions.size());

        return filteredTransitions;
    }
};

/**
 * @brief String-based conflict resolution helpers for Interpreter engine
 *
 * @details
 * Non-template utility class for string-based state ID operations.
 * Used by Interpreter engine which uses string state IDs instead of enums.
 *
 * ARCHITECTURE.md Compliance:
 * - Zero Duplication: Same algorithms as template version
 * - Single Source of Truth: Interpreter delegates to shared logic
 */
struct ConflictResolutionHelperString {
    /**
     * @brief Transition descriptor for Interpreter engine
     */
    struct TransitionDescriptor {
        std::string source;
        std::string target;
        std::vector<std::string> exitSet;
        int transitionIndex = 0;
        bool hasActions = false;  // W3C SCXML 3.13: Transition action metadata
        bool isInternal = false;  // W3C SCXML 3.13: Whether transition is type="internal"

        TransitionDescriptor() = default;

        TransitionDescriptor(std::string src, std::string tgt, int idx = 0, bool actions = false, bool internal = false)
            : source(std::move(src)), target(std::move(tgt)), transitionIndex(idx), hasActions(actions),
              isInternal(internal) {}
    };

    /**
     * @brief Compute exit set for string-based state IDs
     *
     * @tparam GetParentFunc Lambda/function: std::optional<std::string>(const std::string&)
     * @param source Source state ID
     * @param target Target state ID
     * @param getParent Function to get parent state ID
     * @return Exit set (state IDs to be exited)
     */
    template <typename GetParentFunc>
    [[nodiscard]] static std::vector<std::string> computeExitSet(const std::string &source, const std::string &target,
                                                                 GetParentFunc getParent) {
        std::vector<std::string> exitSet;

        // Build path from source to root
        std::vector<std::string> sourcePath;
        std::string current = source;
        sourcePath.push_back(current);

        while (true) {
            auto parent = getParent(current);
            if (!parent.has_value()) {
                break;
            }
            current = parent.value();
            sourcePath.push_back(current);
        }

        // Build path from target to root
        std::vector<std::string> targetPath;
        current = target;
        targetPath.push_back(current);

        while (true) {
            auto parent = getParent(current);
            if (!parent.has_value()) {
                break;
            }
            current = parent.value();
            targetPath.push_back(current);
        }

        // Find LCA
        std::string lca = sourcePath.back();  // Root by default
        for (const auto &s : sourcePath) {
            for (const auto &t : targetPath) {
                if (s == t) {
                    lca = s;
                    goto found_lca;
                }
            }
        }
    found_lca:

        // Collect states from source up to (but not including) LCA
        for (const auto &s : sourcePath) {
            if (s == lca) {
                break;
            }
            exitSet.push_back(s);
        }

        return exitSet;
    }

    /**
     * @brief Check if two exit sets have non-empty intersection
     */
    static bool hasIntersection(const std::vector<std::string> &set1, const std::vector<std::string> &set2) {
        for (const auto &s1 : set1) {
            for (const auto &s2 : set2) {
                if (s1 == s2) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Check if one state is descendant of another
     *
     * @tparam GetParentFunc Lambda/function: std::optional<std::string>(const std::string&)
     * @param descendant Potential descendant state ID
     * @param ancestor Potential ancestor state ID
     * @param getParent Function to get parent state ID
     * @return true if descendant is child/grandchild/... of ancestor
     */
    template <typename GetParentFunc>
    [[nodiscard]] static bool isDescendantOf(const std::string &descendant, const std::string &ancestor,
                                             GetParentFunc getParent) {
        std::string current = descendant;
        while (true) {
            auto parent = getParent(current);
            if (!parent.has_value()) {
                return false;  // Reached root without finding ancestor
            }
            if (parent.value() == ancestor) {
                return true;  // Found ancestor in parent chain
            }
            current = parent.value();
        }
    }

    /**
     * @brief Remove conflicting transitions for Interpreter engine
     *
     * @tparam GetParentFunc Lambda/function: std::optional<std::string>(const std::string&)
     * @param enabledTransitions All enabled transitions (in document order)
     * @param getParent Function to get parent state ID
     * @return Filtered non-conflicting transition set
     */
    template <typename GetParentFunc>
    [[nodiscard]] static std::vector<TransitionDescriptor>
    removeConflictingTransitions(const std::vector<TransitionDescriptor> &enabledTransitions, GetParentFunc getParent) {
        std::vector<TransitionDescriptor> filteredTransitions;

        LOG_DEBUG("ConflictResolutionHelperString::removeConflictingTransitions: Processing {} transitions",
                  enabledTransitions.size());

        for (const auto &t1 : enabledTransitions) {
            bool t1Preempted = false;
            std::vector<size_t> transitionsToRemove;

            for (size_t i = 0; i < filteredTransitions.size(); ++i) {
                const auto &t2 = filteredTransitions[i];

                // Check if exit sets intersect (conflict)
                if (hasIntersection(t1.exitSet, t2.exitSet)) {
                    LOG_DEBUG("ConflictResolutionHelperString: Conflict detected: {} -> {} vs {} -> {}", t1.source,
                              t1.target, t2.source, t2.target);

                    // If t1's source is descendant of t2's source, t1 preempts t2
                    if (isDescendantOf(t1.source, t2.source, getParent)) {
                        LOG_DEBUG("ConflictResolutionHelperString: {} preempts {} (descendant rule)", t1.source,
                                  t2.source);
                        transitionsToRemove.push_back(i);
                    } else {
                        // Otherwise t2 preempts t1 (document order)
                        LOG_DEBUG("ConflictResolutionHelperString: {} preempts {} (document order)", t2.source,
                                  t1.source);
                        t1Preempted = true;
                        break;
                    }
                }
            }

            if (!t1Preempted) {
                // Remove in reverse order to maintain indices
                for (auto it = transitionsToRemove.rbegin(); it != transitionsToRemove.rend(); ++it) {
                    filteredTransitions.erase(filteredTransitions.begin() + *it);
                }
                filteredTransitions.push_back(t1);
            }
        }

        LOG_DEBUG("ConflictResolutionHelperString::removeConflictingTransitions: Filtered to {} transitions",
                  filteredTransitions.size());

        return filteredTransitions;
    }
};

}  // namespace RSM::Common
