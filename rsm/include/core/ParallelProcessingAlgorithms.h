#pragma once

#include "common/Logger.h"
#include <vector>

/**
 * @file ParallelProcessingAlgorithms.h
 * @brief Single Source of Truth for W3C SCXML Parallel state processing (W3C SCXML 3.4)
 *
 * Design principles (same as EventProcessingAlgorithms and InvokeProcessingAlgorithms):
 * 1. Algorithm sharing only - data structures remain engine-specific
 * 2. Template-based zero overhead (compile-time inlining)
 * 3. Clear interface contracts via template parameters
 *
 * W3C SCXML Sections:
 * - 3.4: Parallel state element and semantics
 * - 3.13: Eventless transitions with parallel states
 * - D.1: Event broadcasting to parallel regions
 */

namespace RSM::Core {

class ParallelProcessingAlgorithms {
public:
    /**
     * @brief W3C SCXML 3.4: Initialize all regions of a parallel state
     *
     * When entering a parallel state, ALL child regions must be entered
     * simultaneously. Each region maintains its own active state.
     *
     * @tparam ParallelStateManager Interface for parallel state management
     *         Required methods:
     *         - void enterRegion(const RegionId& regionId)
     *
     * @tparam RegionList Container of region identifiers
     *
     * @param parallelManager Parallel state manager providing region operations
     * @param regions List of child region identifiers to enter
     *
     * @example Interpreter usage:
     * @code
     * RSM::Core::InterpreterParallelStateManager adapter(stateNode);
     * RSM::Core::ParallelProcessingAlgorithms::enterAllRegions(
     *     adapter,
     *     stateNode->getChildRegions()
     * );
     * @endcode
     *
     * @example JIT usage:
     * @code
     * RSM::Core::JITParallelStateManager<Policy> adapter(policy_);
     * std::vector<State> regions = { State::Region1, State::Region2 };
     * RSM::Core::ParallelProcessingAlgorithms::enterAllRegions(
     *     adapter,
     *     regions
     * );
     * @endcode
     */
    template <typename ParallelStateManager, typename RegionList>
    static void enterAllRegions(ParallelStateManager &parallelManager, const RegionList &regions) {
        LOG_DEBUG("ParallelProcessingAlgorithms: Entering {} parallel regions", regions.size());

        for (const auto &region : regions) {
            parallelManager.enterRegion(region);
        }
    }

    /**
     * @brief W3C SCXML D.1: Broadcast event to all active parallel regions
     *
     * When a parallel state receives an event, it must be broadcast to ALL
     * active child regions. Each region processes the event independently,
     * potentially taking different transitions.
     *
     * @tparam ParallelStateManager Interface for parallel state management
     *         Required methods:
     *         - bool processRegionEvent(const RegionId& regionId, const Event& event)
     *
     * @tparam Event Event type (engine-specific: string for Interpreter, enum for JIT)
     * @tparam RegionList Container of region identifiers
     *
     * @param parallelManager Parallel state manager providing region operations
     * @param event Event to broadcast
     * @param activeRegions List of currently active region identifiers
     *
     * @return true if any region took a transition, false otherwise
     *
     * @example Interpreter usage:
     * @code
     * RSM::Core::InterpreterParallelStateManager adapter(stateNode);
     * bool anyTransition = RSM::Core::ParallelProcessingAlgorithms::broadcastEventToRegions(
     *     adapter,
     *     event,
     *     stateNode->getActiveRegions()
     * );
     * @endcode
     *
     * @example JIT usage:
     * @code
     * RSM::Core::JITParallelStateManager<Policy> adapter(policy_);
     * std::vector<State> activeRegions = policy_.getActiveRegions(currentState);
     * bool anyTransition = RSM::Core::ParallelProcessingAlgorithms::broadcastEventToRegions(
     *     adapter,
     *     event,
     *     activeRegions
     * );
     * @endcode
     */
    template <typename ParallelStateManager, typename Event, typename RegionList>
    static bool broadcastEventToRegions(ParallelStateManager &parallelManager, const Event &event,
                                        const RegionList &activeRegions) {
        LOG_DEBUG("ParallelProcessingAlgorithms: Broadcasting event to {} active regions", activeRegions.size());

        bool anyTransition = false;

        for (const auto &region : activeRegions) {
            if (parallelManager.processRegionEvent(region, event)) {
                anyTransition = true;
            }
        }

        return anyTransition;
    }

    /**
     * @brief W3C SCXML 3.4: Check if all regions are in final states
     *
     * A parallel state is considered to be in a final configuration when
     * ALL of its child regions are in final states. This triggers the
     * done.state.id event for the parallel state.
     *
     * @tparam ParallelStateManager Interface for parallel state management
     *         Required methods:
     *         - bool isRegionInFinalState(const RegionId& regionId)
     *
     * @tparam RegionList Container of region identifiers
     *
     * @param parallelManager Parallel state manager providing region operations
     * @param regions List of child region identifiers to check
     *
     * @return true if ALL regions are in final states, false otherwise
     *
     * @example Interpreter usage:
     * @code
     * RSM::Core::InterpreterParallelStateManager adapter(stateNode);
     * bool allFinal = RSM::Core::ParallelProcessingAlgorithms::areAllRegionsInFinalState(
     *     adapter,
     *     stateNode->getChildRegions()
     * );
     * if (allFinal) {
     *     raiseEvent("done.state." + stateNode->getId());
     * }
     * @endcode
     *
     * @example JIT usage:
     * @code
     * RSM::Core::JITParallelStateManager<Policy> adapter(policy_);
     * std::vector<State> regions = policy_.getParallelRegions(State::ParallelState);
     * bool allFinal = RSM::Core::ParallelProcessingAlgorithms::areAllRegionsInFinalState(
     *     adapter,
     *     regions
     * );
     * if (allFinal) {
     *     engine.raise(Event::Done_state_ParallelState);
     * }
     * @endcode
     */
    template <typename ParallelStateManager, typename RegionList>
    static bool areAllRegionsInFinalState(ParallelStateManager &parallelManager, const RegionList &regions) {
        LOG_DEBUG("ParallelProcessingAlgorithms: Checking if {} regions are in final state", regions.size());

        for (const auto &region : regions) {
            if (!parallelManager.isRegionInFinalState(region)) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief W3C SCXML 3.4: Exit all regions of a parallel state
     *
     * When exiting a parallel state, ALL child regions must be exited.
     * Exit actions for each region are executed in reverse document order.
     *
     * @tparam ParallelStateManager Interface for parallel state management
     *         Required methods:
     *         - void exitRegion(const RegionId& regionId)
     *
     * @tparam RegionList Container of region identifiers
     *
     * @param parallelManager Parallel state manager providing region operations
     * @param regions List of child region identifiers to exit
     *
     * @example Interpreter usage:
     * @code
     * RSM::Core::InterpreterParallelStateManager adapter(stateNode);
     * RSM::Core::ParallelProcessingAlgorithms::exitAllRegions(
     *     adapter,
     *     stateNode->getChildRegions()
     * );
     * @endcode
     *
     * @example JIT usage:
     * @code
     * RSM::Core::JITParallelStateManager<Policy> adapter(policy_);
     * std::vector<State> regions = { State::Region1, State::Region2 };
     * RSM::Core::ParallelProcessingAlgorithms::exitAllRegions(
     *     adapter,
     *     regions
     * );
     * @endcode
     */
    template <typename ParallelStateManager, typename RegionList>
    static void exitAllRegions(ParallelStateManager &parallelManager, const RegionList &regions) {
        LOG_DEBUG("ParallelProcessingAlgorithms: Exiting {} parallel regions", regions.size());

        // W3C SCXML: Exit in reverse document order
        for (auto it = regions.rbegin(); it != regions.rend(); ++it) {
            parallelManager.exitRegion(*it);
        }
    }
};

}  // namespace RSM::Core
