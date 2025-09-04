#pragma once

#include <memory>
#include <string>
#include <vector>
#include "model/IActionNode.h"
#include "runtime/ActionExecutor.h"

namespace SCXML {

namespace Model {
class DocumentModel;
}
namespace Core {
class DocumentModel;
}

namespace Core {
// Forward declaration moved to Model namespace
// Forward declaration moved to Model namespace
}  // namespace Core

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {
class RuntimeContext;
class ExpressionEvaluator;

/**
 * @brief Action Processor for SCXML
 *
 * This class handles the execution of state entry and exit actions according to
 * the W3C SCXML specification. It manages onentry and onexit executable content
 * and ensures proper execution order.
 */

class ActionProcessor {
public:
    /**
     * @brief Action execution result
     */
    struct ActionResult {
        bool success;                              // Whether execution succeeded
        std::vector<std::string> executedActions;  // IDs of actions that were executed
        std::string errorMessage;                  // Error message if execution failed

        ActionResult() : success(false) {}
    };

    /**
     * @brief Action execution context
     */
    struct ActionContext {
        std::string stateId;                      // State being processed
        SCXML::Events::EventPtr triggeringEvent;  // Event that caused state change
        bool isEntry;                             // true for entry, false for exit

        ActionContext() : isEntry(true) {}
    };

    /**
     * @brief Construct a new Action Processor
     */
    ActionProcessor();

    /**
     * @brief Destructor
     */
    ~ActionProcessor() = default;

    /**
     * @brief Set expression evaluator for action expressions
     * @param evaluator Expression evaluator instance
     */
    void setExpressionEvaluator(SCXML::Runtime::ExpressionEvaluator &evaluator);

    /**
     * @brief Execute entry actions for a state
     * @param model SCXML model
     * @param stateId State ID to execute entry actions for
     * @param triggeringEvent Event that caused state entry
     * @param context Runtime context
     * @return Action execution result
     */
    ActionResult executeEntryActions(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId,
                                     SCXML::Events::EventPtr triggeringEvent, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute exit actions for a state
     * @param model SCXML model
     * @param stateId State ID to execute exit actions for
     * @param triggeringEvent Event that caused state exit
     * @param context Runtime context
     * @return Action execution result
     */
    ActionResult executeExitActions(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId,
                                    SCXML::Events::EventPtr triggeringEvent, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute entry actions for multiple states in proper order
     * @param model SCXML model
     * @param stateIds State IDs in entry order
     * @param triggeringEvent Event that caused state changes
     * @param context Runtime context
     * @return Combined action execution result
     */
    ActionResult executeMultipleEntryActions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                             const std::vector<std::string> &stateIds,
                                             SCXML::Events::EventPtr triggeringEvent,
                                             SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute exit actions for multiple states in proper order
     * @param model SCXML model
     * @param stateIds State IDs in exit order
     * @param triggeringEvent Event that caused state changes
     * @param context Runtime context
     * @return Combined action execution result
     */
    ActionResult executeMultipleExitActions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                            const std::vector<std::string> &stateIds,
                                            SCXML::Events::EventPtr triggeringEvent,
                                            SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if state has entry actions
     * @param model SCXML model
     * @param stateId State ID to check
     * @return true if state has entry actions
     */
    bool hasEntryActions(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId) const;

    /**
     * @brief Check if state has exit actions
     * @param model SCXML model
     * @param stateId State ID to check
     * @return true if state has exit actions
     */
    bool hasExitActions(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId) const;

protected:
    /**
     * @brief Get entry actions for a state
     * @param model SCXML model
     * @param stateId State ID
     * @return Vector of entry actions
     */
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>>
    getEntryActions(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId) const;

    /**
     * @brief Get exit actions for a state
     * @param model SCXML model
     * @param stateId State ID
     * @return Vector of exit actions
     */
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>>
    getExitActions(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId) const;

    /**
     * @brief Execute a list of actions
     * @param actions Actions to execute
     * @param actionCtx Action context
     * @param context Runtime context
     * @return Action execution result
     */
    ActionResult executeActionList(const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &actions,
                                   const ActionContext &actionCtx, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute a single action
     * @param action Action to execute
     * @param actionCtx Action context
     * @param context Runtime context
     * @return true if action executed successfully
     */
    bool executeSingleAction(std::shared_ptr<SCXML::Model::IActionNode> action, const ActionContext &actionCtx,
                             SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Handle action execution error
     * @param action Action that failed
     * @param error Error message
     * @param actionCtx Action context
     * @param context Runtime context
     */
    void handleActionError(std::shared_ptr<SCXML::Model::IActionNode> action, const std::string &error,
                           const ActionContext &actionCtx, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Generate system events for state changes
     * @param stateId State that was entered/exited
     * @param isEntry true for entry, false for exit
     * @param context Runtime context
     */
    void generateSystemEvents(const std::string &stateId, bool isEntry, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if state is final state
     * @param model SCXML model
     * @param stateId State ID to check
     * @return true if state is final
     */
    bool isFinalState(std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::string &stateId) const;

    /**
     * @brief Generate done event for final state
     * @param stateId Final state ID
     * @param context Runtime context
     */
    void generateDoneEvent(const std::string &stateId, SCXML::Runtime::RuntimeContext &context);

private:
    // Internal state for error handling
    std::vector<std::string> errorMessages_;

    // Expression evaluation
    SCXML::Runtime::ExpressionEvaluator *expressionEvaluator_;

    // Action execution factory
    std::shared_ptr<ActionExecutorFactory> executorFactory_;

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

}  // namespace Runtime
}  // namespace SCXML