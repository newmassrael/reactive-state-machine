#pragma once

#include <algorithm>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vector>

namespace RSM::Core {

/**
 * @brief Helper for hierarchical state operations (W3C SCXML 3.3)
 *
 * Single Source of Truth for hierarchical state logic shared between:
 * - StaticExecutionEngine (JIT engine)
 * - StateMachine (Interpreter engine)
 *
 * Ensures zero duplication per ARCHITECTURE.md principles.
 */
template <typename StatePolicy> class HierarchicalStateHelper {
public:
    using State = typename StatePolicy::State;

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
    static std::optional<State> getParent(State state) {
        return StatePolicy::getParent(state);
    }
};

}  // namespace RSM::Core
