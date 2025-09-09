#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations
namespace SCXML {

namespace Model {
class DocumentModel;
class ITransitionNode;
class IActionNode;
}  // namespace Model

// Forward declarations
namespace Core {
class DocumentModel;
}

namespace Core {
// Forward declaration moved to Model namespace
// Forward declaration moved to Model namespace
// Forward declaration moved to Model namespace
}  // namespace Core

namespace Runtime {
class RuntimeContext;
class ExpressionEvaluator;
}  // namespace Runtime

class GuardEvaluator;

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

/**
 * @brief State Transition Execution Engine for SCXML
 *
 * This class implements the SCXML state transition algorithm according to
 * the W3C specification. It evaluates transitions, executes actions, and
 * manages state changes in response to events.
 */
class TransitionExecutor {
public:
    /**
     * @brief Transition execution result
     */
    struct TransitionResult {
        bool transitionTaken;                    // Whether any transition was executed
        std::vector<std::string> exitedStates;   // States that were exited
        std::vector<std::string> enteredStates;  // States that were entered
        std::vector<std::string> targetStates;   // Final target states
        std::string errorMessage;                // Error message if execution failed

        TransitionResult() : transitionTaken(false) {}
    };

    /**
     * @brief Executable transition found during selection
     */
    struct ExecutableTransition {
        std::string sourceState;                                    // Source state ID
        std::string targetState;                                    // Target state ID
        std::shared_ptr<SCXML::Model::ITransitionNode> transition;  // Transition node
        SCXML::Events::EventPtr event;                              // Triggering event
        bool isInternal;                                            // Whether transition is internal
        int documentOrder;                                          // Document order for selection priority

        ExecutableTransition() : isInternal(false), documentOrder(0) {}
    };

    /**
     * @brief Construct a new Transition Executor
     */
    TransitionExecutor();

    /**
     * @brief Destructor
     */
    ~TransitionExecutor() = default;

    /**
     * @brief Set expression evaluator for transition conditions
     * @param evaluator Expression evaluator instance
     */
    void setExpressionEvaluator(SCXML::Runtime::ExpressionEvaluator &evaluator);

    /**
     * @brief Set guard evaluator for transition guards
     * @param evaluator Guard evaluator instance
     */
    void setGuardEvaluator(SCXML::GuardEvaluator &evaluator);

    /**
     * @brief Execute transitions for an event
     * @param model SCXML model
     * @param event Triggering event
     * @param context Runtime context
     * @return Transition execution result
     */
    TransitionResult executeTransitions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                        SCXML::Events::EventPtr event, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute eventless transitions with recursion protection
     * @param model SCXML model
     * @param context Runtime context
     * @param maxDepth Maximum recursion depth
     * @return Transition execution result
     */
    TransitionResult executeEventlessTransitions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                 SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Find enabled transitions for event in current configuration
     * @param model SCXML model
     * @param event Triggering event
     * @param context Runtime context
     * @return Vector of executable transitions
     */
    std::vector<ExecutableTransition> findEnabledTransitions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                             SCXML::Events::EventPtr event,
                                                             SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if transition is enabled for event
     * @param transition Transition to check
     * @param event Event to match
     * @param context Runtime context
     * @return true if transition is enabled
     */
    bool isTransitionEnabled(std::shared_ptr<SCXML::Model::ITransitionNode> transition, SCXML::Events::EventPtr event,
                             SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute a single transition
     * @param execTransition Executable transition
     * @param context Runtime context
     * @return true if execution succeeded
     */
    bool executeSingleTransition(const ExecutableTransition &execTransition, SCXML::Runtime::RuntimeContext &context);

protected:
    /**
     * @brief Select optimal transition set (resolve conflicts)
     * @param transitions Candidate transitions
     * @param context Runtime context
     * @return Selected transitions for execution
     */
    std::vector<ExecutableTransition> selectTransitions(const std::vector<ExecutableTransition> &transitions,
                                                        SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Compute exit set (states to be exited)
     * @param transitions Selected transitions
     * @param model SCXML model
     * @param context Runtime context
     * @return States to exit in proper order
     */
    std::vector<std::string> computeExitSet(const std::vector<ExecutableTransition> &transitions,
                                            std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                            SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Compute entry set (states to be entered)
     * @param transitions Selected transitions
     * @param model SCXML model
     * @param context Runtime context
     * @return States to enter in proper order
     */
    std::vector<std::string> computeEntrySet(const std::vector<ExecutableTransition> &transitions,
                                             std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                             SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute exit actions for states
     * @param exitStates States to exit
     * @param model SCXML model
     * @param context Runtime context
     * @return true if all exit actions succeeded
     */
    bool executeExitActions(const std::vector<std::string> &exitStates,
                            std::shared_ptr<::SCXML::Model::DocumentModel> model,
                            SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute transition actions
     * @param transitions Transitions to execute actions for
     * @param context Runtime context
     * @return true if all transition actions succeeded
     */
    bool executeTransitionActions(const std::vector<ExecutableTransition> &transitions,
                                  SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute entry actions for states
     * @param entryStates States to enter
     * @param model SCXML model
     * @param context Runtime context
     * @return true if all entry actions succeeded
     */
    bool executeEntryActions(const std::vector<std::string> &entryStates,
                             std::shared_ptr<::SCXML::Model::DocumentModel> model,
                             SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Match event against transition's event specification
     * @param eventSpec Event specification from transition (e.g., "button.click", "*")
     * @param eventName Actual event name
     * @return true if event matches specification
     */
    bool matchesEventSpec(const std::string &eventSpec, const std::string &eventName);

    /**
     * @brief Evaluate guard condition for transition
     * @param transition Transition with guard condition
     * @param context Runtime context
     * @return true if guard condition is satisfied
     */
    bool evaluateGuardCondition(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get transitions from a specific state
     * @param model SCXML model
     * @param stateId State ID to get transitions from
     * @return Vector of transitions from the state
     */
    std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>>
    getTransitionsFromState(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId);

    /**
     * @brief Check if transition is internal (doesn't exit source state)
     * @param transition Transition to check
     * @param model SCXML model
     * @return true if transition is internal
     */
    bool isInternalTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                              std::shared_ptr<::SCXML::Model::DocumentModel> model);

    /**
     * @brief Get least common compound ancestor of states
     * @param states State IDs
     * @param model SCXML model
     * @return Least common ancestor state ID
     */
    std::string getLeastCommonAncestor(const std::vector<std::string> &states,
                                       std::shared_ptr<::SCXML::Model::DocumentModel> model);

    /**
     * @brief Get proper ancestors of a state in document order
     * @param stateId State ID
     * @param model SCXML model
     * @return Ancestor state IDs from root to parent
     */
    std::vector<std::string> getProperAncestors(const std::string &stateId,
                                                std::shared_ptr<::SCXML::Model::DocumentModel> model);

    /**
     * @brief Execute actions from action list
     * @param actions Actions to execute
     * @param context Runtime context
     * @return true if all actions succeeded
     */
    bool executeActions(const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &actions,
                        SCXML::Runtime::RuntimeContext &context);

private:
    // Internal state for execution
    std::vector<std::string> errorMessages_;

    // Guard evaluation
    std::unique_ptr<SCXML::GuardEvaluator> guardEvaluator_;
    SCXML::GuardEvaluator *externalGuardEvaluator_;

    // Expression evaluation
    class SCXML::Runtime::ExpressionEvaluator *expressionEvaluator_;

    /**
     * @brief Add error message
     * @param message Error message to add
     */
    void addError(const std::string &message);

    /**
     * @brief Clear internal state
     */
    void clearState();
};

}  // namespace SCXML