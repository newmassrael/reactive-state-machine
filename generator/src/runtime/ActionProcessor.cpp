#include "runtime/ActionProcessor.h"
#include "common/Logger.h"
#include "core/actions/AssignActionNode.h"
#include "core/actions/CancelActionNode.h"
#include "core/actions/ForeachActionNode.h"
#include "core/actions/IfActionNode.h"
#include "core/actions/LogActionNode.h"
#include "core/actions/RaiseActionNode.h"
#include "core/actions/ScriptActionNode.h"
#include "core/actions/SendActionNode.h"
#include "events/Event.h"
#include "model/DocumentModel.h"
#include "model/IActionNode.h"
#include "model/IStateNode.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {
namespace Runtime {

ActionProcessor::ActionProcessor()
    : expressionEvaluator_(nullptr), executorFactory_(std::make_shared<DefaultActionExecutorFactory>()) {}

ActionProcessor::ActionResult ActionProcessor::executeEntryActions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                                   const std::string &stateId,
                                                                   SCXML::Events::EventPtr triggeringEvent,
                                                                   SCXML::Runtime::RuntimeContext &context) {
    clearState();
    ActionResult result;

    if (!model || stateId.empty()) {
        result.errorMessage = "Invalid model or state ID";
        return result;
    }

    try {
        // Set up action context
        ActionContext actionCtx;
        actionCtx.stateId = stateId;
        actionCtx.triggeringEvent = triggeringEvent;
        actionCtx.isEntry = true;

        // Get entry actions for the state
        auto entryActions = getEntryActions(model, stateId);
        if (entryActions.empty()) {
            // No entry actions - this is success
            result.success = true;
            return result;
        }

        // Execute the actions
        result = executeActionList(entryActions, actionCtx, context);

        // Generate system events if successful
        if (result.success) {
            generateSystemEvents(stateId, true, context);

            // Generate done event if this is a final state
            if (isFinalState(model, stateId)) {
                generateDoneEvent(stateId, context);
            }
        }

        return result;

    } catch (const std::exception &e) {
        result.errorMessage = "Exception during entry action execution: " + std::string(e.what());
        return result;
    }
}

ActionProcessor::ActionResult ActionProcessor::executeExitActions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                                  const std::string &stateId,
                                                                  SCXML::Events::EventPtr triggeringEvent,
                                                                  SCXML::Runtime::RuntimeContext &context) {
    clearState();
    ActionResult result;

    if (!model || stateId.empty()) {
        result.errorMessage = "Invalid model or state ID";
        return result;
    }

    try {
        // Set up action context
        ActionContext actionCtx;
        actionCtx.stateId = stateId;
        actionCtx.triggeringEvent = triggeringEvent;
        actionCtx.isEntry = false;

        // Get exit actions for the state
        auto exitActions = getExitActions(model, stateId);

        if (exitActions.empty()) {
            // No exit actions - this is success
            result.success = true;
            return result;
        }

        // Execute the actions
        result = executeActionList(exitActions, actionCtx, context);

        // Generate system events if successful
        if (result.success) {
            generateSystemEvents(stateId, false, context);
        }

        return result;

    } catch (const std::exception &e) {
        result.errorMessage = "Exception during exit action execution: " + std::string(e.what());
        return result;
    }
}

ActionProcessor::ActionResult ActionProcessor::executeMultipleEntryActions(
    std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::vector<std::string> &stateIds,
    SCXML::Events::EventPtr triggeringEvent, SCXML::Runtime::RuntimeContext &context) {
    ActionResult combinedResult;
    combinedResult.success = true;

    for (const auto &stateId : stateIds) {
        auto result = executeEntryActions(model, stateId, triggeringEvent, context);

        // Combine results
        combinedResult.executedActions.insert(combinedResult.executedActions.end(), result.executedActions.begin(),
                                              result.executedActions.end());

        if (!result.success) {
            combinedResult.success = false;
            if (!combinedResult.errorMessage.empty()) {
                combinedResult.errorMessage += "; ";
            }
            combinedResult.errorMessage += result.errorMessage;
        }
    }

    return combinedResult;
}

ActionProcessor::ActionResult ActionProcessor::executeMultipleExitActions(
    std::shared_ptr<::SCXML::Model::DocumentModel> model, const std::vector<std::string> &stateIds,
    SCXML::Events::EventPtr triggeringEvent, SCXML::Runtime::RuntimeContext &context) {
    ActionResult combinedResult;
    combinedResult.success = true;

    for (const auto &stateId : stateIds) {
        auto result = executeExitActions(model, stateId, triggeringEvent, context);

        // Combine results
        combinedResult.executedActions.insert(combinedResult.executedActions.end(), result.executedActions.begin(),
                                              result.executedActions.end());

        if (!result.success) {
            combinedResult.success = false;
            if (!combinedResult.errorMessage.empty()) {
                combinedResult.errorMessage += "; ";
            }
            combinedResult.errorMessage += result.errorMessage;
        }
    }

    return combinedResult;
}

bool ActionProcessor::hasEntryActions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                      const std::string &stateId) const {
    if (!model || stateId.empty()) {
        return false;
    }

    auto actions = getEntryActions(model, stateId);
    return !actions.empty();
}

bool ActionProcessor::hasExitActions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                     const std::string &stateId) const {
    if (!model || stateId.empty()) {
        return false;
    }

    auto actions = getExitActions(model, stateId);
    return !actions.empty();
}

// ========== Protected Methods ==========

std::vector<std::shared_ptr<SCXML::Model::IActionNode>>
ActionProcessor::getEntryActions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                 const std::string &stateId) const {
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> actions;

    if (!model) {
        return actions;
    }
    SCXML::Model::IStateNode *stateNode = model->findStateById(stateId);
    if (!stateNode) {
        return actions;
    }

    // Get entry action nodes from the state node
    actions = stateNode->getEntryActionNodes();
    return actions;
}

std::vector<std::shared_ptr<SCXML::Model::IActionNode>>
ActionProcessor::getExitActions(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                const std::string &stateId) const {
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> actions;

    if (!model) {
        return actions;
    }

    SCXML::Model::IStateNode *stateNode = model->findStateById(stateId);
    if (!stateNode) {
        return actions;
    }

    return actions;
}

ActionProcessor::ActionResult
ActionProcessor::executeActionList(const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &actions,
                                   const ActionContext &actionCtx, SCXML::Runtime::RuntimeContext &context) {
    ActionResult result;
    result.success = true;

    for (auto action : actions) {
        if (!action) {
            continue;
        }

        bool actionSuccess = executeSingleAction(action, actionCtx, context);

        if (actionSuccess) {
            result.executedActions.push_back(action->getId());
        } else {
            result.success = false;
            handleActionError(action, "Action execution failed", actionCtx, context);
        }
    }

    return result;
}

bool ActionProcessor::executeSingleAction(std::shared_ptr<SCXML::Model::IActionNode> action,
                                          const ActionContext &actionCtx, SCXML::Runtime::RuntimeContext &context) {
    if (!action) {
        return false;
    }

    try {
        // Get action type and find appropriate executor
        std::string actionType = action->getActionType();
        SCXML::Common::Logger::error("ActionProcessor::executeSingleAction - Processing action type: " + actionType +
                                     ", ID: " + action->getId());

        // Debug: Check actual runtime type
        const std::type_info &actionRuntimeType = typeid(*action);
        SCXML::Common::Logger::error("ActionProcessor::executeSingleAction - Actual runtime type: " +
                                     std::string(actionRuntimeType.name()));

        auto executor = executorFactory_->createExecutor(actionType);

        if (!executor) {
            addError("No executor found for action type: " + actionType);
            return false;
        }

        // Try casting to concrete action types
        Core::ActionNode *coreAction = nullptr;

        if (actionType == "assign") {
            auto assignAction = std::dynamic_pointer_cast<Core::AssignActionNode>(action);
            if (assignAction) {
                coreAction = assignAction.get();
            }
        } else if (actionType == "log") {
            auto logAction = std::dynamic_pointer_cast<Core::LogActionNode>(action);
            if (logAction) {
                coreAction = logAction.get();
            }
        } else if (actionType == "raise") {
            auto raiseAction = std::dynamic_pointer_cast<Core::RaiseActionNode>(action);
            if (raiseAction) {
                coreAction = raiseAction.get();
            }
        } else if (actionType == "if") {
            auto ifAction = std::dynamic_pointer_cast<Core::IfActionNode>(action);
            if (ifAction) {
                coreAction = ifAction.get();
            }
        } else if (actionType == "send") {
            auto sendAction = std::dynamic_pointer_cast<Core::SendActionNode>(action);
            if (sendAction) {
                coreAction = sendAction.get();
            }
        } else if (actionType == "script") {
            auto scriptAction = std::dynamic_pointer_cast<Core::ScriptActionNode>(action);
            if (scriptAction) {
                coreAction = scriptAction.get();
            }
        } else if (actionType == "cancel") {
            auto cancelAction = std::dynamic_pointer_cast<Core::CancelActionNode>(action);
            if (cancelAction) {
                coreAction = cancelAction.get();
            }
        } else if (actionType == "foreach") {
            auto foreachAction = std::dynamic_pointer_cast<Core::ForeachActionNode>(action);
            if (foreachAction) {
                coreAction = foreachAction.get();
            }
        }

        if (!coreAction) {
            addError("Failed to cast action to concrete type: " + actionType + " (" + action->getId() + ")");
            return false;
        }

        // Execute action using the appropriate executor
        bool result = executor->execute(*coreAction, context);
        if (!result) {
            addError("Action execution failed: " + action->getId());
            return false;
        }

        context.log("debug", std::string("Executing ") + (actionCtx.isEntry ? "entry" : "exit") + " action '" +
                                 action->getId() + "' for state '" + actionCtx.stateId + "'");

        return true;

    } catch (const std::exception &e) {
        addError("Action execution failed: " + std::string(e.what()));
        return false;
    }
}

void ActionProcessor::handleActionError(std::shared_ptr<SCXML::Model::IActionNode> action, const std::string &error,
                                        const ActionContext &actionCtx, SCXML::Runtime::RuntimeContext &context) {
    std::string errorMsg = "Action '" + action->getId() + "' failed in state '" + actionCtx.stateId + "': " + error;

    context.log("error", errorMsg);

    // Generate error event
    auto errorEvent = context.createEvent("error.execution", errorMsg, "platform");
    context.raiseEvent(errorEvent);
}

void ActionProcessor::generateSystemEvents(const std::string &stateId, bool isEntry,
                                           SCXML::Runtime::RuntimeContext &context) {
    if (isEntry) {
        // Generate entry event (informational)
        auto entryEvent = context.createEvent("system.state.entered", stateId, "platform");
        context.raiseEvent(entryEvent);
    } else {
        // Generate exit event (informational)
        auto exitEvent = context.createEvent("system.state.exited", stateId, "platform");
        context.raiseEvent(exitEvent);
    }
}

bool ActionProcessor::isFinalState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                   const std::string &stateId) const {
    if (!model) {
        return false;
    }

    SCXML::Model::IStateNode *stateNode = model->findStateById(stateId);
    if (!stateNode) {
        return false;
    }

    // Check if state is final
    return stateNode->isFinalState();
}

void ActionProcessor::generateDoneEvent(const std::string &stateId, SCXML::Runtime::RuntimeContext &context) {
    // Generate done.state.stateId event
    std::string doneEventName = "done.state." + stateId;
    auto doneEvent = context.createEvent(doneEventName, stateId, "platform");
    context.raiseEvent(doneEvent);

    context.log("info", "Generated done event for final state: " + stateId);
}

// ========== Private Methods ==========

void ActionProcessor::addError(const std::string &message) {
    errorMessages_.push_back(message);
}

void ActionProcessor::clearState() {
    errorMessages_.clear();
}

void ActionProcessor::setExpressionEvaluator(ExpressionEvaluator &evaluator) {
    expressionEvaluator_ = &evaluator;
}

}  // namespace Runtime
}  // namespace SCXML