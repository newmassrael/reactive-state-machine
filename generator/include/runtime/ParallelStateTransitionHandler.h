#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
namespace SCXML {

namespace Runtime {
class RuntimeContext;
}

class ParallelStateNode;

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events
}  // namespace SCXML

/**
 * @brief Handles transitions involving parallel states
 *
 * This class manages the complex transition logic for parallel states including:
 * - Transitions from regular states to parallel states
 * - Transitions from parallel states to regular states
 * - Transitions between parallel states
 * - Internal transitions within parallel state regions
 * - Conditional transitions based on region completion
 */
class ParallelStateTransitionHandler {
public:
    /**
     * @brief Transition execution result
     */
    struct TransitionResult {
        bool success = false;
        bool stateChanged = false;
        std::vector<std::string> enteredStates;
        std::vector<std::string> exitedStates;
        std::vector<std::string> errorMessages;
        std::string targetState;

        // Parallel state specific results
        std::unordered_map<std::string, std::vector<std::string>> regionTransitions;
        std::unordered_set<std::string> completedRegions;
        bool allParallelRegionsComplete = false;
    };

    /**
     * @brief Parallel transition context
     */
    struct ParallelTransitionContext {
        std::string sourceParallelState;
        std::string targetParallelState;
        std::vector<std::string> sourceRegions;
        std::vector<std::string> targetRegions;
        SCXML::Events::EventPtr triggerEvent;
        std::unordered_map<std::string, std::string> regionStates;

        bool isInternalTransition() const {
            return sourceParallelState == targetParallelState && !sourceParallelState.empty();
        }

        bool isEnteringParallelState() const {
            return sourceParallelState.empty() && !targetParallelState.empty();
        }

        bool isExitingParallelState() const {
            return !sourceParallelState.empty() && targetParallelState.empty();
        }

        bool isParallelToParallelTransition() const {
            return !sourceParallelState.empty() && !targetParallelState.empty() &&
                   sourceParallelState != targetParallelState;
        }
    };

public:
    /**
     * @brief Constructor
     */
    ParallelStateTransitionHandler();

    /**
     * @brief Destructor
     */
    virtual ~ParallelStateTransitionHandler() = default;

    /**
     * @brief Execute transition involving parallel states
     * @param transition Transition to execute
     * @param event Triggering event
     * @param context Runtime context
     * @return Transition execution result
     */
    TransitionResult executeParallelTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                               SCXML::Events::EventPtr event, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Handle entry into parallel state
     * @param parallelState Parallel state to enter
     * @param context Runtime context
     * @return Transition result with entry status
     */
    TransitionResult enterParallelState(std::shared_ptr<ParallelStateNode> parallelState,
                                        SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Handle exit from parallel state
     * @param parallelState Parallel state to exit
     * @param context Runtime context
     * @return Transition result with exit status
     */
    TransitionResult exitParallelState(std::shared_ptr<ParallelStateNode> parallelState,
                                       SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Handle internal transition within parallel state
     * @param parallelState Containing parallel state
     * @param sourceRegion Source region for transition
     * @param transition Internal transition to execute
     * @param event Triggering event
     * @param context Runtime context
     * @return Transition result
     */
    TransitionResult executeInternalParallelTransition(std::shared_ptr<ParallelStateNode> parallelState,
                                                       const std::string &sourceRegion,
                                                       std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                                       SCXML::Events::EventPtr event,
                                                       SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if transition can be taken based on parallel state conditions
     * @param transition Transition to check
     * @param event Triggering event
     * @param context Runtime context
     * @return true if transition can be taken
     */
    bool canTakeParallelTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                   SCXML::Events::EventPtr event, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get transitions available from parallel state
     * @param parallelState Parallel state to check
     * @param event Current event
     * @param context Runtime context
     * @return Vector of available transitions
     */
    std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>>
    getAvailableParallelTransitions(std::shared_ptr<ParallelStateNode> parallelState, SCXML::Events::EventPtr event,
                                    SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Compute transition targets for parallel state transitions
     * @param transition Transition with parallel source/target
     * @param context Runtime context
     * @return Set of computed target state IDs
     */
    std::unordered_set<std::string>
    computeParallelTransitionTargets(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                     SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Create parallel transition context from current state
     * @param sourceStateId Current active state ID
     * @param transition Transition being evaluated
     * @param event Triggering event
     * @param context Runtime context
     * @return Parallel transition context
     */
    ParallelTransitionContext createTransitionContext(const std::string &sourceStateId,
                                                      std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                                      SCXML::Events::EventPtr event,
                                                      SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check completion conditions for parallel state exit
     * @param parallelState Parallel state to check
     * @param context Runtime context
     * @return true if all completion conditions are met
     */
    bool checkParallelCompletionConditions(std::shared_ptr<ParallelStateNode> parallelState,
                                           SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Validate parallel transition structure
     * @param transition Transition to validate
     * @return Vector of validation error messages
     */
    std::vector<std::string> validateParallelTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition);

protected:
    /**
     * @brief Execute region-specific transitions within parallel state
     * @param parallelState Containing parallel state
     * @param event Triggering event
     * @param context Runtime context
     * @return Map of region ID to transition results
     */
    std::unordered_map<std::string, TransitionResult>
    executeRegionTransitions(std::shared_ptr<ParallelStateNode> parallelState, SCXML::Events::EventPtr event,
                             SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Find target state node from transition
     * @param transition Transition containing targets
     * @param context Runtime context
     * @return Target state node if found
     */
    std::shared_ptr<SCXML::Model::IStateNode> findTargetState(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                                             SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if state is a parallel state
     * @param state State to check
     * @return true if state is parallel
     */
    bool isParallelState(std::shared_ptr<SCXML::Model::IStateNode> state) const;

    /**
     * @brief Find containing parallel state for a given state
     * @param stateId State ID to search from
     * @param context Runtime context
     * @return Parallel state containing the given state, or nullptr
     */
    std::shared_ptr<ParallelStateNode> findContainingParallelState(const std::string &stateId,
                                                                   SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute transition actions
     * @param transition Transition containing actions
     * @param context Runtime context
     * @return true if all actions executed successfully
     */
    bool executeTransitionActions(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                  SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Evaluate transition guard conditions
     * @param transition Transition with conditions
     * @param event Triggering event
     * @param context Runtime context
     * @return true if all conditions are satisfied
     */
    bool evaluateTransitionConditions(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                      SCXML::Events::EventPtr event, SCXML::Runtime::RuntimeContext &context);

private:
    /**
     * @brief Helper to merge multiple transition results
     * @param results Vector of transition results to merge
     * @return Merged transition result
     */
    TransitionResult mergeTransitionResults(const std::vector<TransitionResult> &results);

    /**
     * @brief Generate unique transition ID for tracking
     * @param transition Transition to generate ID for
     * @return Unique transition identifier
     */
    std::string generateTransitionId(std::shared_ptr<SCXML::Model::ITransitionNode> transition) const;
};