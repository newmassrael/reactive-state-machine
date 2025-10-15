#pragma once

#include <string>
#include <vector>

/**
 * @file ParallelStateAdapters.h
 * @brief Adapter pattern for Parallel state management (Phase 4)
 *
 * Design principles (same as InvokeManagerAdapters and EventQueueAdapters):
 * 1. Minimal interface for ParallelProcessingAlgorithms
 * 2. Engine-specific implementations hidden behind uniform API
 * 3. Zero overhead through inline methods
 *
 * Required interface for ParallelProcessingAlgorithms:
 * - void enterRegion(const RegionId& regionId)
 * - bool processRegionEvent(const RegionId& regionId, const Event& event)
 * - bool isRegionInFinalState(const RegionId& regionId)
 * - void exitRegion(const RegionId& regionId)
 */

namespace RSM::Core {

// Forward declarations
class StateNode;
class StateMachine;

/**
 * @brief Interpreter engine parallel state manager adapter
 *
 * Adapts Interpreter's StateNode (tree-based parallel state management) to the unified
 * interface required by ParallelProcessingAlgorithms.
 *
 * Implementation notes:
 * - Interpreter uses StateNode tree structure for parallel regions
 * - Each region is a child StateNode of the parallel state
 * - Adapter delegates to StateNode's region management methods
 *
 * @example Usage in Interpreter StateMachine.cpp:
 * @code
 * RSM::Core::InterpreterParallelStateManager adapter(parallelStateNode);
 * RSM::Core::ParallelProcessingAlgorithms::enterAllRegions(
 *     adapter,
 *     parallelStateNode->getChildRegions()
 * );
 * @endcode
 */
class InterpreterParallelStateManager {
public:
    /**
     * @brief Constructor
     * @param parallelState Parallel state node containing regions
     */
    explicit InterpreterParallelStateManager(StateNode *parallelState) : parallelState_(parallelState) {}

    /**
     * @brief Enter a parallel region (W3C SCXML 3.4)
     * @param regionNode Region StateNode to enter
     */
    void enterRegion(StateNode *regionNode) {
        if (regionNode) {
            // Delegate to StateNode's enterState logic
            regionNode->enter();
        }
    }

    /**
     * @brief Process event in a parallel region (W3C SCXML D.1)
     * @param regionNode Region StateNode
     * @param event Event to process
     * @return true if region took a transition, false otherwise
     */
    template <typename Event> bool processRegionEvent(StateNode *regionNode, const Event &event) {
        if (regionNode) {
            // Delegate to StateNode's event processing
            return regionNode->processEvent(event);
        }
        return false;
    }

    /**
     * @brief Check if region is in final state (W3C SCXML 3.4)
     * @param regionNode Region StateNode
     * @return true if region is in final state, false otherwise
     */
    bool isRegionInFinalState(StateNode *regionNode) const {
        if (regionNode) {
            // Delegate to StateNode's final state check
            return regionNode->isInFinalState();
        }
        return false;
    }

    /**
     * @brief Exit a parallel region (W3C SCXML 3.4)
     * @param regionNode Region StateNode to exit
     */
    void exitRegion(StateNode *regionNode) {
        if (regionNode) {
            // Delegate to StateNode's exitState logic
            regionNode->exit();
        }
    }

private:
    StateNode *parallelState_;
};

/**
 * @brief JIT engine parallel state manager adapter
 *
 * Adapts JIT's Policy class (containing parallel region state variables) to the unified
 * interface required by ParallelProcessingAlgorithms.
 *
 * Implementation notes:
 * - Policy stores region states in parallel_<state>_region<N>State_ variables
 * - Generated code includes region management logic
 * - Adapter manipulates Policy's flat state variables
 *
 * @tparam Policy Generated policy class from StaticCodeGenerator
 *         Must provide:
 *         - State parallel_<parallelState>_region<N>State_ variables
 *         - processEvent(State regionState, Event event) method
 *         - isFinalState(State regionState) method
 *
 * @example Usage in generated JIT code:
 * @code
 * RSM::Core::JITParallelStateManager<MyStateMachinePolicy> adapter(policy_, State::ParallelState);
 * std::vector<State> regions = { State::Region1, State::Region2 };
 * RSM::Core::ParallelProcessingAlgorithms::enterAllRegions(
 *     adapter,
 *     regions
 * );
 * @endcode
 */
template <typename Policy> class JITParallelStateManager {
public:
    using State = typename Policy::State;

    /**
     * @brief Constructor
     * @param policy Reference to Policy instance containing parallel state data
     * @param parallelState The parallel state enum value (for region variable access)
     */
    JITParallelStateManager(Policy &policy, State parallelState) : policy_(policy), parallelState_(parallelState) {}

    /**
     * @brief Enter a parallel region (W3C SCXML 3.4)
     * @param regionState Region state enum value to enter
     */
    void enterRegion(State regionState) {
        // Execute entry actions for the region
        policy_.executeEntryActions(regionState, policy_);
    }

    /**
     * @brief Process event in a parallel region (W3C SCXML D.1)
     * @param regionState Region state enum value
     * @param event Event to process
     * @return true if region took a transition, false otherwise
     */
    template <typename Event> bool processRegionEvent(State regionState, const Event &event) {
        // Process event for this region state
        return policy_.processEvent(regionState, event);
    }

    /**
     * @brief Check if region is in final state (W3C SCXML 3.4)
     * @param regionState Region state enum value
     * @return true if region is in final state, false otherwise
     */
    bool isRegionInFinalState(State regionState) const {
        // Check if region state is a final state
        return policy_.isFinalState(regionState);
    }

    /**
     * @brief Exit a parallel region (W3C SCXML 3.4)
     * @param regionState Region state enum value to exit
     */
    void exitRegion(State regionState) {
        // Execute exit actions for the region
        policy_.executeExitActions(regionState, policy_);
    }

private:
    Policy &policy_;
    State parallelState_;
};

}  // namespace RSM::Core
