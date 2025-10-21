#pragma once

#include "types.h"
#include <optional>
#include <unordered_set>
#include <vector>

namespace RSM {
namespace HistoryHelper {

/**
 * @brief W3C SCXML 3.11: Filter states for shallow history recording
 *
 * Shallow history records only immediate child states of the parent compound state.
 *
 * @tparam StateType Enum or string type for state representation
 * @tparam GetParentFunc Function type: std::optional<StateType>(StateType) -> parent state
 * @param activeStates All currently active states
 * @param parentState Parent compound state whose history is being recorded
 * @param getParent Function to get parent of a state
 * @return Filtered states (immediate children only)
 */
template <typename StateType, typename GetParentFunc>
std::vector<StateType> filterShallowHistory(const std::vector<StateType> &activeStates, StateType parentState,
                                            GetParentFunc getParent) {
    std::vector<StateType> filteredStates;

    // W3C SCXML 3.11: Shallow history records only direct children
    for (const auto &state : activeStates) {
        auto parent = getParent(state);
        if (parent.has_value() && parent.value() == parentState) {
            filteredStates.push_back(state);
        }
    }

    return filteredStates;
}

/**
 * @brief Check if a state is a descendant of a parent state
 *
 * @tparam StateType Enum or string type for state representation
 * @tparam GetParentFunc Function type: std::optional<StateType>(StateType) -> parent state
 * @param state State to check
 * @param parentState Potential ancestor state
 * @param getParent Function to get parent of a state
 * @return true if state is a descendant of parentState
 */
template <typename StateType, typename GetParentFunc>
bool isDescendant(StateType state, StateType parentState, GetParentFunc getParent) {
    if (state == parentState) {
        return false;  // A state is not a descendant of itself
    }

    // Walk up the parent chain
    auto current = getParent(state);
    while (current.has_value()) {
        if (current.value() == parentState) {
            return true;
        }
        current = getParent(current.value());
    }

    return false;
}

/**
 * @brief W3C SCXML 3.11: Filter states for deep history recording
 *
 * Deep history records all leaf (atomic) descendant states of the parent compound state.
 * A leaf state is one that has no active child states in the current configuration.
 *
 * @tparam StateType Enum or string type for state representation
 * @tparam GetParentFunc Function type: std::optional<StateType>(StateType) -> parent state
 * @param activeStates All currently active states (set for O(1) lookup)
 * @param parentState Parent compound state whose history is being recorded
 * @param getParent Function to get parent of a state
 * @return Filtered states (leaf descendants only)
 */
template <typename StateType, typename GetParentFunc>
std::vector<StateType> filterDeepHistory(const std::vector<StateType> &activeStates, StateType parentState,
                                         GetParentFunc getParent) {
    std::vector<StateType> filteredStates;

    // W3C SCXML 3.11: Deep history records deepest active descendant configuration
    // We keep only leaf states (no active children) that are descendants of parent

    // Convert to set for O(1) lookup when checking if children are active
    std::unordered_set<StateType> activeStateSet(activeStates.begin(), activeStates.end());

    for (const auto &state : activeStates) {
        // Check if this state is a descendant of the parent
        if (!isDescendant(state, parentState, getParent)) {
            continue;
        }

        // Check if this is a leaf state (no active children)
        // Since we don't have access to state structure in AOT, we check if any
        // other active state is a child of this state
        bool isLeaf = true;
        for (const auto &otherState : activeStates) {
            if (otherState == state) {
                continue;
            }
            auto otherParent = getParent(otherState);
            if (otherParent.has_value() && otherParent.value() == state) {
                isLeaf = false;
                break;
            }
        }

        if (isLeaf) {
            filteredStates.push_back(state);
        }
    }

    return filteredStates;
}

/**
 * @brief W3C SCXML 3.11: Record history for a compound state
 *
 * This is the core recording logic shared between Interpreter and AOT engines.
 *
 * @tparam StateType Enum or string type for state representation
 * @tparam GetParentFunc Function type: std::optional<StateType>(StateType) -> parent state
 * @param activeStates All currently active states
 * @param parentState Parent compound state whose history is being recorded
 * @param historyType Shallow or deep history
 * @param getParent Function to get parent of a state
 * @return Filtered states to record in history
 */
template <typename StateType, typename GetParentFunc>
std::vector<StateType> recordHistory(const std::vector<StateType> &activeStates, StateType parentState,
                                     HistoryType historyType, GetParentFunc getParent) {
    if (historyType == HistoryType::SHALLOW) {
        return filterShallowHistory(activeStates, parentState, getParent);
    } else {
        return filterDeepHistory(activeStates, parentState, getParent);
    }
}

}  // namespace HistoryHelper
}  // namespace RSM
