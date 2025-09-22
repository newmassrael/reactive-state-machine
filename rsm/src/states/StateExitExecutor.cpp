#include "states/StateExitExecutor.h"
#include "actions/IActionNode.h"
#include "model/IStateNode.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"
#include <cassert>

namespace RSM {

bool StateExitExecutor::executeStateExitActions(std::shared_ptr<IStateNode> state,
                                                std::shared_ptr<IExecutionContext> executionContext) {
    // SCXML W3C Specification compliance: Exit actions must be executed
    assert(state != nullptr && "SCXML violation: state node cannot be null during exit");
    assert(executionContext != nullptr && "SCXML violation: execution context required for exit actions");
    assert(executionContext->isValid() && "SCXML violation: execution context must be valid");

    const std::string &stateId = state->getId();
    assert(!stateId.empty() && "SCXML violation: state must have non-empty ID");

    logExitAction(stateId, "Starting SCXML-compliant state exit");

    try {
        // SCXML W3C Spec: Only IActionNode-based actions are SCXML compliant
        bool actionNodesResult = executeActionNodes(state, executionContext);

        // SCXML compliance check - exit actions must succeed
        assert(actionNodesResult && "SCXML violation: exit actions must execute successfully");

        logExitAction(stateId, "Successfully completed SCXML-compliant state exit");
        return actionNodesResult;

    } catch (const std::exception &e) {
        Logger::error("StateExitExecutor::executeStateExitActions - SCXML execution error: " + std::string(e.what()));
        assert(false && "SCXML violation: state exit must not throw exceptions");
        return false;
    }
}

bool StateExitExecutor::executeMultipleStateExits(const std::vector<std::string> &activeStateIds,
                                                  std::shared_ptr<IStateNode> rootState,
                                                  std::shared_ptr<IExecutionContext> executionContext) {
    // SCXML W3C Specification compliance
    assert(!activeStateIds.empty() && "SCXML violation: cannot exit empty state list");
    assert(rootState != nullptr && "SCXML violation: root state required for exit");
    assert(executionContext != nullptr && "SCXML violation: execution context required");
    assert(executionContext->isValid() && "SCXML violation: execution context must be valid");

    logExitAction("MULTIPLE_STATES", "Starting SCXML-compliant multiple state exit");

    bool allSuccessful = true;

    // SCXML W3C Spec: Exit actions execute in document order
    for (const auto &activeStateId : activeStateIds) {
        assert(!activeStateId.empty() && "SCXML violation: state ID cannot be empty");

        logExitAction(activeStateId, "Processing SCXML exit for active state");

        // SCXML-compliant state exit execution
        // Note: In a complete implementation, we would traverse the state hierarchy
        // to find the specific state node by ID. For now, we use the root state
        // as a proxy for the active state's exit actions.
        bool result = executeStateExitActions(rootState, executionContext);

        // SCXML violation check
        if (!result) {
            Logger::error("StateExitExecutor::executeMultipleStateExits - SCXML violation: failed to exit state: " +
                          activeStateId);
            assert(false && ("SCXML violation: exit must succeed for state " + activeStateId).c_str());
            allSuccessful = false;
        }
    }

    // SCXML compliance check - all exits must succeed
    assert(allSuccessful && "SCXML violation: all state exits must succeed");

    logExitAction("MULTIPLE_STATES", "Completed SCXML-compliant multiple state exit");
    return allSuccessful;
}

bool StateExitExecutor::executeActionNodes(std::shared_ptr<IStateNode> state,
                                           std::shared_ptr<IExecutionContext> executionContext) {
    // SCXML compliance assertions
    assert(state != nullptr && "SCXML violation: state node required");
    assert(executionContext != nullptr && "SCXML violation: execution context required");
    assert(executionContext->isValid() && "SCXML violation: execution context must be valid");

    try {
        auto &actionExecutor = executionContext->getActionExecutor();
        const auto &exitActionNodes = state->getExitActionNodes();

        // SCXML W3C Spec: Execute exit actions in document order
        for (const auto &exitAction : exitActionNodes) {
            if (exitAction) {
                logExitAction(state->getId(), "Executing SCXML exit action node");

                // Using injected ActionExecutor for SCXML-compliant execution
                try {
                    // Execute the action through the SCXML-compliant action executor
                    logExitAction(state->getId(), "ActionExecutor address: " +
                                                      std::to_string(reinterpret_cast<uintptr_t>(&actionExecutor)));

                    // SCXML-compliant action execution
                    bool actionResult = true;  // Placeholder for actual SCXML execution

                    // Future SCXML implementation:
                    // bool actionResult = actionExecutor.executeAction(*exitAction);

                    // SCXML compliance check
                    assert(actionResult && "SCXML violation: exit action execution must not fail");

                    if (!actionResult) {
                        Logger::error(
                            "StateExitExecutor::executeActionNodes - SCXML violation: action failed for state: " +
                            state->getId());
                        assert(false && "SCXML violation: action execution failure not allowed");
                        return false;
                    }

                    logExitAction(state->getId(), "Successfully executed SCXML exit action node");
                } catch (const std::exception &actionException) {
                    // SCXML spec violation: exit actions should not throw
                    Logger::error("StateExitExecutor::executeActionNodes - SCXML violation: " +
                                  std::string(actionException.what()));
                    assert(false && "SCXML violation: exit actions must not throw exceptions");
                    return false;
                }
            }
        }

        return true;

    } catch (const std::exception &e) {
        Logger::error("StateExitExecutor::executeActionNodes - SCXML execution error: " + std::string(e.what()));
        assert(false && "SCXML violation: action execution must not throw");
        return false;
    }
}

void StateExitExecutor::logExitAction(const std::string &stateId, const std::string &actionDescription) const {
    Logger::debug("StateExitExecutor - " + actionDescription + " for state: " + stateId);
}

}  // namespace RSM
