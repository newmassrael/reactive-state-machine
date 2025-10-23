#pragma once

#include "common/Logger.h"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

namespace RSM::Common {

/**
 * @brief Helper for hierarchical state operations (W3C SCXML 3.3)
 *
 * Single Source of Truth for hierarchical state logic shared between:
 * - StaticExecutionEngine (AOT engine)
 * - StateMachine (Interpreter engine)
 *
 * ARCHITECTURE.md Compliance:
 * - Zero Duplication Principle: Shared logic between Interpreter and AOT engines
 * - Single Source of Truth: All hierarchical operations centralized in this Helper
 * - Static-First Principle: All hierarchy operations are Closed World
 *   (state structure known at compile-time from SCXML parse)
 * - All-or-Nothing Strategy: Pure compile-time helper, no dynamic features
 *
 * Ensures zero duplication per ARCHITECTURE.md principles.
 */
template <typename StatePolicy> class HierarchicalStateHelper {
public:
    using State = typename StatePolicy::State;

private:
    // SFINAE helper: addParallelRegions if method exists
    template <typename T = StatePolicy>
    static auto addParallelRegionsImpl(std::vector<State> &chain, State leafState, int)
        -> decltype(T::isParallelState(leafState), void()) {
        LOG_DEBUG("HierarchicalStateHelper::buildEntryChain - Checking if leafState {} is parallel",
                  static_cast<int>(leafState));
        if (T::isParallelState(leafState)) {
            LOG_DEBUG("HierarchicalStateHelper::buildEntryChain - leafState {} IS parallel, adding regions",
                      static_cast<int>(leafState));
            auto regions = T::getParallelRegions(leafState);
            LOG_DEBUG("HierarchicalStateHelper::buildEntryChain - Found {} regions", regions.size());
            for (const auto &region : regions) {
                LOG_DEBUG("HierarchicalStateHelper::buildEntryChain - Adding region {}", static_cast<int>(region));
                chain.push_back(region);

                if (T::isCompoundState(region)) {
                    State regionInitialChild = T::getInitialChild(region);
                    LOG_DEBUG("HierarchicalStateHelper::buildEntryChain - Region {} initial child: {}",
                              static_cast<int>(region), static_cast<int>(regionInitialChild));
                    if (regionInitialChild != region) {
                        LOG_DEBUG("HierarchicalStateHelper::buildEntryChain - Adding initial child {}",
                                  static_cast<int>(regionInitialChild));
                        chain.push_back(regionInitialChild);
                    }
                }
            }
        }
    }

    template <typename T = StatePolicy> static void addParallelRegionsImpl(std::vector<State> &, State, ...) {
        // No-op if isParallelState doesn't exist
    }

    static void addParallelRegions(std::vector<State> &chain, State leafState) {
        addParallelRegionsImpl(chain, leafState, 0);
    }

public:
    // Static assertions to validate StatePolicy interface at compile-time
    static_assert(std::is_same_v<decltype(StatePolicy::getParent(std::declval<State>())), std::optional<State>>,
                  "StatePolicy::getParent() must return std::optional<State>");
    static_assert(std::is_same_v<decltype(StatePolicy::isCompoundState(std::declval<State>())), bool>,
                  "StatePolicy::isCompoundState() must return bool");
    static_assert(std::is_same_v<decltype(StatePolicy::getInitialChild(std::declval<State>())), State>,
                  "StatePolicy::getInitialChild() must return State");

    /**
     * @brief Build entry chain from leaf state to root
     *
     * @details
     * W3C SCXML 3.3 requires hierarchical state entry from ancestor to descendant.
     * This method builds the complete entry chain for a target state.
     *
     * The implementation includes safety checks for cyclic parent relationships
     * and performance optimizations for typical hierarchy depths.
     *
     * @param leafState Target leaf state to enter
     * @return Vector of states in entry order (root → ... → leaf)
     *
     * @par Thread Safety
     * This method is thread-safe and reentrant.
     *
     * @par Performance
     * - Time Complexity: O(depth) where depth is hierarchy depth
     * - Space Complexity: O(depth)
     * - Typical depth: 1-5 levels
     * - Maximum safe depth: 16 levels
     * - Pre-allocated capacity: 8 states (avoids reallocation in 99% of cases)
     *
     * @par Example
     * @code
     * // Given hierarchy: S0 (root) → S01 (child) → S011 (grandchild)
     * auto chain = HierarchicalStateHelper<Policy>::buildEntryChain(State::S011);
     * // Returns: [State::S0, State::S01, State::S011]
     *
     * // Execute entry actions in correct order
     * for (const auto& state : chain) {
     *     executeOnEntry(state);  // S0 first, then S01, finally S011
     * }
     * @endcode
     *
     * @throws std::runtime_error If cyclic parent relationship detected (depth > MAX_DEPTH)
     * @throws std::runtime_error If MAX_DEPTH exceeded (prevents infinite loops)
     *
     * @par Error Handling
     * Malformed SCXML with cyclic parent relationships will be detected and reported.
     * This protects against code generator bugs or corrupted state machine definitions.
     */
    static std::vector<State> buildEntryChain(State leafState) {
        // Maximum allowed hierarchy depth (W3C SCXML practical limit)
        // Typical state machines: 1-5 levels
        // Complex state machines: up to 10 levels
        // Safety buffer: 16 levels (prevents infinite loops from cyclic parents)
        constexpr size_t MAX_DEPTH = 16;

        // Pre-allocate for typical case (avoids reallocation)
        // 99% of state machines have depth <= 8
        std::vector<State> chain;
        chain.reserve(8);

        State current = leafState;
        size_t depth = 0;

        // Build chain from leaf to root with cycle detection
        while (depth < MAX_DEPTH) {
            chain.push_back(current);

            auto parent = StatePolicy::getParent(current);
            if (!parent.has_value()) {
                break;  // Reached root state
            }

            current = parent.value();
            ++depth;
        }

        // Safety check: detect cyclic parent relationships
        if (depth >= MAX_DEPTH) {
            LOG_ERROR("HierarchicalStateHelper::buildEntryChain() - Maximum depth ({}) exceeded for state. "
                      "Cyclic parent relationship detected in state machine definition. "
                      "This indicates a bug in the code generator or corrupted SCXML.",
                      MAX_DEPTH);
            throw std::runtime_error("Cyclic parent relationship detected in state hierarchy");
        }

        // Reverse to get root-to-leaf order (entry order per W3C SCXML 3.3)
        std::reverse(chain.begin(), chain.end());

        // W3C SCXML 3.3: If leaf is compound state, add initial child hierarchy
        // This ensures S01 (compound) automatically enters S011 (initial child)
        State leafToCheck = leafState;
        depth = 0;
        while (depth < MAX_DEPTH && StatePolicy::isCompoundState(leafToCheck)) {
            State initialChild = StatePolicy::getInitialChild(leafToCheck);
            if (initialChild == leafToCheck) {
                break;  // No initial child or self-reference
            }
            chain.push_back(initialChild);
            leafToCheck = initialChild;
            ++depth;
        }

        // W3C SCXML 3.4: If leaf is parallel state, add ALL child regions and their initial states
        // This ensures parallel states enter all regions simultaneously (Interpreter behavior)
        LOG_DEBUG("HierarchicalStateHelper::buildEntryChain - About to call addParallelRegions for leafState {}",
                  static_cast<int>(leafState));
        addParallelRegions(chain, leafState);
        LOG_DEBUG("HierarchicalStateHelper::buildEntryChain - Returned from addParallelRegions, chain size now {}",
                  chain.size());

        return chain;
    }

    /**
     * @brief Check if state has a parent (is a child of composite state)
     *
     * @details
     * Root states return false, child states return true.
     * Useful for determining if a state is part of a hierarchical structure.
     *
     * @param state State to check
     * @return true if state has parent, false if root state
     *
     * @par Thread Safety
     * Thread-safe and reentrant.
     *
     * @par Performance
     * O(1) - Delegates to StatePolicy::getParent()
     *
     * @par Example
     * @code
     * if (HierarchicalStateHelper<Policy>::hasParent(State::S01)) {
     *     // S01 is a child state, need to handle parent transitions
     * }
     * @endcode
     */
    static bool hasParent(State state) {
        return StatePolicy::getParent(state).has_value();
    }

    /**
     * @brief Get parent state of a child state
     *
     * @details
     * Returns the immediate parent of a state in the hierarchy.
     * Root states return std::nullopt.
     *
     * @param state Child state
     * @return Parent state if exists, std::nullopt for root states
     *
     * @par Thread Safety
     * Thread-safe and reentrant.
     *
     * @par Performance
     * O(1) - Direct delegation to StatePolicy::getParent()
     *
     * @par Example
     * @code
     * auto parent = HierarchicalStateHelper<Policy>::getParent(State::S01);
     * if (parent.has_value()) {
     *     LOG_INFO("Parent of S01 is {}", parent.value());
     * }
     * @endcode
     */
    /**
     * @brief Build exit chain from current state up to (excluding) ancestor
     *
     * @details
     * W3C SCXML 3.12 requires hierarchical state exit from descendant to ancestor.
     * This method builds the complete exit chain for a state transition.
     *
     * Exit order is child → parent, matching Interpreter's buildExitSetForDescendants().
     *
     * @param fromState Current state to exit from
     * @param stopBeforeState Ancestor state to stop before (exclusive, not included in exit chain)
     * @return Vector of states in exit order (leaf → ... → parent, excluding stopBeforeState)
     *
     * @par Thread Safety
     * This method is thread-safe and reentrant.
     *
     * @par Performance
     * - Time Complexity: O(depth)
     * - Space Complexity: O(depth)
     *
     * @par Example
     * @code
     * // Given hierarchy: S0 → S01 → S011
     * // Transition from S011 to S02 (sibling of S01)
     * // LCA is S0, so exit S011 and S01 (but not S0)
     * auto chain = HierarchicalStateHelper<Policy>::buildExitChain(State::S011, State::S0);
     * // Returns: [State::S011, State::S01]  (child → parent order)
     *
     * // Execute exit actions in correct order
     * for (const auto& state : chain) {
     *     executeOnExit(state);  // S011 first, then S01
     * }
     * @endcode
     *
     * @par W3C SCXML 3.12 Compliance
     * Matches Interpreter's buildExitSetForDescendants() behavior:
     * - Builds exit set from active state up to (but not including) LCA
     * - Maintains child → parent exit order
     * - Used for external transitions with proper LCA calculation
     */
    static std::vector<State> buildExitChain(State fromState, State stopBeforeState) {
        std::vector<State> chain;
        chain.reserve(8);

        State current = fromState;

        // Build exit chain from current state up to (but not including) stopBeforeState
        while (current != stopBeforeState) {
            chain.push_back(current);

            auto parent = StatePolicy::getParent(current);
            if (!parent.has_value()) {
                break;  // Reached root
            }

            current = parent.value();
        }

        // Return in child → parent order (already correct, no reverse needed)
        return chain;
    }

    /**
     * @brief Build entry chain from parent down to target state
     *
     * @details
     * W3C SCXML 3.12: After finding LCA, enter states from LCA down to target.
     * This method builds the entry chain excluding the parent (LCA) itself.
     *
     * Entry order is parent → child, matching Interpreter's hierarchical entry.
     *
     * @param targetState Target state to enter
     * @param parentState Parent state to start from (exclusive, not included in entry chain)
     * @return Vector of states in entry order (parent → ... → target, excluding parentState)
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
     * // Given hierarchy: S0 → S01 → S011
     * // Transition from S02 to S011 (sibling transition)
     * // LCA is S0, so enter S01 and S011 (but not S0)
     * auto chain = HierarchicalStateHelper<Policy>::buildEntryChainFromParent(State::S011, State::S0);
     * // Returns: [State::S01, State::S011]  (parent → child order)
     *
     * // Execute entry actions in correct order
     * for (const auto& state : chain) {
     *     executeOnEntry(state);  // S01 first, then S011
     * }
     * @endcode
     *
     * @par W3C SCXML 3.12 Compliance
     * Matches Interpreter's hierarchical entry after LCA calculation.
     */
    static std::vector<State> buildEntryChainFromParent(State targetState, State parentState) {
        std::vector<State> chain;
        chain.reserve(8);

        State current = targetState;

        // Build chain from target up to (but not including) parent
        while (current != parentState) {
            chain.push_back(current);

            auto parent = StatePolicy::getParent(current);
            if (!parent.has_value()) {
                break;  // Reached root
            }

            current = parent.value();
        }

        // Reverse to get parent → child order (entry order per W3C SCXML 3.3)
        std::reverse(chain.begin(), chain.end());
        return chain;
    }

    /**
     * @brief Find Least Common Ancestor (LCA) of two states
     *
     * @details
     * W3C SCXML 3.12: External transitions exit states up to the LCA,
     * then enter states from LCA down to target.
     *
     * The LCA is the deepest common ancestor in the state hierarchy.
     *
     * @param state1 First state
     * @param state2 Second state
     * @return LCA state, or std::nullopt if no common ancestor (shouldn't happen in valid SCXML)
     *
     * @par Thread Safety
     * Thread-safe and reentrant.
     *
     * @par Performance
     * - Time Complexity: O(depth1 + depth2)
     * - Space Complexity: O(depth1)
     *
     * @par Example
     * @code
     * // Given hierarchy: S0 → { S01 → S011, S02 → S021 }
     * auto lca = HierarchicalStateHelper<Policy>::findLCA(State::S011, State::S021);
     * // Returns: State::S0 (common parent of S01 and S02)
     *
     * // Same state
     * lca = HierarchicalStateHelper<Policy>::findLCA(State::S011, State::S011);
     * // Returns: State::S011 (state is its own LCA)
     * @endcode
     *
     * @par W3C SCXML 3.12 Compliance
     * Matches Interpreter's findLCA() behavior for external transitions.
     */
    static std::optional<State> findLCA(State state1, State state2) {
        // Same state is its own LCA
        if (state1 == state2) {
            return state1;
        }

        // Build ancestor chain for state1 (W3C SCXML 3.13: exclude state1 itself, start from parent)
        // This matches the original Interpreter behavior before commit d0c3ab2
        std::vector<State> ancestors1;
        ancestors1.reserve(8);

        // Start from state1's parent, not state1 itself (W3C SCXML test 504)
        auto parent1 = StatePolicy::getParent(state1);
        if (parent1.has_value()) {
            State current = parent1.value();
            while (true) {
                ancestors1.push_back(current);
                auto parent = StatePolicy::getParent(current);
                if (!parent.has_value()) {
                    break;
                }
                current = parent.value();
            }
        }

        // Walk up from state2 and check if we find any ancestor of state1
        State current = state2;
        while (true) {
            // Check if current is in state1's ancestor chain
            for (const auto &ancestor : ancestors1) {
                if (current == ancestor) {
                    return current;  // Found LCA
                }
            }

            auto parent = StatePolicy::getParent(current);
            if (!parent.has_value()) {
                break;  // Reached root without finding LCA
            }
            current = parent.value();
        }

        // No common ancestor (shouldn't happen in valid SCXML)
        return std::nullopt;
    }

    static std::optional<State> getParent(State state) {
        return StatePolicy::getParent(state);
    }
};

/**
 * @brief String-based hierarchical state helpers for Interpreter engine
 *
 * @details
 * Non-template utility class for string-based state ID operations.
 * Used by Interpreter engine which uses string state IDs instead of enums.
 *
 * ARCHITECTURE.md Compliance:
 * - Zero Duplication: Same algorithms as template version
 * - Single Source of Truth: Interpreter delegates to shared logic
 */
struct HierarchicalStateHelperString {
    /**
     * @brief Find Least Common Ancestor (LCA) for string-based state IDs
     *
     * @tparam GetParentFunc Lambda/function: std::optional<std::string>(const std::string&)
     * @param state1 First state ID
     * @param state2 Second state ID
     * @param getParent Function to get parent state ID from child state ID
     * @return LCA state ID, or empty string if no common ancestor
     */
    template <typename GetParentFunc>
    [[nodiscard]] static std::string findLCA(const std::string &state1, const std::string &state2,
                                             GetParentFunc getParent) {
        // W3C SCXML 3.12: Same state is its own LCA
        if (state1 == state2) {
            return state1;
        }

        // Build ancestor chain for state1 (W3C SCXML 3.13: exclude state1 itself, start from parent)
        // This matches the original Interpreter behavior before commit d0c3ab2
        std::vector<std::string> ancestors1;
        ancestors1.reserve(8);

        // Start from state1's parent, not state1 itself (W3C SCXML test 504)
        auto parent1 = getParent(state1);
        if (parent1.has_value()) {
            std::string current = parent1.value();
            while (true) {
                ancestors1.push_back(current);
                auto parent = getParent(current);
                if (!parent.has_value()) {
                    break;  // Reached root
                }
                current = parent.value();
            }
        }

        // Walk up from state2 and check if we find any ancestor of state1
        std::string current = state2;
        while (true) {
            // Check if current is in state1's ancestor chain
            for (const auto &ancestor : ancestors1) {
                if (current == ancestor) {
                    return current;  // Found LCA
                }
            }

            auto parent = getParent(current);
            if (!parent.has_value()) {
                break;  // Reached root without finding LCA
            }
            current = parent.value();
        }

        // No common ancestor (shouldn't happen in valid SCXML)
        return "";
    }

    /**
     * @brief Build exit chain from fromState up to (but not including) stopBeforeState
     *
     * @tparam GetParentFunc Lambda/function: std::optional<std::string>(const std::string&)
     * @param fromState State to start exiting from
     * @param stopBeforeState State to stop before (excluded from chain)
     * @param getParent Function to get parent state ID
     * @return Exit chain in child → parent order (execution order per W3C SCXML 3.8)
     */
    template <typename GetParentFunc>
    [[nodiscard]] static std::vector<std::string>
    buildExitChain(const std::string &fromState, const std::string &stopBeforeState, GetParentFunc getParent) {
        std::vector<std::string> chain;
        chain.reserve(8);

        std::string current = fromState;
        while (current != stopBeforeState) {
            chain.push_back(current);

            auto parent = getParent(current);
            if (!parent.has_value()) {
                break;  // Reached root
            }
            current = parent.value();
        }

        return chain;  // Already in exit order (child → parent)
    }

    /**
     * @brief Build entry chain from parentState down to targetState
     *
     * @tparam GetParentFunc Lambda/function: std::optional<std::string>(const std::string&)
     * @param targetState Final target state (leaf)
     * @param parentState Starting parent state (excluded from chain)
     * @param getParent Function to get parent state ID
     * @return Entry chain in parent → child order (execution order per W3C SCXML 3.7)
     */
    template <typename GetParentFunc>
    [[nodiscard]] static std::vector<std::string>
    buildEntryChain(const std::string &targetState, const std::string &parentState, GetParentFunc getParent) {
        std::vector<std::string> chain;
        chain.reserve(8);

        // Build chain from targetState up to parentState (reversed order)
        std::string current = targetState;
        while (current != parentState) {
            chain.push_back(current);

            auto parent = getParent(current);
            if (!parent.has_value()) {
                break;  // Reached root
            }
            current = parent.value();
        }

        // Reverse to get parent → child order (entry order per W3C SCXML 3.7)
        std::reverse(chain.begin(), chain.end());
        return chain;
    }
};

}  // namespace RSM::Common
