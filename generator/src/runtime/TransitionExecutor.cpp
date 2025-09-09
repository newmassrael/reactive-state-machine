#include "runtime/TransitionExecutor.h"
#include "common/Logger.h"
#include "core/types.h"
#include "events/Event.h"
#include "model/DocumentModel.h"
#include "model/IActionNode.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "runtime/DataModelEngine.h"
#include "runtime/ExecutableContentProcessor.h"
#include "runtime/GuardEvaluator.h"
#include "runtime/IECMAScriptEngine.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <regex>
#include <set>

using namespace SCXML;

TransitionExecutor::TransitionExecutor()
    : guardEvaluator_(std::make_unique<GuardEvaluator>()), externalGuardEvaluator_(nullptr),
      expressionEvaluator_(nullptr) {}

TransitionExecutor::TransitionResult
TransitionExecutor::executeTransitions(std::shared_ptr<::Model::DocumentModel> model, Events::EventPtr event,
                                       Runtime::RuntimeContext &context) {
    clearState();
    TransitionResult result;

    if (!model || !event) {
        result.errorMessage = "Null model or event provided";
        return result;
    }

    //  TransitionExecutor JSON 이벤트 추적 시작
    std::string eventName = event->getName();
    std::string eventData = event->getDataAsString();

    // JSON 데이터 감지 및 경고
    if (!eventData.empty() && eventData.front() == '{') {
    }

    try {
        // 1. Find all enabled transitions for this event
        auto enabledTransitions = findEnabledTransitions(model, event, context);

        if (enabledTransitions.empty()) {
            // No transitions enabled - not an error, just no state change
            return result;
        }

        // 2. Select optimal transition set (resolve conflicts)
        auto selectedTransitions = selectTransitions(enabledTransitions, context);

        if (selectedTransitions.empty()) {
            return result;
        }

        // 3. Compute exit and entry sets
        auto exitSet = computeExitSet(selectedTransitions, model, context);
        auto entrySet = computeEntrySet(selectedTransitions, model, context);

        // 4. Execute exit actions
        if (!executeExitActions(exitSet, model, context)) {
            result.errorMessage = "Failed to execute exit actions";
            return result;
        }

        // 5. Execute transition actions
        if (!executeTransitionActions(selectedTransitions, context)) {
            result.errorMessage = "Failed to execute transition actions";
            return result;
        }

        // 6. Update state configuration
        for (const auto &stateId : exitSet) {
            context.deactivateState(stateId);
        }

        for (const auto &stateId : entrySet) {
            context.activateState(stateId);
        }

        // 7. Execute entry actions
        if (!executeEntryActions(entrySet, model, context)) {
            result.errorMessage = "Failed to execute entry actions";
            return result;
        }

        // 7.5. DISABLED: Check for eventless transitions after entry actions (TEMPORARY FIX)
        // This was causing duplicate state processing and incorrect guard evaluations
        // TODO: Implement proper eventless transition handling that doesn't interfere with current event processing
        TransitionResult eventlessResult;  // Declare outside scope for later use
        if (true) {                        // Execute eventless transitions, but with priority logic
            eventlessResult = executeEventlessTransitions(model, context);
            if (eventlessResult.transitionTaken) {
                SCXML::Common::Logger::debug(
                    "TransitionExecutor::executeTransitions - Executed eventless transitions successfully");
                // Update result with eventless transition results
                result.enteredStates.insert(result.enteredStates.end(), eventlessResult.enteredStates.begin(),
                                            eventlessResult.enteredStates.end());
                result.exitedStates.insert(result.exitedStates.end(), eventlessResult.exitedStates.begin(),
                                           eventlessResult.exitedStates.end());
                result.targetStates.insert(result.targetStates.end(), eventlessResult.targetStates.begin(),
                                           eventlessResult.targetStates.end());
            }
        }

        // 8. Set new current state (primary target) - but don't override eventless transitions
        if (!eventlessResult.transitionTaken && !entrySet.empty()) {
            context.setCurrentState(entrySet.back());
        } else if (eventlessResult.transitionTaken && !eventlessResult.enteredStates.empty()) {
            // Use the final state from eventless transition execution
            context.setCurrentState(eventlessResult.enteredStates.back());
            SCXML::Common::Logger::debug(
                "TransitionExecutor::executeTransitions - Using eventless transition target state: " +
                eventlessResult.enteredStates.back());
        }

        // 9. Populate result
        result.transitionTaken = true;
        result.exitedStates = exitSet;
        result.enteredStates = entrySet;

        for (const auto &transition : selectedTransitions) {
            if (!transition.targetState.empty()) {
                result.targetStates.push_back(transition.targetState);
            }
        }

        return result;

    } catch (const std::exception &e) {
        result.errorMessage = "Exception during transition execution: " + std::string(e.what());
        return result;
    }
}

std::vector<TransitionExecutor::ExecutableTransition>
TransitionExecutor::findEnabledTransitions(std::shared_ptr<::Model::DocumentModel> model, Events::EventPtr event,
                                           Runtime::RuntimeContext &context) {
    std::vector<ExecutableTransition> enabledTransitions;

    if (!model) {
        return enabledTransitions;
    }

    // Note: event can be null for eventless transitions

    // Get current active states
    auto activeStates = context.getActiveStates();

    for (const auto &stateId : activeStates) {
        // Get transitions from this state
        auto transitions = getTransitionsFromState(model, stateId);

        for (auto transition : transitions) {
            if (isTransitionEnabled(transition, event, context)) {
                ExecutableTransition execTrans;
                execTrans.sourceState = stateId;
                execTrans.transition = transition;
                execTrans.event = event;
                execTrans.isInternal = isInternalTransition(transition, model);

                // Get target state
                auto targets = transition->getTargets();
                execTrans.targetState = targets.empty() ? "" : targets[0];  // Use first target

                enabledTransitions.push_back(execTrans);
            }
        }
    }

    // Check document-level transitions (transitions at scxml root level)
    const auto &docTransitions = model->getDocumentTransitions();
    SCXML::Common::Logger::debug("TransitionExecutor::findEnabledTransitions - Checking " +
                                 std::to_string(docTransitions.size()) + " document-level transitions");

    for (auto transition : docTransitions) {
        if (isTransitionEnabled(transition, event, context)) {
            ExecutableTransition execTrans;
            execTrans.sourceState = "";  // Document-level transition has no specific source state
            execTrans.transition = transition;
            execTrans.event = event;
            execTrans.isInternal = false;  // Document-level transitions are always external

            // Get target state
            auto targets = transition->getTargets();
            execTrans.targetState = targets.empty() ? "" : targets[0];  // Use first target

            SCXML::Common::Logger::debug(
                "TransitionExecutor::findEnabledTransitions - Found enabled document-level transition to: " +
                execTrans.targetState);
            enabledTransitions.push_back(execTrans);
        }
    }

    return enabledTransitions;
}

bool TransitionExecutor::isTransitionEnabled(std::shared_ptr<Model::ITransitionNode> transition, Events::EventPtr event,
                                             Runtime::RuntimeContext &context) {
    if (!transition) {
        return false;
    }

    // 1. Check event matching
    std::string transitionEvent = transition->getEvent();

    // Handle eventless transitions (no event specified)
    if (transitionEvent.empty()) {
        // This is an eventless transition - only check guard condition
        return evaluateGuardCondition(transition, context);
    }

    // For event-based transitions, event must be provided and must match
    if (!event) {
        return false;
    }

    if (transitionEvent != event->getName()) {
        return false;
    }

    // 2. Evaluate guard condition
    if (!evaluateGuardCondition(transition, context)) {
        return false;
    }

    return true;
}

bool TransitionExecutor::executeSingleTransition(const ExecutableTransition &execTransition,
                                                 Runtime::RuntimeContext &context) {
    (void)context;  // Suppress unused parameter warning

    if (!execTransition.transition) {
        return false;
    }

    try {
        // Execute transition actions (simplified - actions are strings, not executable objects)
        auto actions = execTransition.transition->getActions();
        for (const auto &actionId : actions) {
            if (!actionId.empty()) {
                SCXML::Common::Logger::info("Executing transition action: " + actionId);
                // For now, just log the action execution
            }
        }

        return true;

    } catch (const std::exception &e) {
        addError("Failed to execute transition: " + std::string(e.what()));
        return false;
    }
}

// ========== Protected Methods ==========

std::vector<TransitionExecutor::ExecutableTransition>
TransitionExecutor::selectTransitions(const std::vector<ExecutableTransition> &transitions,
                                      Runtime::RuntimeContext &context) {
    (void)context;  // Suppress unused parameter warning

    // SCXML transition selection algorithm with comprehensive conflict resolution
    std::vector<ExecutableTransition> selectedTransitions;

    if (transitions.empty()) {
        return selectedTransitions;
    }

    // Sort by W3C SCXML priority: deeper states first, then by document order
    auto sortedTransitions = transitions;
    std::sort(sortedTransitions.begin(), sortedTransitions.end(),
              [](const ExecutableTransition &a, const ExecutableTransition &b) {
                  // First by state depth (deeper states have higher priority)
                  int depthA =
                      a.sourceState.empty() ? 0 : std::count(a.sourceState.begin(), a.sourceState.end(), '.') + 1;
                  int depthB =
                      b.sourceState.empty() ? 0 : std::count(b.sourceState.begin(), b.sourceState.end(), '.') + 1;

                  if (depthA != depthB) {
                      return depthA > depthB;  // Higher depth first (W3C SCXML specification)
                  }

                  // Same depth: sort by document order
                  return a.documentOrder < b.documentOrder;
              });

    // W3C SCXML conflict resolution: only one transition per event can be selected
    // For document-level transitions with same event, select only the first one
    std::set<std::string> processedEvents;
    std::set<std::string> conflictingStates;

    for (const auto &trans : sortedTransitions) {
        bool hasConflict = false;

        // Check for event conflicts - only one transition per event in a microstep
        if (trans.event && !trans.event->getName().empty()) {
            const std::string eventName = trans.event->getName();
            if (processedEvents.find(eventName) != processedEvents.end()) {
                continue;  // Skip this transition - event already processed
            }
            processedEvents.insert(eventName);
        }

        // Check for state conflicts based on transition type
        if (!trans.isInternal) {
            // External transition - conflicts with any transition that would exit overlapping states
            std::set<std::string> wouldExit;
            wouldExit.insert(trans.sourceState);

            // Add all ancestors that would be exited
            // Model::IStateNode* sourceNode = nullptr;  // Unused - commented out
            // Check for state hierarchy conflicts using proper ancestor analysis

            for (const auto &state : wouldExit) {
                if (conflictingStates.find(state) != conflictingStates.end()) {
                    hasConflict = true;
                    break;
                }
            }
        } else {
            // Internal transition - only conflicts with transitions from same source state
            if (conflictingStates.find(trans.sourceState) != conflictingStates.end()) {
                hasConflict = true;
            }
        }

        if (!hasConflict) {
            selectedTransitions.push_back(trans);
            conflictingStates.insert(trans.sourceState);
        }
    }

    return selectedTransitions;
}

std::vector<std::string> TransitionExecutor::computeExitSet(const std::vector<ExecutableTransition> &transitions,
                                                            std::shared_ptr<::Model::DocumentModel> model,
                                                            Runtime::RuntimeContext &context) {
    (void)context;  // Suppress unused parameter warning

    std::vector<std::string> exitSet;
    std::set<std::string> toExit;

    for (const auto &transition : transitions) {
        if (!transition.isInternal) {
            // External transition - need to exit source state (but skip empty sourceState for document-level
            // transitions)
            if (!transition.sourceState.empty()) {
                toExit.insert(transition.sourceState);

                // Also exit ancestors up to LCA with target (only if we have a source state)
                if (!transition.targetState.empty()) {
                    std::vector<std::string> states = {transition.sourceState, transition.targetState};
                    std::string lca = getLeastCommonAncestor(states, model);

                    // Exit all ancestors from source to LCA (exclusive)
                    auto sourceAncestors = getProperAncestors(transition.sourceState, model);
                    for (const auto &ancestor : sourceAncestors) {
                        if (ancestor != lca) {
                            toExit.insert(ancestor);
                        } else {
                            break;  // Stop at LCA
                        }
                    }
                }
            } else {
                // Document-level transition: exit current active states (all states need to be exited)
                // For document-level transitions from parallel state, we need to exit all parallel regions
                SCXML::Common::Logger::debug("TransitionExecutor::computeExitSet - Document-level transition detected, "
                                             "exiting all active states");
                auto activeStates = context.getActiveStates();
                for (const auto &activeState : activeStates) {
                    toExit.insert(activeState);
                }
            }
        }
    }

    // Convert set to vector in proper exit order (reverse document order)
    exitSet.assign(toExit.begin(), toExit.end());
    std::reverse(exitSet.begin(), exitSet.end());

    return exitSet;
}

std::vector<std::string> TransitionExecutor::computeEntrySet(const std::vector<ExecutableTransition> &transitions,
                                                             std::shared_ptr<::Model::DocumentModel> model,
                                                             Runtime::RuntimeContext &context) {
    (void)context;  // Suppress unused parameter warning

    std::vector<std::string> entrySet;
    std::set<std::string> toEnter;

    for (const auto &transition : transitions) {
        if (!transition.targetState.empty()) {
            // Need to enter target state
            toEnter.insert(transition.targetState);

            // Also enter ancestors from LCA to target
            if (!transition.sourceState.empty()) {
                std::vector<std::string> states = {transition.sourceState, transition.targetState};
                std::string lca = getLeastCommonAncestor(states, model);

                // Enter target state (simplified approach without path hierarchy)
                toEnter.insert(transition.targetState);
            }
        }
    }

    // Convert set to vector in proper entry order (document order)
    entrySet.assign(toEnter.begin(), toEnter.end());

    return entrySet;
}

bool TransitionExecutor::executeExitActions(const std::vector<std::string> &exitStates,
                                            std::shared_ptr<::Model::DocumentModel> model,
                                            Runtime::RuntimeContext &context) {
    (void)context;  // Suppress unused parameter warning

    for (const auto &stateId : exitStates) {
        Model::IStateNode *stateNode = model->findStateById(stateId);
        if (stateNode) {
            // Execute onexit actions
            auto exitActions = stateNode->getExitActions();
            for (const auto &actionId : exitActions) {
                if (!actionId.empty()) {
                    SCXML::Common::Logger::info("Executing exit action for state " + stateId + ": " + actionId);
                    // For now, just log the action execution - need proper action execution system
                }
            }

            SCXML::Common::Logger::debug("Exiting state: " + stateId);
        }
    }

    return true;
}

bool TransitionExecutor::executeTransitionActions(const std::vector<ExecutableTransition> &transitions,
                                                  Runtime::RuntimeContext &context) {
    for (const auto &transition : transitions) {
        if (transition.transition && transition.event) {
            // Set current event in context for ActionNodes to access
            context.setCurrentEvent(transition.event->getName(), transition.event->getDataAsString());

            // Also set current event in ECMAScript engine to avoid circular calls in DataModelEngine
            auto dataModelEngine = context.getDataModelEngine();
            if (dataModelEngine && dataModelEngine->getDataModelType() == DataModelEngine::DataModelType::ECMASCRIPT) {
                auto ecmaEngine = dataModelEngine->getECMAScriptEngine();
                if (ecmaEngine) {
                    ecmaEngine->setCurrentEvent(transition.event);

                    if (ecmaEngine && ecmaEngine->supportsDataModelSync()) {
                        ecmaEngine->syncDataModelVariables(context);
                    }
                }
            }

            // Get ActionNode objects directly
            auto actionNodes = transition.transition->getActionNodes();

            // Execute each ActionNode directly using their execute() method
            for (const auto &actionNode : actionNodes) {
                if (actionNode) {
                    SCXML::Common::Logger::info("Executing action node: " + actionNode->getId());

                    // Call the ActionNode's execute method directly - this is graceful!

                    bool success = actionNode->execute(context);

                    if (success) {
                        SCXML::Common::Logger::debug("Successfully executed action: " + actionNode->getId());
                    } else {
                        SCXML::Common::Logger::error("Failed to execute action: " + actionNode->getId());
                        return false;
                    }
                }
            }

            SCXML::Common::Logger::debug("Executing transition from " + transition.sourceState + " to " +
                                         transition.targetState);
        }
    }

    return true;
}

bool TransitionExecutor::executeEntryActions(const std::vector<std::string> &entryStates,
                                             std::shared_ptr<::Model::DocumentModel> model,
                                             Runtime::RuntimeContext &context) {
    for (const auto &stateId : entryStates) {
        Model::IStateNode *stateNode = model->findStateById(stateId);
        if (stateNode) {
            // Execute onentry actions using ActionNode objects (graceful approach)
            auto entryActionNodes = stateNode->getEntryActionNodes();

            // Execute each ActionNode directly using their execute() method
            for (const auto &actionNode : entryActionNodes) {
                if (actionNode) {
                    SCXML::Common::Logger::info("Executing entry ActionNode for state " + stateId + ": " +
                                                actionNode->getId());

                    // Call the ActionNode's execute method directly - this matches transition action execution!

                    bool success = actionNode->execute(context);

                    if (success) {
                        SCXML::Common::Logger::debug("Successfully executed entry action: " + actionNode->getId());
                    } else {
                        SCXML::Common::Logger::error("Failed to execute entry action: " + actionNode->getId());
                        return false;
                    }
                }
            }

            SCXML::Common::Logger::debug("Entering state: " + stateId);

            // PARALLEL STATE HANDLING: If this is a parallel state, enter all its regions
            if (stateNode->getType() == SCXML::Type::PARALLEL) {
                SCXML::Common::Logger::debug("Parallel state detected: " + stateId + ", entering all regions");

                // Get all child states (regions) of the parallel state
                auto children = stateNode->getChildren();
                for (auto child : children) {
                    if (child) {
                        std::string regionId = child->getId();
                        SCXML::Common::Logger::debug("Entering parallel region: " + regionId);

                        // Activate the region
                        context.activateState(regionId);

                        // Enter the initial state of the region if it has one
                        std::string initialState = child->getInitialState();
                        if (!initialState.empty()) {
                            SCXML::Common::Logger::debug("Entering initial state of region " + regionId + ": " +
                                                         initialState);
                            context.activateState(initialState);

                            // Execute entry actions for the initial state
                            Model::IStateNode *initialStateNode = model->findStateById(initialState);
                            if (initialStateNode) {
                                auto initialEntryActions = initialStateNode->getEntryActionNodes();
                                for (const auto &actionNode : initialEntryActions) {
                                    if (actionNode) {
                                        SCXML::Common::Logger::info("Executing entry ActionNode for initial state " +
                                                                    initialState + ": " + actionNode->getId());
                                        bool success = actionNode->execute(context);
                                        if (!success) {
                                            SCXML::Common::Logger::error(
                                                "Failed to execute entry action for initial state: " +
                                                actionNode->getId());
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Check if this state is a final state and trigger processor stop
            if (stateNode->isFinalState()) {
                SCXML::Common::Logger::info("Final state reached: " + stateId + ", stopping processor");
                // Signal processor to stop by setting a flag in the context
                context.setFinalStateReached(true);
            }
        }
    }

    return true;
}

bool TransitionExecutor::matchesEventSpec(const std::string &eventSpec, const std::string &eventName) {
    if (eventSpec.empty()) {
        return false;
    }

    // Handle wildcard
    if (eventSpec == "*") {
        return true;
    }

    // Exact match
    if (eventSpec == eventName) {
        return true;
    }

    // Pattern matching with wildcards
    try {
        // Convert SCXML event pattern to regex
        std::string regexPattern = eventSpec;

        // Replace * with .*
        size_t pos = 0;
        while ((pos = regexPattern.find("*", pos)) != std::string::npos) {
            regexPattern.replace(pos, 1, ".*");
            pos += 2;
        }

        std::regex pattern(regexPattern);
        return std::regex_match(eventName, pattern);

    } catch (const std::exception &e) {
        // Invalid regex pattern, fall back to exact match
        return eventSpec == eventName;
    }
}

bool TransitionExecutor::evaluateGuardCondition(std::shared_ptr<Model::ITransitionNode> transition,
                                                Runtime::RuntimeContext &context) {
    if (!transition) {
        return true;  // No transition means condition is true
    }

    // Create guard context from runtime context
    GuardEvaluator::GuardContext guardContext;
    guardContext.runtimeContext = &context;

    // Set source and target states if available
    // guardContext.sourceState = transition->getSourceStateId(); // Method doesn't exist
    // Note: Source state ID retrieval requires interface extension
    auto targets = transition->getTargets();
    guardContext.targetState = targets.empty() ? "" : targets[0];

    // Set current event if available
    // guardContext.currentEvent = context.getCurrentEvent(); // Type mismatch issue
    // Note: Event type compatibility requires namespace harmonization

    SCXML::Common::Logger::debug("TransitionExecutor::evaluateGuardCondition - Evaluating guard for transition: " +
                                 guardContext.sourceState + " -> " + guardContext.targetState);

    // Use external GuardEvaluator if available, otherwise use internal one
    GuardEvaluator *evaluatorToUse = externalGuardEvaluator_ ? externalGuardEvaluator_ : guardEvaluator_.get();
    auto result = evaluatorToUse->evaluateTransitionGuard(transition, guardContext);

    if (!result.satisfied && !result.errorMessage.empty()) {
        addError("Guard condition failed: " + result.errorMessage + " (expression: '" + result.guardExpression + "')");
        SCXML::Common::Logger::warning("TransitionExecutor::evaluateGuardCondition - Guard failed: " +
                                       result.errorMessage);
    }

    SCXML::Common::Logger::debug(std::string("TransitionExecutor::evaluateGuardCondition - Guard result: ") +
                                 (result.satisfied ? "satisfied" : "not satisfied"));

    return result.satisfied;
}

std::vector<std::shared_ptr<Model::ITransitionNode>>
TransitionExecutor::getTransitionsFromState(std::shared_ptr<::Model::DocumentModel> model, const std::string &stateId) {
    std::vector<std::shared_ptr<Model::ITransitionNode>> transitions;

    if (!model) {
        return transitions;
    }

    Model::IStateNode *stateNode = model->findStateById(stateId);
    if (!stateNode) {
        return transitions;
    }

    // Get transitions from this state
    return stateNode->getTransitions();
}

bool TransitionExecutor::isInternalTransition(std::shared_ptr<Model::ITransitionNode> transition,
                                              std::shared_ptr<::Model::DocumentModel> model) {
    (void)model;  // Suppress unused parameter warning
    if (!transition) {
        return false;
    }

    // Check transition type - internal transitions don't exit the source state
    // This depends on the transition's type attribute and target relationships
    // auto transitionType = transition->getTransitionType(); // Method doesn't exist
    // Note: Transition type checking requires getTransitionType() method
    return false;  // Default to external transition
}

std::string TransitionExecutor::getLeastCommonAncestor(const std::vector<std::string> &states,
                                                       std::shared_ptr<::Model::DocumentModel> model) {
    if (states.empty() || !model) {
        return "";
    }

    if (states.size() == 1) {
        return states[0];
    }

    // Get paths from root to each state
    std::vector<std::vector<std::string>> statePaths;
    for (const auto &stateId : states) {
        Model::IStateNode *stateNode = model->findStateById(stateId);
        if (stateNode) {
            // auto path = stateNode->getStatePath(); // Method doesn't exist
            std::vector<std::string> path = {stateId};  // Simplified - just use the state ID
            if (!path.empty()) {
                statePaths.push_back(path);
            }
        }
    }

    if (statePaths.empty()) {
        return "";
    }

    // Find common prefix of all paths
    std::string lca = "";
    size_t minPathLength = statePaths[0].size();
    for (const auto &path : statePaths) {
        minPathLength = std::min(minPathLength, path.size());
    }

    for (size_t i = 0; i < minPathLength; ++i) {
        std::string currentAncestor = statePaths[0][i];
        bool isCommon = true;

        for (size_t j = 1; j < statePaths.size(); ++j) {
            if (statePaths[j][i] != currentAncestor) {
                isCommon = false;
                break;
            }
        }

        if (isCommon) {
            lca = currentAncestor;
        } else {
            break;
        }
    }

    return lca;
}

std::vector<std::string> TransitionExecutor::getProperAncestors(const std::string &stateId,
                                                                std::shared_ptr<::Model::DocumentModel> model) {
    std::vector<std::string> ancestors;

    if (!model || stateId.empty()) {
        return ancestors;
    }

    Model::IStateNode *stateNode = model->findStateById(stateId);
    if (!stateNode) {
        return ancestors;
    }

    // Get full path from root to this state
    // auto statePath = stateNode->getStatePath(); // Method doesn't exist
    std::vector<std::string> statePath = {stateId};  // Simplified - just use the state ID

    // Return all ancestors (excluding the state itself)
    for (size_t i = 0; i < statePath.size() - 1; ++i) {
        ancestors.push_back(statePath[i]);
    }

    return ancestors;
}

bool TransitionExecutor::executeActions(const std::vector<std::shared_ptr<Model::IActionNode>> &actions,
                                        Runtime::RuntimeContext &context) {
    (void)context;  // Suppress unused parameter warning

    for (auto action : actions) {
        if (action) {
            // Note: Action execution requires proper ActionProcessor integration
            SCXML::Common::Logger::debug("Executing transition action");
            // For now, just log that we would execute the action
        }
    }

    return true;
}

TransitionExecutor::TransitionResult
TransitionExecutor::executeEventlessTransitions(std::shared_ptr<::Model::DocumentModel> model,
                                                Runtime::RuntimeContext &context) {
    TransitionResult result;

    if (!model) {
        return result;
    }

    // Find eventless transitions (pass nullptr as event)
    auto eventlessTransitions = findEnabledTransitions(model, nullptr, context);

    if (eventlessTransitions.empty()) {
        return result;
    }

    SCXML::Common::Logger::debug("TransitionExecutor::executeEventlessTransitions - Found " +
                                 std::to_string(eventlessTransitions.size()) + " eventless transitions");

    try {
        // Select optimal transition set
        auto selectedTransitions = selectTransitions(eventlessTransitions, context);

        if (selectedTransitions.empty()) {
            return result;
        }

        // Compute exit and entry sets
        auto exitSet = computeExitSet(selectedTransitions, model, context);
        auto entrySet = computeEntrySet(selectedTransitions, model, context);

        // Execute exit actions
        if (!executeExitActions(exitSet, model, context)) {
            result.errorMessage = "Failed to execute exit actions for eventless transition";
            return result;
        }

        // Execute transition actions (eventless transitions can have actions)
        if (!executeTransitionActions(selectedTransitions, context)) {
            result.errorMessage = "Failed to execute transition actions for eventless transition";
            return result;
        }

        // Update state configuration
        for (const auto &stateId : exitSet) {
            context.deactivateState(stateId);
        }

        for (const auto &stateId : entrySet) {
            context.activateState(stateId);
        }

        // Execute entry actions
        if (!executeEntryActions(entrySet, model, context)) {
            result.errorMessage = "Failed to execute entry actions for eventless transition";
            return result;
        }

        // Set new current state
        if (!entrySet.empty()) {
            context.setCurrentState(entrySet.back());
        }

        // Populate result
        result.transitionTaken = true;
        result.exitedStates = exitSet;
        result.enteredStates = entrySet;

        for (const auto &transition : selectedTransitions) {
            if (!transition.targetState.empty()) {
                result.targetStates.push_back(transition.targetState);
            }
        }

        // No recursion - processUntilStable handles timeout protection

        return result;

    } catch (const std::exception &e) {
        result.errorMessage = "Exception during eventless transition execution: " + std::string(e.what());
        return result;
    }
}

// ========== Private Methods ==========

void TransitionExecutor::addError(const std::string &message) {
    errorMessages_.push_back(message);
}

void TransitionExecutor::clearState() {
    errorMessages_.clear();
}

void TransitionExecutor::setExpressionEvaluator(Runtime::ExpressionEvaluator &evaluator) {
    expressionEvaluator_ = &evaluator;

    // Set the expression evaluator for the guard evaluator being used
    GuardEvaluator *evaluatorToUse = externalGuardEvaluator_ ? externalGuardEvaluator_ : guardEvaluator_.get();
    if (evaluatorToUse) {
        evaluatorToUse->setExpressionEvaluator(
            std::shared_ptr<Runtime::ExpressionEvaluator>(&evaluator, [](Runtime::ExpressionEvaluator *) {}));
    }
}

void TransitionExecutor::setGuardEvaluator(GuardEvaluator &evaluator) {
    // Replace the internal guard evaluator with a reference to the external one
    externalGuardEvaluator_ = &evaluator;
    SCXML::Common::Logger::debug("TransitionExecutor: Guard evaluator set to external evaluator");
}
