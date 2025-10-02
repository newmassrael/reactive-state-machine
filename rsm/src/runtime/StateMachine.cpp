#include "runtime/StateMachine.h"
#include "common/Logger.h"
#include "events/EventRaiserService.h"

#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"
#include "parsing/XIncludeProcessor.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/DeepHistoryFilter.h"
#include "runtime/ExecutionContextImpl.h"
#include "runtime/HistoryManager.h"
#include "runtime/HistoryStateAutoRegistrar.h"
#include "runtime/HistoryValidator.h"
#include "runtime/ShallowHistoryFilter.h"
#include "scripting/JSEngine.h"
#include "states/ConcurrentRegion.h"
#include "states/ConcurrentStateNode.h"
#include <fstream>
#include <random>
#include <regex>
#include <sstream>

namespace RSM {

StateMachine::StateMachine() : isRunning_(false), jsEnvironmentReady_(false) {
    sessionId_ = JSEngine::instance().generateSessionIdString("sm_");
    // JS 환경은 지연 초기화로 변경
    // ActionExecutor와 ExecutionContext는 setupJSEnvironment에서 초기화

    // Initialize History Manager with SOLID architecture (Dependency Injection)
    initializeHistoryManager();

    // Initialize InvokeExecutor with SOLID architecture (W3C SCXML invoke support)
    invokeExecutor_ = std::make_unique<InvokeExecutor>(nullptr);  // EventDispatcher will be set later if needed
}

// Constructor with session ID injection for invoke scenarios
StateMachine::StateMachine(const std::string &sessionId) : isRunning_(false), jsEnvironmentReady_(false) {
    if (sessionId.empty()) {
        throw std::invalid_argument("StateMachine: Session ID cannot be empty when using injection constructor");
    }

    sessionId_ = sessionId;
    LOG_DEBUG("StateMachine: Created with injected session ID: {}", sessionId_);

    // JS environment uses lazy initialization - use existing session
    // ActionExecutor and ExecutionContext are initialized in setupJSEnvironment

    // Initialize History Manager with SOLID architecture (Dependency Injection)
    initializeHistoryManager();

    // Initialize InvokeExecutor with SOLID architecture (W3C SCXML invoke support)
    invokeExecutor_ = std::make_unique<InvokeExecutor>(nullptr);  // EventDispatcher will be set later if needed
}

StateMachine::~StateMachine() {
    // Clear callbacks first to prevent execution during destruction
    completionCallback_ = nullptr;

    if (isRunning_) {
        stop();
    }
    // Clean up session only if JS environment was initialized
    if (jsEnvironmentReady_) {
        RSM::JSEngine::instance().destroySession(sessionId_);
    }
}

bool StateMachine::loadSCXML(const std::string &filename) {
    try {
        auto nodeFactory = std::make_shared<NodeFactory>();
        auto xincludeProcessor = std::make_shared<XIncludeProcessor>();
        SCXMLParser parser(nodeFactory, xincludeProcessor);

        model_ = parser.parseFile(filename);
        if (!model_) {
            LOG_ERROR("Failed to parse SCXML file: {}", filename);
            return false;
        }

        // Register file path for this session to enable relative path resolution
        RSM::JSEngine::instance().registerSessionFilePath(sessionId_, filename);
        LOG_DEBUG("StateMachine: Registered file path '{}' for session '{}'", filename, sessionId_);

        return initializeFromModel();
    } catch (const std::exception &e) {
        LOG_ERROR("Exception loading SCXML: {}", e.what());
        return false;
    }
}

bool StateMachine::loadSCXMLFromString(const std::string &scxmlContent) {
    try {
        auto nodeFactory = std::make_shared<NodeFactory>();
        auto xincludeProcessor = std::make_shared<XIncludeProcessor>();
        SCXMLParser parser(nodeFactory, xincludeProcessor);

        // Use parseContent method which exists in SCXMLParser
        model_ = parser.parseContent(scxmlContent);
        if (!model_) {
            LOG_ERROR("StateMachine: Failed to parse SCXML content");
            return false;
        }

        return initializeFromModel();
    } catch (const std::exception &e) {
        LOG_ERROR("Exception parsing SCXML content: {}", e.what());
        return false;
    }
}

bool StateMachine::start() {
    if (initialState_.empty()) {
        LOG_ERROR("StateMachine: Cannot start - no initial state defined");
        return false;
    }

    // JS 환경 초기화 보장
    if (!ensureJSEnvironment()) {
        LOG_ERROR("StateMachine: Cannot start - JavaScript environment initialization failed");
        return false;
    }

    LOG_DEBUG("Starting with initial state: {}", initialState_);

    // Check EventRaiser status at StateMachine start
    if (eventRaiser_) {
        LOG_DEBUG("StateMachine: EventRaiser status check - EventRaiser: {}, sessionId: {}", (void *)eventRaiser_.get(),
                  sessionId_);
    } else {
        LOG_WARN("StateMachine: EventRaiser is null - sessionId: {}", sessionId_);
    }

    // Set running state before entering initial state to handle immediate done.state events
    isRunning_ = true;

    if (!enterState(initialState_)) {
        LOG_ERROR("Failed to enter initial state: {}", initialState_);
        isRunning_ = false;  // Rollback on failure
        return false;
    }

    // W3C SCXML compliance: Execute deferred invokes after initial state entry completes
    LOG_DEBUG("StateMachine: Executing pending invokes after initial state entry for session: {}", sessionId_);
    executePendingInvokes();
    updateStatistics();

    LOG_INFO("StateMachine: Started successfully");
    return true;
}

void StateMachine::stop() {
    if (!isRunning_) {
        return;
    }

    LOG_DEBUG("StateMachine: Stopping state machine");

    // Use StateHierarchyManager to check current state
    std::string currentState = getCurrentState();
    if (!currentState.empty()) {
        exitState(currentState);
    }

    isRunning_ = false;
    // State management delegated to StateHierarchyManager
    if (hierarchyManager_) {
        hierarchyManager_->reset();
    }

    // Unregister from JSEngine
    RSM::JSEngine::instance().setStateMachine(nullptr, sessionId_);
    LOG_DEBUG("StateMachine: Unregistered from JSEngine");

    updateStatistics();
    LOG_INFO("StateMachine: Stopped");
}

StateMachine::TransitionResult StateMachine::processEvent(const std::string &eventName, const std::string &eventData) {
    // W3C SCXML 6.4: Check if there's an origin session ID from EventRaiser thread-local storage
    std::string originSessionId = EventRaiserImpl::getCurrentOriginSessionId();

    // Delegate to overload with originSessionId (may be empty for non-invoke events)
    return processEvent(eventName, eventData, originSessionId);
}

StateMachine::TransitionResult StateMachine::processEvent(const std::string &eventName, const std::string &eventData,
                                                          const std::string &originSessionId) {
    if (!isRunning_) {
        LOG_WARN("StateMachine: Cannot process event - state machine not running");
        TransitionResult result;
        result.success = false;
        result.errorMessage = "State machine not running";
        return result;
    }

    // JS 환경 확인
    if (!jsEnvironmentReady_) {
        LOG_ERROR("StateMachine: Cannot process event - JavaScript environment not ready");
        TransitionResult result;
        result.success = false;
        result.errorMessage = "JavaScript environment not ready";
        return result;
    }

    LOG_DEBUG("StateMachine: Processing event: '{}' with data: '{}' in session: '{}', originSessionId: '{}'", eventName,
              eventData, sessionId_, originSessionId);

    // Set event processing flag with RAII for exception safety
    struct ProcessingEventGuard {
        bool &flag_;

        explicit ProcessingEventGuard(bool &flag) : flag_(flag) {
            LOG_DEBUG("ProcessingEventGuard: Setting isProcessingEvent_ = true");
            flag_ = true;
        }

        ~ProcessingEventGuard() {
            LOG_DEBUG("ProcessingEventGuard: Setting isProcessingEvent_ = false");
            flag_ = false;
        }

        // Delete copy constructor and assignment
        ProcessingEventGuard(const ProcessingEventGuard &) = delete;
        ProcessingEventGuard &operator=(const ProcessingEventGuard &) = delete;
    };

    ProcessingEventGuard eventGuard(isProcessingEvent_);

    // Count this event
    stats_.totalEvents++;

    // Store event data for access in guards/actions
    currentEventData_ = eventData;

    // W3C SCXML compliance: Set current event in ActionExecutor for _event context
    if (actionExecutor_) {
        auto actionExecutorImpl = std::dynamic_pointer_cast<ActionExecutorImpl>(actionExecutor_);
        if (actionExecutorImpl) {
            actionExecutorImpl->setCurrentEvent(eventName, eventData);
            LOG_DEBUG("StateMachine: Set current event in ActionExecutor - event: '{}', data: '{}'", eventName,
                      eventData);
        }
    }

    // W3C SCXML Test 252: Filter events from cancelled invoke child sessions
    if (invokeExecutor_ && !originSessionId.empty()) {
        if (invokeExecutor_->shouldFilterCancelledInvokeEvent(originSessionId)) {
            LOG_DEBUG("StateMachine: Filtering event '{}' from cancelled invoke child session: {}", eventName,
                      originSessionId);
            return TransitionResult(false, getCurrentState(), getCurrentState(), eventName);
        }
    }

    // W3C SCXML 1.0 Section 6.4: Execute finalize handler before processing events from invoked children
    // According to W3C SCXML: "finalize markup runs BEFORE the event is processed"
    // The finalize handler is executed when an event arrives from an invoked child
    // and has access to _event.data to update parent variables before transition evaluation
    if (invokeExecutor_ && !originSessionId.empty()) {
        // W3C SCXML compliance: Use originSessionId to find the exact child that sent this event
        std::string finalizeScript = invokeExecutor_->getFinalizeScriptForChildSession(originSessionId);

        if (!finalizeScript.empty()) {
            LOG_DEBUG("StateMachine: Executing finalize handler BEFORE processing event '{}', script: '{}'", eventName,
                      finalizeScript);

            // W3C SCXML 6.4: Parse and execute finalize as SCXML executable content
            // Finalize contains elements like <assign>, <script>, <log>, etc.
            if (actionExecutor_) {
                try {
                    // Parse finalize SCXML content to extract assign actions
                    // Simple pattern: <assign location="var1" expr="_event.data.aParam"/>
                    std::regex assign_pattern("<assign location=\"([^\"]+)\" expr=\"([^\"]+)\"/>");
                    std::smatch match;
                    std::string content = finalizeScript;

                    while (std::regex_search(content, match, assign_pattern)) {
                        std::string location = match[1].str();
                        std::string expr = match[2].str();

                        LOG_DEBUG("StateMachine: Finalize assign - location: '{}', expr: '{}'", location, expr);

                        // Execute assignment: evaluate expression and assign to variable
                        auto exprFuture = JSEngine::instance().evaluateExpression(sessionId_, expr);
                        auto exprResult = exprFuture.get();

                        if (exprResult.isSuccess()) {
                            // Get the actual value from JSResult
                            const ScriptValue &value = exprResult.getInternalValue();
                            JSEngine::instance().setVariable(sessionId_, location, value);
                            LOG_DEBUG("StateMachine: Finalize assigned '{}' successfully", location);
                        } else {
                            LOG_WARN("StateMachine: Finalize expr evaluation failed: {}", exprResult.getErrorMessage());
                        }

                        content = match.suffix();
                    }

                    LOG_DEBUG("StateMachine: Finalize handler executed successfully for event '{}'", eventName);
                } catch (const std::exception &e) {
                    LOG_ERROR("StateMachine: Exception during finalize handler execution: {}", e.what());
                }
            } else {
                LOG_WARN("StateMachine: No ActionExecutor available for finalize execution");
            }
        }
    }

    // W3C SCXML 1.0 Section 6.4: Auto-forward external events to child invoke sessions
    if (invokeExecutor_) {
        auto autoForwardSessions = invokeExecutor_->getAutoForwardSessions(sessionId_);
        for (auto *childStateMachine : autoForwardSessions) {
            if (childStateMachine->isRunning()) {
                childStateMachine->processEvent(eventName, eventData);
            }
        }
    }

    // Find applicable transitions from SCXML model
    if (!model_) {
        LOG_ERROR("StateMachine: No SCXML model available");
        TransitionResult result;
        result.success = false;
        result.fromState = getCurrentState();
        result.eventName = eventName;
        result.errorMessage = "No SCXML model available";
        return result;
    }

    // SCXML W3C specification section 3.4: Handle parallel state event broadcasting
    std::string currentState = getCurrentState();
    auto currentStateNode = model_->findStateById(currentState);
    if (!currentStateNode) {
        LOG_DEBUG("Current state not found in model: {}", currentState);
        TransitionResult result;
        result.success = false;
        result.fromState = getCurrentState();
        result.eventName = eventName;
        result.errorMessage = "Current state not found in model";
        return result;
    }

    // SCXML W3C specification compliance: Process parallel state events according to standard priority
    if (currentStateNode->getType() == Type::PARALLEL) {
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(currentStateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        LOG_DEBUG("Processing event '{}' for parallel state: {}", eventName, currentState);

        // SCXML W3C specification: First check transitions on the parallel state itself
        // This follows the same transition evaluation logic as non-parallel states
        auto stateTransitionResult = processStateTransitions(currentStateNode, eventName, eventData);
        if (stateTransitionResult.success) {
            LOG_DEBUG("SCXML compliant - parallel state transition executed: {} -> {}", stateTransitionResult.fromState,
                      stateTransitionResult.toState);
            return stateTransitionResult;
        }

        // SCXML W3C specification 3.4: Check transitions on region root states
        // Region root states (direct children of parallel) can have transitions that exit the parallel state
        const auto &parallelChildren = parallelState->getChildren();
        for (const auto &child : parallelChildren) {
            if (child) {
                // Check if this child state has a transition for this event
                auto childTransitionResult = processStateTransitions(child.get(), eventName, eventData);
                if (childTransitionResult.success) {
                    LOG_DEBUG(
                        "SCXML W3C: Region root state '{}' has transition for event '{}' -> exiting parallel state",
                        child->getId(), eventName);
                    return childTransitionResult;
                }
            }
        }

        // SCXML W3C specification 3.4: Process event in ALL regions independently
        // Events must be broadcast to all active regions simultaneously
        const auto &regions = parallelState->getRegions();
        bool anyRegionTransitioned = false;
        std::vector<std::string> transitionedRegions;

        // Create EventDescriptor for region processing
        EventDescriptor regionEvent;
        regionEvent.eventName = eventName;
        regionEvent.data = eventData;

        for (const auto &region : regions) {
            if (region && region->isActive()) {
                // W3C SCXML 3.4: Each region processes events independently
                // Use region's own processEvent method which manages its internal state
                auto regionResult = region->processEvent(regionEvent);
                if (regionResult.isSuccess) {
                    anyRegionTransitioned = true;
                    transitionedRegions.push_back(region->getId());
                    LOG_DEBUG("SCXML W3C: Region '{}' successfully processed event '{}'", region->getId(), eventName);
                }
            }
        }

        // W3C SCXML 3.4: If any region transitioned, check completion and return success
        if (anyRegionTransitioned) {
            LOG_INFO("SCXML W3C: {} regions processed transitions for event '{}'", transitionedRegions.size(),
                     eventName);

            // W3C SCXML 3.4: Check if all regions completed (reached final states)
            bool allRegionsComplete = parallelState->areAllRegionsComplete();
            if (allRegionsComplete) {
                LOG_DEBUG("SCXML W3C: All parallel regions completed after transitions");
            }

            TransitionResult result;
            result.success = true;
            result.fromState = currentState;
            result.toState = currentState;  // Parallel state remains active
            result.eventName = eventName;
            return result;
        }

        // SCXML W3C specification: If no transition on parallel state or regions, broadcast to all active regions
        LOG_DEBUG("StateMachine: No transitions on parallel state or region children, broadcasting to all regions");

        // Create EventDescriptor for SCXML-compliant event processing
        EventDescriptor event;
        event.eventName = eventName;
        event.data = eventData;

        // Broadcast event to all active regions (SCXML W3C mandated)
        auto results = parallelState->processEventInAllRegions(event);

        bool anyTransitionExecuted = false;
        std::vector<std::string> successfulTransitions;

        for (const auto &result : results) {
            if (result.isSuccess) {
                anyTransitionExecuted = true;
                successfulTransitions.push_back(result.regionId + ": SUCCESS");
            }
        }

        if (anyTransitionExecuted) {
            stats_.totalTransitions++;
            LOG_INFO("SCXML compliant parallel region processing succeeded. Transitions: [{}/{}]",
                     successfulTransitions.size(), results.size());

            // W3C SCXML 3.4: Check if all regions completed (reached final states)
            // This triggers done.state.{id} event generation
            bool allRegionsComplete = parallelState->areAllRegionsComplete();
            if (allRegionsComplete) {
                LOG_DEBUG("SCXML W3C: All parallel regions completed for state: {}", currentState);
            }

            // Invoke execution consolidated to key lifecycle points            // Return success with parallel state as
            // context
            TransitionResult finalResult;
            finalResult.success = true;
            finalResult.fromState = currentState;
            finalResult.toState = currentState;  // Parallel state remains active
            finalResult.eventName = eventName;
            return finalResult;
        } else {
            LOG_DEBUG("No transitions executed in any region for event: {}", eventName);
            stats_.failedTransitions++;
            TransitionResult result;
            result.success = false;
            result.fromState = getCurrentState();
            result.eventName = eventName;
            result.errorMessage = "No valid transitions found";
            return result;
        }
    }

    // Non-parallel state: SCXML W3C compliant hierarchical event processing
    // Process transitions in active state hierarchy (innermost to outermost)
    auto activeStates = hierarchyManager_->getActiveStates();

    LOG_DEBUG("SCXML hierarchical processing: Checking {} active states for event '{}'", activeStates.size(),
              eventName);

    // Process states from most specific (innermost) to least specific (outermost)
    for (auto it = activeStates.rbegin(); it != activeStates.rend(); ++it) {
        const std::string &stateId = *it;
        auto stateNode = model_->findStateById(stateId);
        if (stateNode) {
            LOG_DEBUG("SCXML hierarchical processing: Checking state '{}' for transitions", stateId);
            auto transitionResult = processStateTransitions(stateNode, eventName, eventData);
            if (transitionResult.success) {
                LOG_DEBUG("SCXML hierarchical processing: Transition found in state '{}': {} -> {}", stateId,
                          transitionResult.fromState, transitionResult.toState);

                // Invoke execution consolidated to key lifecycle points
                return transitionResult;
            }
        } else {
            LOG_WARN("SCXML hierarchical processing: State node not found: {}", stateId);
        }
    }

    // No transitions found in any active state
    LOG_DEBUG("SCXML hierarchical processing: No transitions found in any active state for event '{}'", eventName);
    stats_.failedTransitions++;

    TransitionResult result;
    result.success = false;
    result.fromState = getCurrentState();
    result.eventName = eventName;
    result.errorMessage = "No valid transitions found in active state hierarchy";
    return result;
}

StateMachine::TransitionResult StateMachine::processStateTransitions(IStateNode *stateNode,
                                                                     const std::string &eventName,
                                                                     const std::string &eventData) {
    // eventData available for future SCXML features (e.g., event.data access in guards/actions)
    (void)eventData;

    if (!stateNode) {
        TransitionResult result;
        result.success = false;
        result.fromState = getCurrentState();
        result.eventName = eventName;
        result.errorMessage = "Invalid state node";
        return result;
    }

    // SCXML W3C specification: Process transitions in document order
    const auto &transitions = stateNode->getTransitions();

    LOG_DEBUG("Checking {} transitions for event '{}' on state: {}", transitions.size(), eventName, stateNode->getId());

    // Execute first valid transition (SCXML W3C specification)
    for (const auto &transitionNode : transitions) {
        std::string transitionEvent = transitionNode->getEvent();

        // Check if this transition matches the event
        // Allow eventless transitions when eventName is empty (automatic transitions)
        bool eventMatches = false;
        if (eventName.empty()) {
            // For automatic transitions, only consider transitions without events
            eventMatches = transitionEvent.empty();
        } else {
            // W3C SCXML: Support wildcard (*), exact matching, and hierarchical token matching
            // Hierarchical matching: "done.invoke" matches "done.invoke.foo"
            if (transitionEvent == "*") {
                eventMatches = true;
            } else if (transitionEvent == eventName) {
                eventMatches = true;
            } else {
                // Check hierarchical prefix matching (tokenized by '.')
                // "done.invoke" should match "done.invoke.foo"
                eventMatches = eventName.starts_with(transitionEvent + ".");
            }
        }

        if (!eventMatches) {
            continue;
        }

        const auto &targets = transitionNode->getTargets();

        // W3C SCXML: Internal transitions have no targets but should still execute
        bool isInternal = transitionNode->isInternal();
        if (targets.empty() && !isInternal) {
            LOG_DEBUG("StateMachine: Skipping transition with no targets (not internal)");
            continue;
        }

        std::string targetState = targets.empty() ? "" : targets[0];
        std::string condition = transitionNode->getGuard();

        LOG_DEBUG("Checking transition: {} -> {} with condition: '{}' (event: '{}')", stateNode->getId(), targetState,
                  condition, transitionEvent);

        bool conditionResult = condition.empty() || evaluateCondition(condition);
        LOG_DEBUG("Condition result: {}", conditionResult ? "true" : "false");

        if (conditionResult) {
            std::string fromState = getCurrentState();  // Save the original state

            // W3C SCXML: Internal transitions execute actions without exiting/entering states
            if (isInternal) {
                LOG_DEBUG("StateMachine: Executing internal transition actions (no state change)");
                const auto &actionNodes = transitionNode->getActionNodes();
                if (!actionNodes.empty()) {
                    executeActionNodes(actionNodes, false);
                }

                TransitionResult result;
                result.success = true;
                result.fromState = fromState;
                result.toState = fromState;  // Same state (internal transition)
                result.eventName = eventName;
                return result;
            }

            LOG_DEBUG("Executing SCXML compliant transition from {} to {}", fromState, targetState);

            // Exit current state
            if (!exitState(fromState)) {
                LOG_ERROR("Failed to exit state: {}", fromState);
                TransitionResult result;
                result.success = false;
                result.fromState = fromState;
                result.eventName = eventName;
                result.errorMessage = "Failed to exit state: " + fromState;
                return result;
            }

            // Execute transition actions (SCXML W3C specification)
            // W3C compliance: Events raised in transition actions must be queued, not processed immediately
            const auto &actionNodes = transitionNode->getActionNodes();
            if (!actionNodes.empty()) {
                LOG_DEBUG("StateMachine: Executing transition actions (events will be queued)");
                // processEventsAfter=false: Don't process events yet, they will be handled in macrostep loop
                executeActionNodes(actionNodes, false);
            } else {
                LOG_DEBUG("StateMachine: No transition actions for this transition");
            }

            // Enter new state
            if (!enterState(targetState)) {
                LOG_ERROR("Failed to enter state: {}", targetState);
                TransitionResult result;
                result.success = false;
                result.fromState = fromState;
                result.toState = targetState;
                result.eventName = eventName;
                result.errorMessage = "Failed to enter state: " + targetState;
                return result;
            }

            updateStatistics();
            stats_.totalTransitions++;

            LOG_INFO("Successfully transitioned from {} to {}", fromState, targetState);

            // W3C SCXML compliance: Macrostep loop - process internal events one at a time
            // After a transition completes, we must:
            // 1. Check for eventless transitions on all active states
            // 2. If none, dequeue ONE internal event and process it
            // 3. Repeat until no eventless transitions and no internal events
            if (eventRaiser_) {
                auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
                if (eventRaiserImpl) {
                    LOG_DEBUG("W3C SCXML: Starting macrostep loop after transition");

                    // W3C SCXML: Safety guard against infinite loops in malformed SCXML
                    // Typical SCXML should complete in far fewer iterations
                    const int MAX_MACROSTEP_ITERATIONS = 1000;
                    int iterations = 0;

                    while (true) {
                        if (++iterations > MAX_MACROSTEP_ITERATIONS) {
                            LOG_ERROR(
                                "W3C SCXML: Macrostep limit exceeded ({} iterations) - possible infinite loop in SCXML",
                                MAX_MACROSTEP_ITERATIONS);
                            LOG_ERROR("W3C SCXML: Check for circular eventless transitions in your SCXML document");
                            break;  // Safety exit
                        }
                        // Step 1: Check for eventless transitions on all active states
                        bool eventlessTransitionExecuted = checkEventlessTransitions();

                        if (eventlessTransitionExecuted) {
                            LOG_DEBUG("W3C SCXML: Eventless transition executed, continuing macrostep");
                            continue;  // Loop back to check for more eventless transitions
                        }

                        // Step 2: No eventless transitions, check for internal events
                        if (!eventRaiserImpl->hasQueuedEvents()) {
                            LOG_DEBUG("W3C SCXML: No eventless transitions and no queued events, macrostep complete");
                            break;  // Macrostep complete
                        }

                        // Step 3: Process ONE internal event
                        LOG_DEBUG("W3C SCXML: Processing next internal event in macrostep");
                        bool eventProcessed = eventRaiserImpl->processNextQueuedEvent();

                        if (!eventProcessed) {
                            LOG_DEBUG("W3C SCXML: No event processed, macrostep complete");
                            break;  // No more events
                        }

                        // Loop continues - check for eventless transitions again
                    }

                    LOG_DEBUG("W3C SCXML: Macrostep loop complete");
                }
            }

            // W3C SCXML compliance: Execute deferred invokes after macrostep completes
            executePendingInvokes();

            return TransitionResult(true, fromState, targetState, eventName);
        }
    }

    // No valid transitions found
    LOG_DEBUG("No valid transitions found for event: {} from state: {}", eventName, stateNode->getId());

    // Note: Failed transition counter is managed at processEvent() level to avoid double counting

    TransitionResult result;
    result.success = false;
    result.fromState = getCurrentState();
    result.eventName = eventName;
    result.errorMessage = "No valid transitions found";
    return result;
}

std::string StateMachine::getCurrentState() const {
    // 계층 관리자는 SCXML 표준에 필수
    if (!hierarchyManager_) {
        assert(false && "StateHierarchyManager is required for SCXML compliance");
        return "";
    }

    return hierarchyManager_->getCurrentState();
}

std::vector<std::string> StateMachine::getActiveStates() const {
    // 계층 관리자는 SCXML 표준에 필수
    if (!hierarchyManager_) {
        assert(false && "StateHierarchyManager is required for SCXML compliance");
        return {};
    }

    return hierarchyManager_->getActiveStates();
}

bool StateMachine::isRunning() const {
    return isRunning_;
}

bool StateMachine::isStateActive(const std::string &stateId) const {
    if (!hierarchyManager_) {
        return false;
    }
    return hierarchyManager_->isStateActive(stateId);
}

bool StateMachine::isStateInFinalState(const std::string &stateId) const {
    if (!model_) {
        LOG_DEBUG("StateMachine::isStateInFinalState: No model available");
        return false;
    }

    if (stateId.empty()) {
        LOG_DEBUG("StateMachine::isStateInFinalState: State ID is empty");
        return false;
    }

    auto state = model_->findStateById(stateId);
    bool isFinal = state && state->isFinalState();
    LOG_DEBUG("StateMachine::isStateInFinalState: stateId='{}', state found: {}, isFinalState: {}", stateId,
              (void *)state, isFinal);
    return isFinal;
}

bool StateMachine::isInFinalState() const {
    if (!isRunning_) {
        LOG_DEBUG("StateMachine::isInFinalState: State machine is not running");
        return false;
    }

    return isStateInFinalState(getCurrentState());
}

bool StateMachine::isInitialStateFinal() const {
    return isStateInFinalState(model_ ? model_->getInitialState() : "");
}

std::string StateMachine::getCurrentEventData() const {
    return currentEventData_;
}

const std::string &StateMachine::getSessionId() const {
    return sessionId_;
}

std::shared_ptr<SCXMLModel> StateMachine::getModel() const {
    return model_;
}

StateMachine::Statistics StateMachine::getStatistics() const {
    return stats_;
}

// W3C SCXML 5.3: Collect all data items from document for global scope initialization
std::vector<StateMachine::DataItemInfo> StateMachine::collectAllDataItems() const {
    std::vector<DataItemInfo> allDataItems;

    if (!model_) {
        return allDataItems;
    }

    // Collect top-level datamodel items
    const auto &topLevelItems = model_->getDataModelItems();
    for (const auto &item : topLevelItems) {
        allDataItems.push_back(DataItemInfo{"", item});  // Empty stateId for top-level
    }
    LOG_DEBUG("StateMachine: Collected {} top-level data items", topLevelItems.size());

    // Collect state-level data items from all states
    const auto &allStates = model_->getAllStates();
    for (const auto &state : allStates) {
        if (!state) {
            continue;
        }

        const auto &stateDataItems = state->getDataItems();
        if (!stateDataItems.empty()) {
            for (const auto &item : stateDataItems) {
                allDataItems.push_back(DataItemInfo{state->getId(), item});
            }
            LOG_DEBUG("StateMachine: Collected {} data items from state '{}'", stateDataItems.size(), state->getId());
        }
    }

    LOG_INFO("StateMachine: Total data items collected: {} (for global scope initialization)", allDataItems.size());
    return allDataItems;
}

// W3C SCXML 5.3: Initialize a single data item with binding mode support
void StateMachine::initializeDataItem(const std::shared_ptr<IDataModelItem> &item, bool assignValue) {
    if (!item) {
        return;
    }

    std::string id = item->getId();
    std::string expr = item->getExpr();
    std::string content = item->getContent();

    // W3C SCXML 6.4: Check if variable was pre-initialized (e.g., by invoke namelist/param)
    if (RSM::JSEngine::instance().isVariablePreInitialized(sessionId_, id)) {
        LOG_INFO("StateMachine: Skipping initialization for '{}' - pre-initialized by invoke data", id);
        return;
    }

    if (!assignValue) {
        // Late binding: Create variable but don't assign value yet (leave undefined)
        RSM::JSEngine::instance().setVariable(sessionId_, id, ScriptValue{});
        LOG_DEBUG("StateMachine: Created unbound variable '{}' for late binding", id);
        return;
    }

    // Early binding or late binding value assignment: Evaluate and assign
    if (!expr.empty()) {
        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, expr);
        auto result = future.get();

        if (RSM::JSEngine::isSuccess(result)) {
            RSM::JSEngine::instance().setVariable(sessionId_, id, result.getInternalValue());
            LOG_DEBUG("StateMachine: Initialized variable '{}' from expression '{}'", id, expr);
        } else {
            LOG_ERROR("StateMachine: Failed to evaluate expression '{}' for variable '{}': {}", expr, id,
                      result.getErrorMessage());
            // W3C SCXML 5.3: On evaluation error, raise error.execution event and create unbound variable
            if (eventRaiser_) {
                eventRaiser_->raiseEvent("error.execution", "Failed to evaluate data expression for '" + id +
                                                                "': " + result.getErrorMessage());
            }
            // Leave variable unbound (don't create it) so it can be assigned later
            return;
        }
    } else if (!content.empty()) {
        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, content);
        auto result = future.get();

        if (RSM::JSEngine::isSuccess(result)) {
            RSM::JSEngine::instance().setVariable(sessionId_, id, result.getInternalValue());
            LOG_DEBUG("StateMachine: Initialized variable '{}' from content", id);
        } else {
            // Try setting content as string literal
            RSM::JSEngine::instance().setVariable(sessionId_, id, ScriptValue{content});
            LOG_DEBUG("StateMachine: Set variable '{}' as string literal from content", id);
        }
    } else {
        // No expression or content: skip initialization (old behavior for backward compatibility)
        // The variable will be created on first assignment via <assign> action
        LOG_DEBUG("StateMachine: No expression or content for variable '{}', skipping initialization", id);
    }
}

bool StateMachine::initializeFromModel() {
    LOG_DEBUG("StateMachine: Initializing from SCXML model");

    // Clear existing state
    initialState_.clear();

    // Get initial state
    initialState_ = model_->getInitialState();
    if (initialState_.empty()) {
        LOG_ERROR("StateMachine: No initial state defined in SCXML model");
        return false;
    }

    // Extract all states from the model
    const auto &allStates = model_->getAllStates();
    if (allStates.empty()) {
        LOG_ERROR("StateMachine: No states found in SCXML model");
        return false;
    }

    try {
        // Initialize hierarchy manager for hierarchical state support
        hierarchyManager_ = std::make_unique<StateHierarchyManager>(model_);

        // Set up onentry callback for W3C SCXML compliance
        LOG_DEBUG("StateMachine: Setting up onentry callback for StateHierarchyManager");
        hierarchyManager_->setOnEntryCallback([this](const std::string &stateId) {
            LOG_DEBUG("StateMachine: Onentry callback triggered for state: {}", stateId);
            executeOnEntryActions(stateId);
        });
        LOG_DEBUG("StateMachine: Onentry callback successfully configured");

        // W3C SCXML 6.4: Set up invoke defer callback for proper timing in parallel states
        LOG_DEBUG("StateMachine: Setting up invoke defer callback for StateHierarchyManager");
        hierarchyManager_->setInvokeDeferCallback(
            [this](const std::string &stateId, const std::vector<std::shared_ptr<IInvokeNode>> &invokes) {
                LOG_DEBUG("StateMachine: Invoke defer callback triggered for state: {} with {} invokes", stateId,
                          invokes.size());
                deferInvokeExecution(stateId, invokes);
            });
        LOG_DEBUG("StateMachine: Invoke defer callback successfully configured");

        // W3C SCXML: Set up condition evaluator callback for transition guard evaluation in parallel states
        LOG_DEBUG("StateMachine: Setting up condition evaluator callback for StateHierarchyManager");
        hierarchyManager_->setConditionEvaluator(
            [this](const std::string &condition) -> bool { return evaluateCondition(condition); });
        LOG_DEBUG("StateMachine: Condition evaluator callback successfully configured");

        // Set up completion callbacks for parallel states (SCXML W3C compliance)
        setupParallelStateCallbacks();

        // SCXML W3C Section 3.6: Auto-register history states from parsed model (SOLID architecture)
        initializeHistoryAutoRegistrar();
        if (historyAutoRegistrar_) {
            historyAutoRegistrar_->autoRegisterHistoryStates(model_, historyManager_.get());
        }

        LOG_DEBUG("Model initialized with initial state: {}", initialState_);
        LOG_INFO("Model initialized with {} states", allStates.size());
        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to extract model: {}", e.what());
        return false;
    }
}

bool StateMachine::evaluateCondition(const std::string &condition) {
    if (condition.empty()) {
        LOG_DEBUG("Empty condition, returning true");
        return true;
    }

    try {
        LOG_DEBUG("Evaluating condition: '{}'", condition);

        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, condition);
        auto result = future.get();

        if (!RSM::JSEngine::isSuccess(result)) {
            LOG_ERROR("StateMachine: Failed to evaluate condition '{}': evaluation failed", condition);
            return false;
        }

        // Convert result to boolean using integrated JSEngine method
        bool conditionResult = RSM::JSEngine::resultToBool(result);
        LOG_DEBUG("Condition '{}' evaluated to: {}", condition, conditionResult ? "true" : "false");

        return conditionResult;

    } catch (const std::exception &e) {
        LOG_ERROR("Exception evaluating condition '{}': {}", condition, e.what());
        return false;
    }
}

bool StateMachine::enterState(const std::string &stateId) {
    LOG_DEBUG("Entering state: {}", stateId);

    // Guard against invalid reentrant calls, but allow legitimate done.state event processing
    if (isEnteringState_ && !isProcessingEvent_) {
        LOG_DEBUG(
            "Invalid reentrant enterState call detected (isEnteringState_={}, isProcessingEvent_={}), ignoring: {}",
            isEnteringState_, isProcessingEvent_, stateId);
        return true;  // Return success to avoid breaking the transition chain
    }

    // Allow reentrant calls during event processing (done.state events are legitimate)
    if (isEnteringState_ && isProcessingEvent_) {
        LOG_DEBUG("Legitimate reentrant enterState call during event processing (isEnteringState_={}, "
                  "isProcessingEvent_={}): {}",
                  isEnteringState_, isProcessingEvent_, stateId);
    }

    // Set guard flag
    isEnteringState_ = true;

    // Check if this is a history state and handle restoration (SCXML W3C specification section 3.6)
    if (historyManager_ && historyManager_->isHistoryState(stateId)) {
        LOG_INFO("Entering history state: {}", stateId);

        auto restorationResult = historyManager_->restoreHistory(stateId);
        if (restorationResult.success && !restorationResult.targetStateIds.empty()) {
            LOG_INFO("History restoration successful, entering {} target states",
                     restorationResult.targetStateIds.size());

            // Enter all target states from history restoration
            bool allSucceeded = true;
            for (const auto &targetStateId : restorationResult.targetStateIds) {
                if (!enterState(targetStateId)) {
                    LOG_ERROR("Failed to enter restored target state: {}", targetStateId);
                    allSucceeded = false;
                }
            }
            // Clear guard flag before returning
            isEnteringState_ = false;
            return allSucceeded;
        } else {
            LOG_ERROR("History restoration failed: {}", restorationResult.errorMessage);
            // Clear guard flag before returning
            isEnteringState_ = false;
            return false;
        }
    }

    // SCXML W3C specification: hierarchy manager is required for compliant state entry
    assert(hierarchyManager_ && "SCXML violation: hierarchy manager required for state management");

    // W3C SCXML 5.3: Late binding - assign values to state's data items when state is entered
    if (model_) {
        const std::string &binding = model_->getBinding();
        bool isLateBinding = (binding == "late");

        if (isLateBinding && initializedStates_.find(stateId) == initializedStates_.end()) {
            // Late binding: Assign values to this state's data items now (on first entry)
            auto stateNode = model_->findStateById(stateId);
            if (stateNode) {
                const auto &stateDataItems = stateNode->getDataItems();
                if (!stateDataItems.empty()) {
                    LOG_DEBUG("StateMachine: Late binding - assigning values to {} data items for state '{}'",
                              stateDataItems.size(), stateId);
                    for (const auto &item : stateDataItems) {
                        initializeDataItem(item, true);  // assignValue=true
                    }
                    initializedStates_.insert(stateId);  // Mark state as initialized
                }
            }
        }
    }

    bool hierarchyResult = hierarchyManager_->enterState(stateId);
    assert(hierarchyResult && "SCXML violation: state entry must succeed");
    (void)hierarchyResult;  // Suppress unused variable warning in release builds

    // SCXML W3C 3.4: For parallel states, activate regions AFTER parent onentry executed
    // This ensures correct entry sequence: parallel onentry -> child onentry
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && stateNode->getType() == Type::PARALLEL) {
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
            if (parallelState) {
                // Set ExecutionContext for region action execution
                if (executionContext_) {
                    parallelState->setExecutionContextForRegions(executionContext_);
                    LOG_DEBUG("SCXML compliant: Injected ExecutionContext into parallel state regions: {}", stateId);
                }

                // W3C SCXML 3.4: Activate all regions AFTER parallel state entered
                auto activationResults = parallelState->activateAllRegions();
                for (const auto &result : activationResults) {
                    if (!result.isSuccess) {
                        LOG_ERROR("Failed to activate region '{}': {}", result.regionId, result.errorMessage);
                    } else {
                        LOG_DEBUG("SCXML W3C: Activated region '{}' in parallel state '{}'", result.regionId, stateId);
                    }
                }

                // Check if all regions immediately reached final state (for done.state event)
                const auto &regions = parallelState->getRegions();
                bool allInFinalState =
                    !regions.empty() && std::all_of(regions.begin(), regions.end(), [](const auto &region) {
                        return region && region->isInFinalState();
                    });

                if (allInFinalState) {
                    LOG_DEBUG("SCXML W3C 3.4: All parallel regions in final state, triggering done.state event for {}",
                              stateId);
                    handleParallelStateCompletion(stateId);
                }
            }
        }
    }

    // SCXML W3C macrostep compliance: Check if reentrant transition occurred during state entry
    // This handles cases where done.state events cause immediate transitions (e.g., parallel state completion)
    std::string actualCurrentState = getCurrentState();
    LOG_DEBUG("StateMachine: After entering '{}', getCurrentState() returns '{}'", stateId, actualCurrentState);
    if (actualCurrentState != stateId) {
        LOG_DEBUG("SCXML macrostep: State transition occurred during entry (expected: {}, actual: {})", stateId,
                  actualCurrentState);
        LOG_DEBUG("This indicates a valid internal transition (e.g., done.state event) - macrostep continuing");
        // Note: Configuration changed during entry, will check eventless transitions on actual configuration below
    }

    // W3C SCXML: onentry actions (including invokes) are executed via callback from StateHierarchyManager
    // This ensures proper execution order per W3C specification

    // NOTE: _state is not a W3C SCXML standard system variable (only _event, _sessionid, _name, _ioprocessors, _x
    // exist) Setting _state here causes issues with invoke lifecycle when child sessions terminate Removed to comply
    // with W3C SCXML 5.10 specification

    LOG_DEBUG("Successfully entered state using hierarchy manager: {} (current: {})", stateId, getCurrentState());

    // W3C SCXML 6.5: Check for top-level final state and invoke completion callback
    // IMPORTANT: Only for invoked child StateMachines, not for parallel regions
    if (model_ && completionCallback_) {
        auto stateNode = model_->findStateById(actualCurrentState);
        if (stateNode && stateNode->isFinalState()) {
            // Check if this is a top-level final state by checking parent chain
            // Top-level states have no parent or parent is the <scxml> root element
            // We need to traverse up to ensure we're not in a parallel region
            auto parent = stateNode->getParent();
            bool isTopLevel = false;

            if (!parent) {
                // No parent means root-level final state
                isTopLevel = true;
            } else {
                // Check if parent is <scxml> root or if we're in a direct child of <scxml>
                // Parallel regions have intermediate parent states, so we need to check the entire chain
                auto grandparent = parent->getParent();
                if (!grandparent || parent->getId() == "scxml") {
                    // Parent is root or state is direct child of root
                    isTopLevel = true;
                } else if (grandparent->getId() == "scxml" && parent->getType() != RSM::Type::PARALLEL) {
                    // Grandparent is root and parent is NOT a parallel state
                    // This means we're a final state in a compound/atomic state at root level
                    isTopLevel = true;
                }
                // If parent is PARALLEL or we're deeper in the hierarchy, this is NOT top-level
            }

            if (isTopLevel) {
                LOG_INFO("StateMachine: Reached top-level final state: {}, executing onexit then completion callback",
                         actualCurrentState);

                // W3C SCXML: Execute onexit actions BEFORE generating done.invoke
                // For top-level final states, onexit runs when state machine completes
                bool exitResult = executeExitActions(actualCurrentState);
                if (!exitResult) {
                    LOG_WARN("StateMachine: Failed to execute onexit for final state: {}", actualCurrentState);
                }

                // IMPORTANT: Callback is invoked AFTER onexit handlers execute
                // This ensures correct event order: child events → done.invoke
                if (completionCallback_) {
                    try {
                        completionCallback_();
                    } catch (const std::exception &e) {
                        LOG_ERROR("StateMachine: Exception in completion callback: {}", e.what());
                    }
                }
            }
        }
    }

    // W3C SCXML 3.7 & 5.5: Generate done.state event for compound state completion
    if (model_) {
        auto stateNode = model_->findStateById(actualCurrentState);
        if (stateNode && stateNode->isFinalState()) {
            handleCompoundStateFinalChild(actualCurrentState);
        }
    }

    // Clear guard flag before checking automatic transitions
    isEnteringState_ = false;

    // W3C SCXML: Check for eventless transitions after state entry
    checkEventlessTransitions();

    return true;
}

bool StateMachine::checkEventlessTransitions() {
    // SCXML W3C specification: Check for eventless transitions on ALL active states
    // Per W3C spec: "select transitions enabled by NULL in the current configuration"
    if (!model_) {
        return false;
    }

    auto activeStates = hierarchyManager_->getActiveStates();
    LOG_DEBUG("SCXML: Checking eventless transitions on {} active state(s) in current configuration",
              activeStates.size());

    // Process states from innermost to outermost (same as event processing)
    for (auto it = activeStates.rbegin(); it != activeStates.rend(); ++it) {
        const std::string &activeStateId = *it;
        auto stateNode = model_->findStateById(activeStateId);

        if (!stateNode) {
            LOG_WARN("SCXML: Active state '{}' not found in model", activeStateId);
            continue;
        }

        LOG_DEBUG("SCXML: Checking state '{}' for eventless transitions", activeStateId);

        // Process eventless transitions (empty event name)
        auto transitionResult = processStateTransitions(stateNode, "", "");
        if (transitionResult.success) {
            LOG_DEBUG("SCXML: Eventless transition executed from state '{}': {} -> {}", activeStateId,
                      transitionResult.fromState, transitionResult.toState);
            // W3C: First enabled transition executes, then return true
            return true;
        }
    }

    LOG_DEBUG("SCXML: No eventless transitions found in active configuration");
    return false;
}

bool StateMachine::exitState(const std::string &stateId) {
    LOG_DEBUG("Exiting state: {}", stateId);

    // SCXML W3C specification section 3.4: Execute exit actions in correct order for parallel states
    auto stateNode = model_->findStateById(stateId);
    if (stateNode && stateNode->getType() == Type::PARALLEL) {
        // For parallel states: Child regions exit FIRST, then parallel state exits
        LOG_DEBUG("StateMachine: SCXML W3C compliant - executing parallel state exit actions in correct order");

        // Exit actions for child regions are already handled by executeExitActions for parallel
        // Execute parallel state's own onexit actions LAST
        bool exitResult = executeExitActions(stateId);
        if (!exitResult && isRunning_) {
            // Only log error if machine is still running - during shutdown, raise failures are expected
            LOG_ERROR("StateMachine: Failed to execute exit actions for parallel state: {}", stateId);
        }
    } else {
        // Execute IActionNode-based exit actions for non-parallel states
        bool exitResult = executeExitActions(stateId);
        if (!exitResult && isRunning_) {
            // Only log error if machine is still running - during shutdown, raise failures are expected
            LOG_ERROR("StateMachine: Failed to execute exit actions for state: {}", stateId);
        }
        (void)exitResult;  // Suppress unused variable warning in release builds
    }

    // Get state node for invoke cancellation and history recording
    auto stateNodeForCleanup = model_->findStateById(stateId);

    // W3C SCXML specification section 3.13: Cancel invokes BEFORE removing from active states
    // "Then it MUST cancel any ongoing invocations that were triggered by that state"
    // This must happen AFTER onexit handlers but BEFORE state removal
    if (stateNodeForCleanup && invokeExecutor_) {
        const auto &invokes = stateNodeForCleanup->getInvoke();
        LOG_DEBUG("StateMachine::exitState - State '{}' has {} invoke(s) to check", stateId, invokes.size());

        for (const auto &invoke : invokes) {
            const std::string &invokeid = invoke->getId();
            if (!invokeid.empty()) {
                bool isActive = invokeExecutor_->isInvokeActive(invokeid);
                LOG_DEBUG("StateMachine::exitState - Invoke '{}' isActive: {}", invokeid, isActive);

                if (isActive) {
                    LOG_DEBUG("StateMachine: Cancelling active invoke '{}' due to state exit: {}", invokeid, stateId);
                    bool cancelled = invokeExecutor_->cancelInvoke(invokeid);
                    LOG_DEBUG("StateMachine: Cancel result for invoke '{}': {}", invokeid, cancelled);
                } else {
                    LOG_DEBUG("StateMachine: NOT cancelling inactive invoke '{}' (may be completing naturally)",
                              invokeid);
                }
            } else {
                LOG_WARN("StateMachine::exitState - Found invoke with empty ID in state '{}'", stateId);
            }
        }
    } else {
        if (!stateNodeForCleanup) {
            LOG_DEBUG("StateMachine::exitState - stateNodeForCleanup is null for state '{}'", stateId);
        }
        if (!invokeExecutor_) {
            LOG_DEBUG("StateMachine::exitState - invokeExecutor_ is null");
        }
    }

    // Record history before removing from active states (SCXML W3C specification section 3.6)
    // History recording needs current active states, so must happen before hierarchyManager_->exitState
    if (historyManager_ && hierarchyManager_ && stateNodeForCleanup) {
        if (stateNodeForCleanup->getType() == Type::COMPOUND || stateNodeForCleanup->getType() == Type::PARALLEL) {
            // Get current active states before exiting
            auto activeStates = hierarchyManager_->getActiveStates();

            // Record history for this compound state
            bool recorded = historyManager_->recordHistory(stateId, activeStates);
            if (recorded) {
                LOG_DEBUG("Recorded history for compound state: {}", stateId);
            }
        }
    }

    // W3C SCXML section 3.13: Finally remove the state from active states list
    // Use hierarchy manager for SCXML-compliant state exit
    assert(hierarchyManager_ && "SCXML violation: hierarchy manager required for state management");
    LOG_DEBUG("StateMachine::exitState - executionContext_ is {}", executionContext_ ? "valid" : "NULL");
    hierarchyManager_->exitState(stateId, executionContext_);

    // State management fully delegated to StateHierarchyManager

    LOG_DEBUG("Successfully exited state: {}", stateId);
    return true;
}

bool StateMachine::ensureJSEnvironment() {
    if (jsEnvironmentReady_) {
        return true;
    }

    return setupJSEnvironment();
}

bool StateMachine::setupJSEnvironment() {
    // JSEngine은 생성자에서 자동 초기화됨 (RAII)
    auto &jsEngine = RSM::JSEngine::instance();  // RAII 보장
    LOG_DEBUG("StateMachine: JSEngine automatically initialized via RAII at address: {}",
              static_cast<void *>(&jsEngine));

    // Create JavaScript session only if it doesn't exist (for invoke scenarios)
    // Check if session already exists (created by InvokeExecutor for child sessions)
    bool sessionExists = RSM::JSEngine::instance().hasSession(sessionId_);

    if (!sessionExists) {
        // Create new session for standalone StateMachine
        if (!RSM::JSEngine::instance().createSession(sessionId_)) {
            LOG_ERROR("StateMachine: Failed to create JavaScript session");
            return false;
        }
        LOG_DEBUG("StateMachine: Created new JavaScript session: {}", sessionId_);
    } else {
        LOG_DEBUG("StateMachine: Using existing JavaScript session (injected): {}", sessionId_);
    }

    // Set up basic variables
    RSM::JSEngine::instance().setVariable(sessionId_, "_sessionid", ScriptValue{sessionId_});
    RSM::JSEngine::instance().setVariable(sessionId_, "_name", ScriptValue{std::string("StateMachine")});

    // Register this StateMachine instance with JSEngine for In() function support
    RSM::JSEngine::instance().setStateMachine(this, sessionId_);
    LOG_DEBUG("StateMachine: Registered with JSEngine for In() function support");

    // W3C SCXML 5.3: Initialize data model with binding mode support (early/late binding)
    if (model_) {
        // Collect all data items (top-level + state-level) for global scope
        const auto allDataItems = collectAllDataItems();
        LOG_INFO("StateMachine: Initializing {} total data items (global scope with {} binding)", allDataItems.size(),
                 model_->getBinding());

        // Get binding mode: "early" (default) or "late"
        const std::string &binding = model_->getBinding();
        bool isEarlyBinding = (binding.empty() || binding == "early");

        if (isEarlyBinding) {
            // Early binding (default): Initialize all variables with values at document load
            LOG_DEBUG("StateMachine: Using early binding - all variables initialized with values at init");
            for (const auto &dataInfo : allDataItems) {
                initializeDataItem(dataInfo.dataItem, true);  // assignValue=true
            }
        } else {
            // Late binding: Create all variables but don't assign values yet
            LOG_DEBUG("StateMachine: Using late binding - creating variables without values (assigned on state entry)");
            for (const auto &dataInfo : allDataItems) {
                initializeDataItem(dataInfo.dataItem, false);  // assignValue=false (defer assignment)
            }
            // Note: Value assignment will happen in enterState() when each state is entered
        }
    } else {
        LOG_DEBUG("StateMachine: No model available for data model initialization");
    }

    // Initialize ActionExecutor and ExecutionContext
    if (!initializeActionExecutor()) {
        LOG_ERROR("StateMachine: Failed to initialize action executor");
        return false;
    }

    // Pass EventDispatcher to ActionExecutor if it was set before initialization
    if (eventDispatcher_ && actionExecutor_) {
        auto actionExecutorImpl = std::dynamic_pointer_cast<ActionExecutorImpl>(actionExecutor_);
        if (actionExecutorImpl) {
            actionExecutorImpl->setEventDispatcher(eventDispatcher_);
            LOG_DEBUG(
                "StateMachine: EventDispatcher passed to ActionExecutor during JS environment setup for session: {}",
                sessionId_);
        }
    }

    // Pass EventRaiser to ActionExecutor if available
    if (eventRaiser_ && actionExecutor_) {
        actionExecutor_->setEventRaiser(eventRaiser_);
        LOG_DEBUG("StateMachine: EventRaiser passed to ActionExecutor for session: {}", sessionId_);
    }

    // Register EventRaiser with JSEngine after session creation
    // This handles both cases: EventRaiser set before session creation (deferred) and after
    if (eventRaiser_) {
        // Use EventRaiserService for centralized registration
        if (EventRaiserService::getInstance().registerEventRaiser(sessionId_, eventRaiser_)) {
            LOG_DEBUG("StateMachine: EventRaiser registered via Service after session creation for session: {}",
                      sessionId_);
        } else {
            LOG_DEBUG("StateMachine: EventRaiser already registered for session: {}", sessionId_);
        }
    }

    jsEnvironmentReady_ = true;
    LOG_DEBUG("StateMachine: JavaScript environment setup completed");
    return true;
}

void StateMachine::updateStatistics() {
    stats_.currentState = getCurrentState();
    stats_.isRunning = isRunning_;
}

bool StateMachine::initializeActionExecutor() {
    try {
        // Create ActionExecutor using the same session as StateMachine
        actionExecutor_ = std::make_shared<ActionExecutorImpl>(sessionId_);

        // Inject EventRaiser if already set via builder pattern
        if (eventRaiser_) {
            actionExecutor_->setEventRaiser(eventRaiser_);
            LOG_DEBUG("StateMachine: EventRaiser injected to ActionExecutor during initialization for session: {}",
                      sessionId_);
        }

        // Create ExecutionContext with shared_ptr and sessionId
        executionContext_ = std::make_shared<ExecutionContextImpl>(actionExecutor_, sessionId_);

        LOG_DEBUG("ActionExecutor and ExecutionContext initialized for session: {}", sessionId_);
        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to initialize ActionExecutor: {}", e.what());
        return false;
    }
}

bool StateMachine::executeActionNodes(const std::vector<std::shared_ptr<RSM::IActionNode>> &actions,
                                      bool processEventsAfter) {
    if (!executionContext_) {
        LOG_WARN("StateMachine: ExecutionContext not initialized, skipping action node execution");
        return true;  // Not a failure, just no actions to execute
    }

    bool allSucceeded = true;

    // W3C SCXML compliance: Set immediate mode to false during executable content execution
    // This ensures events raised during execution are queued and processed after completion
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            eventRaiserImpl->setImmediateMode(false);
            LOG_DEBUG("SCXML compliance: Set immediate mode to false for executable content execution");
        }
    }

    for (const auto &action : actions) {
        if (!action) {
            LOG_WARN("StateMachine: Null action node encountered, skipping");
            continue;
        }

        try {
            LOG_DEBUG("Executing action: {}", action->getActionType());
            if (action->execute(*executionContext_)) {
                LOG_DEBUG("Successfully executed action: {}", action->getActionType());
            } else {
                LOG_WARN("Failed to execute action: {} - W3C compliance: stopping remaining actions",
                         action->getActionType());
                allSucceeded = false;
                // W3C SCXML specification: If error occurs in executable content,
                // processor MUST NOT process remaining elements in the block
                break;
            }
        } catch (const std::exception &e) {
            LOG_WARN("Exception executing action {}: {} - W3C compliance: stopping remaining actions",
                     action->getActionType(), e.what());
            allSucceeded = false;
            // W3C SCXML specification: If error occurs in executable content,
            // processor MUST NOT process remaining elements in the block
            break;
        }
    }

    // W3C SCXML compliance: Restore immediate mode and optionally process queued events
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            eventRaiserImpl->setImmediateMode(true);
            // Process events only if requested (e.g., for entry actions, not exit/transition actions)
            if (processEventsAfter) {
                eventRaiserImpl->processQueuedEvents();
                LOG_DEBUG("SCXML compliance: Restored immediate mode and processed queued events");
            } else {
                LOG_DEBUG("SCXML compliance: Restored immediate mode (events will be processed later)");
            }
        }
    }

    // W3C SCXML compliance: Return true only if all actions succeeded or no actions to execute
    // If any action failed, we stopped execution per W3C spec, so return false to indicate failure
    return actions.empty() || allSucceeded;
}

bool StateMachine::executeEntryActions(const std::string &stateId) {
    if (!model_) {
        assert(false && "SCXML violation: StateMachine must have a model for entry action execution");
        return false;
    }

    // Find the StateNode in the SCXML model
    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        // SCXML W3C compliance: All states in active configuration must exist in model
        assert(false && "SCXML violation: Active state not found in model");
        return false;
    }

    LOG_DEBUG("Executing entry actions for state: {}", stateId);

    // SCXML W3C specification section 3.4: Parallel states require special handling
    if (stateNode->getType() == Type::PARALLEL) {
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        // SCXML W3C specification section 3.4: Execute parallel state's own onentry actions FIRST
        const auto &parallelEntryActions = parallelState->getEntryActionNodes();
        if (!parallelEntryActions.empty()) {
            LOG_DEBUG("SCXML W3C compliant - executing {} entry actions for parallel state itself: {}",
                      parallelEntryActions.size(), stateId);
            if (!executeActionNodes(parallelEntryActions)) {
                LOG_ERROR("Failed to execute parallel state entry actions for: {}", stateId);
                return false;
            }
        }

        // provide ExecutionContext to all regions for action execution
        if (executionContext_) {
            parallelState->setExecutionContextForRegions(executionContext_);
            LOG_DEBUG("Injected ExecutionContext into all regions of parallel state: {}", stateId);
        }

        // SCXML W3C specification: ALL child regions MUST have their entry actions executed AFTER parallel state
        const auto &regions = parallelState->getRegions();
        assert(!regions.empty() && "SCXML violation: parallel state must have at least one region");

        LOG_DEBUG("SCXML W3C compliant - executing entry actions for {} child regions in parallel state: {}",
                  regions.size(), stateId);

        // Execute entry actions for each region's root state
        for (const auto &region : regions) {
            assert(region && "SCXML violation: parallel state cannot have null regions");

            auto rootState = region->getRootState();
            assert(rootState && "SCXML violation: region must have root state");

            // Execute entry actions for the region's root state
            const auto &regionEntryActions = rootState->getEntryActionNodes();
            if (!regionEntryActions.empty()) {
                LOG_DEBUG("Executing {} entry actions for region: {}", regionEntryActions.size(), region->getId());
                if (!executeActionNodes(regionEntryActions)) {
                    LOG_ERROR("Failed to execute entry actions for region: {}", region->getId());
                    return false;
                }
            }

            // SCXML W3C specification: Enter initial child states of each region ONLY if not already active
            const auto &children = rootState->getChildren();
            if (!children.empty()) {
                // SCXML W3C 사양 준수: 병렬 영역이 이미 활성화되어 있으면 초기 상태로 재진입하지 않음
                if (!region->isActive()) {
                    std::string initialChild = rootState->getInitialState();
                    if (initialChild.empty()) {
                        // SCXML W3C: Use first child as default initial state
                        initialChild = children[0]->getId();
                    }

                    LOG_DEBUG("Entering initial child state for INACTIVE region {}: {}", region->getId(), initialChild);

                    // Execute entry actions for the initial child state
                    auto childState = model_->findStateById(initialChild);
                    if (childState) {
                        const auto &childEntryActions = childState->getEntryActionNodes();
                        if (!childEntryActions.empty()) {
                            LOG_DEBUG("Executing {} entry actions for initial child state: {}",
                                      childEntryActions.size(), initialChild);
                            if (!executeActionNodes(childEntryActions)) {
                                LOG_ERROR("Failed to execute entry actions for initial child state: {}", initialChild);
                                return false;
                            }
                        }
                    }
                } else {
                    // SCXML W3C 사양 준수: 이미 활성화된 영역은 초기 상태로 재진입하지 않음
                    auto concreteRegion = std::dynamic_pointer_cast<ConcurrentRegion>(region);
                    std::string currentState = concreteRegion ? concreteRegion->getCurrentState() : "unknown";

                    LOG_DEBUG("SCXML W3C compliance - skipping initial state entry for already ACTIVE region: {} "
                              "(current state: {})",
                              region->getId(), currentState);

                    // SCXML W3C 사양 위반 방지: 이미 활성화된 영역의 현재 상태 유지
                    assert(concreteRegion && !concreteRegion->getCurrentState().empty() &&
                           "SCXML violation: active region must have current state");

                    // SCXML W3C 사양 준수 검증: 활성 영역이 초기 상태로 재설정되지 않음을 보장
                    assert(region->isActive() &&
                           "SCXML violation: region marked as active but isActive() returns false");

                    // SCXML W3C 사양 위반 감지: 병렬 상태 재진입 시 상태 일관성 검증
                    const auto &currentActiveStates = region->getActiveStates();
                    assert(!currentActiveStates.empty() && "SCXML violation: active region must have active states");
                }
            }
        }

        return true;
    }

    // Execute IActionNode-based entry actions for non-parallel states
    const auto &entryActions = stateNode->getEntryActionNodes();
    if (!entryActions.empty()) {
        LOG_DEBUG("Executing {} entry action nodes for state: {}", entryActions.size(), stateId);
        return executeActionNodes(entryActions);
    }

    return true;
}

bool StateMachine::executeExitActions(const std::string &stateId) {
    if (!model_) {
        return true;  // No model, no actions to execute
    }

    // Find the StateNode in the SCXML model
    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        LOG_DEBUG("State {} not found in SCXML model, skipping exit actions", stateId);
        return true;  // Not an error if state not found in model
    }

    // SCXML W3C specification section 3.4: Parallel states require special exit sequence
    if (stateNode->getType() == Type::PARALLEL) {
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        const auto &regions = parallelState->getRegions();
        assert(!regions.empty() && "SCXML violation: parallel state must have at least one region");

        LOG_DEBUG("SCXML W3C compliant - executing exit sequence for parallel state: {}", stateId);

        // SCXML W3C specification: Execute child region exit actions FIRST in REVERSE document order
        for (auto it = regions.rbegin(); it != regions.rend(); ++it) {
            const auto &region = *it;
            assert(region && "SCXML violation: parallel state cannot have null regions");

            if (region->isActive()) {
                auto rootState = region->getRootState();
                assert(rootState && "SCXML violation: region must have root state");

                // Execute exit actions for currently active child states in this region
                const auto activeStates = region->getActiveStates();
                const auto &children = rootState->getChildren();
                for (const auto &child : children) {
                    // Check if this child is currently active
                    bool isChildActive =
                        std::find(activeStates.begin(), activeStates.end(), child->getId()) != activeStates.end();
                    if (child && isChildActive) {
                        const auto &childExitActions = child->getExitActionNodes();
                        if (!childExitActions.empty()) {
                            LOG_DEBUG("SCXML W3C compliant - executing {} exit actions for active child state: {}",
                                      childExitActions.size(), child->getId());
                            if (!executeActionNodes(childExitActions, false)) {
                                LOG_ERROR("Failed to execute exit actions for child state: {}", child->getId());
                                return false;
                            }
                        }
                        break;
                    }
                }

                // Execute exit actions for the region's root state
                const auto &regionExitActions = rootState->getExitActionNodes();
                if (!regionExitActions.empty()) {
                    LOG_DEBUG("SCXML W3C compliant - executing {} exit actions for region: {}",
                              regionExitActions.size(), region->getId());
                    if (!executeActionNodes(regionExitActions, false)) {
                        LOG_ERROR("Failed to execute exit actions for region: {}", region->getId());
                        return false;
                    }
                }
            }
        }

        // SCXML W3C specification: Execute parallel state's own onexit actions LAST
        const auto &parallelExitActions = parallelState->getExitActionNodes();
        if (!parallelExitActions.empty()) {
            LOG_DEBUG("SCXML W3C compliant - executing {} exit actions for parallel state itself: {}",
                      parallelExitActions.size(), stateId);
            return executeActionNodes(parallelExitActions, false);
        }

        return true;
    }

    // Execute IActionNode-based exit actions for non-parallel states
    const auto &exitActions = stateNode->getExitActionNodes();
    if (!exitActions.empty()) {
        LOG_DEBUG("Executing {} exit action nodes for state: {}", exitActions.size(), stateId);
        return executeActionNodes(exitActions, false);
    }

    return true;
}

void StateMachine::handleParallelStateCompletion(const std::string &stateId) {
    LOG_DEBUG("Handling parallel state completion for: {}", stateId);

    // Generate done.state.{stateId} event according to SCXML W3C specification section 3.4
    std::string doneEventName = "done.state." + stateId;

    LOG_INFO("Generating done.state event: {} for completed parallel state: {}", doneEventName, stateId);

    // Process the done.state event to trigger any transitions waiting for it
    if (isRunning_) {
        auto result = processEvent(doneEventName, "");
        if (result.success) {
            LOG_DEBUG("Successfully processed done.state event: {}", doneEventName);
        } else {
            LOG_DEBUG("No transitions found for done.state event: {} (this is normal if no transitions are waiting for "
                      "this event)",
                      doneEventName);
        }
    } else {
        LOG_WARN("Cannot process done.state event {} - state machine is not running", doneEventName);
    }
}

void StateMachine::setupParallelStateCallbacks() {
    if (!model_) {
        LOG_WARN("StateMachine: Cannot setup parallel state callbacks - no model available");
        return;
    }

    LOG_DEBUG("StateMachine: Setting up completion callbacks for parallel states");

    const auto &allStates = model_->getAllStates();
    int parallelStateCount = 0;

    for (const auto &state : allStates) {
        if (state && state->getType() == Type::PARALLEL) {
            // Cast to ConcurrentStateNode to access the callback method
            auto parallelState = std::dynamic_pointer_cast<ConcurrentStateNode>(state);
            if (parallelState) {
                // Set up the completion callback using a lambda that captures this StateMachine
                parallelState->setCompletionCallback([this](const std::string &completedStateId) {
                    this->handleParallelStateCompletion(completedStateId);
                });

                parallelStateCount++;
                LOG_DEBUG("Set up completion callback for parallel state: {}", state->getId());
            } else {
                LOG_WARN("Found parallel state that is not a ConcurrentStateNode: {}", state->getId());
            }
        }
    }

    LOG_INFO("Set up completion callbacks for {} parallel states", parallelStateCount);
}

void StateMachine::initializeHistoryManager() {
    LOG_DEBUG("StateMachine: Initializing History Manager with SOLID architecture");

    // Create state provider function for dependency injection
    auto stateProvider = [this](const std::string &stateId) -> std::shared_ptr<IStateNode> {
        if (!model_) {
            return nullptr;
        }
        // Find state by ID in the shared_ptr vector
        auto allStates = model_->getAllStates();
        for (const auto &state : allStates) {
            if (state && state->getId() == stateId) {
                return state;
            }
        }
        return nullptr;
    };

    // Create filter components using Strategy pattern
    auto shallowFilter = std::make_unique<ShallowHistoryFilter>(stateProvider);
    auto deepFilter = std::make_unique<DeepHistoryFilter>(stateProvider);
    auto validator = std::make_unique<HistoryValidator>(stateProvider);

    // Create HistoryManager with dependency injection
    historyManager_ = std::make_unique<HistoryManager>(stateProvider, std::move(shallowFilter), std::move(deepFilter),
                                                       std::move(validator));

    LOG_INFO("StateMachine: History Manager initialized with SOLID dependencies");
}

void StateMachine::initializeHistoryAutoRegistrar() {
    LOG_DEBUG("StateMachine: Initializing History Auto-Registrar with SOLID architecture");

    // Create state provider function for dependency injection (same as history manager)
    auto stateProvider = [this](const std::string &stateId) -> std::shared_ptr<IStateNode> {
        if (!model_) {
            return nullptr;
        }
        // Find state by ID in the model
        auto allStates = model_->getAllStates();
        for (const auto &state : allStates) {
            if (state && state->getId() == stateId) {
                return state;
            }
        }
        return nullptr;
    };

    // Create HistoryStateAutoRegistrar with dependency injection
    historyAutoRegistrar_ = std::make_unique<HistoryStateAutoRegistrar>(stateProvider);

    LOG_INFO("StateMachine: History Auto-Registrar initialized with SOLID dependencies");
}

bool StateMachine::registerHistoryState(const std::string &historyStateId, const std::string &parentStateId,
                                        HistoryType type, const std::string &defaultStateId) {
    if (!historyManager_) {
        LOG_ERROR("StateMachine: History Manager not initialized");
        return false;
    }

    return historyManager_->registerHistoryState(historyStateId, parentStateId, type, defaultStateId);
}

bool StateMachine::isHistoryState(const std::string &stateId) const {
    if (!historyManager_) {
        return false;
    }

    return historyManager_->isHistoryState(stateId);
}

void StateMachine::clearAllHistory() {
    if (historyManager_) {
        historyManager_->clearAllHistory();
    }
}

std::vector<HistoryEntry> StateMachine::getHistoryEntries() const {
    if (!historyManager_) {
        return {};
    }

    return historyManager_->getHistoryEntries();
}

void StateMachine::executeOnEntryActions(const std::string &stateId) {
    if (!model_) {
        LOG_ERROR("Cannot execute onentry actions: SCXML model is null");
        return;
    }

    // Find the state node
    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        LOG_ERROR("Cannot find state node for onentry execution: {}", stateId);
        return;
    }

    // Get entry actions from the state
    const auto &entryActions = stateNode->getEntryActionNodes();
    if (entryActions.empty()) {
        LOG_DEBUG("No onentry actions to execute for state: {}", stateId);
        return;
    }

    LOG_DEBUG("Executing {} onentry actions for state: {}", entryActions.size(), stateId);

    // W3C SCXML compliance: Set immediate mode to false during executable content execution
    // This ensures events raised during execution are queued and processed after completion
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            eventRaiserImpl->setImmediateMode(false);
            LOG_DEBUG("SCXML compliance: Set immediate mode to false for onentry actions execution");
        }
    }

    // W3C SCXML: Execute onentry handlers in document order
    for (const auto &action : entryActions) {
        if (!action) {
            LOG_WARN("Null onentry action found in state: {}", stateId);
            continue;
        }

        LOG_DEBUG("StateMachine: Executing onentry action: {} in state: {}", action->getActionType(), stateId);

        // Create execution context for the action
        if (actionExecutor_) {
            auto sharedActionExecutor =
                std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
            ExecutionContextImpl context(sharedActionExecutor, sessionId_);

            // Execute the action
            if (!action->execute(context)) {
                LOG_ERROR("StateMachine: Failed to execute onentry action: {} in state: {} - W3C compliance: stopping "
                          "remaining actions",
                          action->getActionType(), stateId);
                // W3C SCXML specification: If error occurs in executable content,
                // processor MUST NOT process remaining elements in the block
                break;
            } else {
                LOG_DEBUG("StateMachine: Successfully executed onentry action: {} in state: {}",
                          action->getActionType(), stateId);
            }
        } else {
            LOG_ERROR("Cannot execute onentry action: ActionExecutor is null");
        }
    }

    // W3C SCXML compliance: Restore immediate mode and process queued events
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            eventRaiserImpl->setImmediateMode(true);
            // Process any events that were queued during executable content execution
            eventRaiserImpl->processQueuedEvents();
            LOG_DEBUG("SCXML compliance: Restored immediate mode and processed queued events");
        }
    }

    // W3C SCXML: Defer invoke execution until after state entry completes
    // This ensures proper timing with transition actions and pre-registration pattern
    const auto &invokes = stateNode->getInvoke();
    if (!invokes.empty()) {
        LOG_DEBUG("StateMachine: Deferring {} invokes for state: {}", invokes.size(), stateId);
        deferInvokeExecution(stateId, invokes);
    } else {
        LOG_DEBUG("StateMachine: No invokes to defer for state: {}", stateId);
    }
}

// EventDispatcher management
void StateMachine::setEventDispatcher(std::shared_ptr<IEventDispatcher> eventDispatcher) {
    eventDispatcher_ = eventDispatcher;

    // Pass EventDispatcher to ActionExecutor for send actions
    if (actionExecutor_) {
        auto actionExecutorImpl = std::dynamic_pointer_cast<ActionExecutorImpl>(actionExecutor_);
        if (actionExecutorImpl) {
            actionExecutorImpl->setEventDispatcher(eventDispatcher);
            LOG_DEBUG("StateMachine: EventDispatcher passed to ActionExecutor for session: {}", sessionId_);
        }
    }

    // Pass EventDispatcher to InvokeExecutor for child session management
    if (invokeExecutor_) {
        invokeExecutor_->setEventDispatcher(eventDispatcher);
        LOG_DEBUG("StateMachine: EventDispatcher passed to InvokeExecutor for session: {}", sessionId_);

        // W3C SCXML Test 192: Set parent StateMachine for completion callback state checking
        // Only set if this StateMachine is managed by shared_ptr (not during construction)
        // This will be set later in executeInvoke() when actually needed
    }
}

// W3C SCXML 6.5: Completion callback management
void StateMachine::setCompletionCallback(CompletionCallback callback) {
    completionCallback_ = callback;
    LOG_DEBUG("StateMachine: Completion callback {} for session: {}", callback ? "set" : "cleared", sessionId_);
}

// EventRaiser management
void StateMachine::setEventRaiser(std::shared_ptr<IEventRaiser> eventRaiser) {
    LOG_DEBUG("StateMachine: setEventRaiser called for session: {}", sessionId_);
    eventRaiser_ = eventRaiser;

    // SCXML W3C compliance: Set EventRaiser callback to StateMachine's processEvent
    // This allows events generated by raise actions to actually trigger state transitions
    if (eventRaiser_) {
        auto eventRaiserImpl = std::dynamic_pointer_cast<EventRaiserImpl>(eventRaiser_);
        if (eventRaiserImpl) {
            LOG_DEBUG("StateMachine: EventRaiser callback setup - EventRaiser instance: {}, StateMachine instance: {}",
                      (void *)eventRaiserImpl.get(), (void *)this);
            // Set StateMachine's processEvent method as EventRaiser callback
            eventRaiserImpl->setEventCallback(
                [this](const std::string &eventName, const std::string &eventData) -> bool {
                    if (isRunning_) {
                        LOG_DEBUG("EventRaiser callback: StateMachine::processEvent called - event: '{}', data: '{}', "
                                  "StateMachine instance: {}",
                                  eventName, eventData, (void *)this);
                        // Use 2-parameter version (no originSessionId from old callback)
                        auto result = processEvent(eventName, eventData);
                        LOG_DEBUG("EventRaiser callback: processEvent result - success: {}, state transition: {} -> {}",
                                  result.success, result.fromState, result.toState);
                        return result.success;
                    } else {
                        LOG_WARN("EventRaiser callback: StateMachine not running - ignoring event '{}'", eventName);
                        return false;
                    }
                });
            LOG_DEBUG("StateMachine: EventRaiser callback set to processEvent - session: {}, EventRaiser instance: {}",
                      sessionId_, (void *)eventRaiserImpl.get());
        }
    }

    // Register EventRaiser with JSEngine for #_invokeid target support
    // Use EventRaiserService for centralized registration
    if (eventRaiser_) {
        if (EventRaiserService::getInstance().registerEventRaiser(sessionId_, eventRaiser_)) {
            LOG_DEBUG("StateMachine: EventRaiser registered via Service for session: {}", sessionId_);
        } else {
            LOG_DEBUG("StateMachine: EventRaiser registration deferred or already exists for session: {}", sessionId_);
        }
    }

    // Pass EventRaiser to ActionExecutor if it exists (during build phase)
    if (actionExecutor_) {
        actionExecutor_->setEventRaiser(eventRaiser);
        LOG_DEBUG("StateMachine: EventRaiser passed to ActionExecutor for session: {}", sessionId_);
    }
    // Note: If ActionExecutor doesn't exist yet, it will be set during loadSCXMLFromString
}

std::shared_ptr<IEventDispatcher> StateMachine::getEventDispatcher() const {
    return eventDispatcher_;
}

void StateMachine::deferInvokeExecution(const std::string &stateId,
                                        const std::vector<std::shared_ptr<IInvokeNode>> &invokes) {
    LOG_DEBUG("StateMachine: Deferring {} invokes for state: {} in session: {}", invokes.size(), stateId, sessionId_);

    // Log each invoke being deferred
    for (size_t i = 0; i < invokes.size(); ++i) {
        const auto &invoke = invokes[i];
        std::string invokeId = invoke ? invoke->getId() : "null";
        std::string invokeType = invoke ? invoke->getType() : "null";
        LOG_DEBUG("StateMachine: DETAILED DEBUG - Deferring invoke[{}]: id='{}', type='{}'", i, invokeId, invokeType);
    }

    DeferredInvoke deferred;
    deferred.stateId = stateId;
    deferred.invokes = invokes;

    // Thread-safe access to pendingInvokes_
    std::lock_guard<std::mutex> lock(pendingInvokesMutex_);
    size_t beforeSize = pendingInvokes_.size();
    pendingInvokes_.push_back(std::move(deferred));

    LOG_DEBUG("StateMachine: DETAILED DEBUG - Pending invokes count: {} -> {}", beforeSize, pendingInvokes_.size());
}

void StateMachine::executePendingInvokes() {
    // W3C SCXML Test 192: Set parent StateMachine before executing invokes (requires shared_ptr context)
    // This is safe here because executePendingInvokes() is only called when StateMachine is already in shared_ptr
    // context
    if (invokeExecutor_) {
        try {
            invokeExecutor_->setParentStateMachine(shared_from_this());
            LOG_DEBUG(
                "StateMachine: Parent StateMachine set in InvokeExecutor before executing invokes for session: {}",
                sessionId_);
        } catch (const std::bad_weak_ptr &e) {
            LOG_WARN("StateMachine: Cannot set parent StateMachine - not managed by shared_ptr yet for session: {}",
                     sessionId_);
        }
    }

    // Thread-safe copy of pending invokes
    std::vector<DeferredInvoke> invokesToExecute;
    {
        std::lock_guard<std::mutex> lock(pendingInvokesMutex_);
        if (pendingInvokes_.empty()) {
            LOG_DEBUG("StateMachine: No pending invokes to execute for session: {}", sessionId_);
            return;
        }

        LOG_DEBUG("StateMachine: Found {} pending invokes to execute for session: {}", pendingInvokes_.size(),
                  sessionId_);

        LOG_DEBUG("StateMachine: DETAILED DEBUG - Executing {} pending invokes for session {}", pendingInvokes_.size(),
                  sessionId_);

        // Log all pending invokes for debugging
        for (size_t i = 0; i < pendingInvokes_.size(); ++i) {
            const auto &deferred = pendingInvokes_[i];
            LOG_DEBUG("StateMachine: DETAILED DEBUG - Pending invoke[{}]: stateId='{}', invokeCount={}", i,
                      deferred.stateId, deferred.invokes.size());
            for (size_t j = 0; j < deferred.invokes.size(); ++j) {
                const auto &invoke = deferred.invokes[j];
                std::string invokeId = invoke ? invoke->getId() : "null";
                LOG_DEBUG("StateMachine: DETAILED DEBUG - Pending invoke[{}][{}]: id='{}', type='{}'", i, j, invokeId,
                          invoke ? invoke->getType() : "null");
            }
        }

        // Copy to execute outside of lock
        invokesToExecute = std::move(pendingInvokes_);
        pendingInvokes_.clear();
        LOG_DEBUG("StateMachine: Cleared all pending invokes");
    }

    // Execute invokes outside of lock to avoid deadlock
    for (const auto &deferred : invokesToExecute) {
        // W3C SCXML Test 252: Only execute invokes if their state is still active
        if (!isStateActive(deferred.stateId)) {
            LOG_DEBUG("StateMachine: Skipping {} deferred invokes for inactive state: {}", deferred.invokes.size(),
                      deferred.stateId);
            continue;
        }

        LOG_DEBUG("StateMachine: Executing {} deferred invokes for state: {}", deferred.invokes.size(),
                  deferred.stateId);

        if (invokeExecutor_) {
            bool invokeSuccess = invokeExecutor_->executeInvokes(deferred.invokes, sessionId_);
            if (invokeSuccess) {
                LOG_DEBUG("StateMachine: Successfully executed all deferred invokes for state: {}", deferred.stateId);
            } else {
                LOG_ERROR("StateMachine: Failed to execute some deferred invokes for state: {}", deferred.stateId);
                // W3C SCXML: Continue execution even if invokes fail
            }
        } else {
            LOG_ERROR("StateMachine: Cannot execute deferred invokes - InvokeExecutor is null");
        }
    }
}

// W3C SCXML 3.7 & 5.5: Handle compound state completion when final child is entered
void StateMachine::handleCompoundStateFinalChild(const std::string &finalStateId) {
    if (!model_) {
        return;
    }

    auto finalState = model_->findStateById(finalStateId);
    if (!finalState || !finalState->isFinalState()) {
        return;
    }

    // Get parent state
    auto parent = finalState->getParent();
    if (!parent) {
        return;  // Top-level final state, no done.state event for compound
    }

    // Only generate done.state for compound (non-parallel) parent states
    if (parent->getType() == Type::PARALLEL) {
        return;  // Parallel states handled separately
    }

    // W3C SCXML 3.7: Generate done.state.{parentId} event
    std::string parentId = parent->getId();
    std::string doneEventName = "done.state." + parentId;

    LOG_INFO("W3C SCXML 3.7: Compound state '{}' completed, generating done.state event: {}", parentId, doneEventName);

    // W3C SCXML 5.5 & 5.7: Evaluate donedata and construct event data
    // If evaluation fails (error.execution raised), do not generate done.state event
    std::string eventData;
    if (!evaluateDoneData(finalStateId, eventData)) {
        LOG_DEBUG("W3C SCXML 5.7: Donedata evaluation failed, skipping done.state event generation");
        return;
    }

    // W3C SCXML: Queue the done.state event (not immediate processing)
    // This allows error.execution events from donedata evaluation to be processed first
    if (isRunning_ && eventRaiser_) {
        eventRaiser_->raiseEvent(doneEventName, eventData);
        LOG_DEBUG("W3C SCXML: Queued done.state event: {}", doneEventName);
    }
}

// Helper: Escape special characters in JSON strings
std::string StateMachine::escapeJsonString(const std::string &str) {
    std::ostringstream escaped;
    for (char c : str) {
        switch (c) {
        case '"':
            escaped << "\\\"";
            break;
        case '\\':
            escaped << "\\\\";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        case '\b':
            escaped << "\\b";
            break;
        case '\f':
            escaped << "\\f";
            break;
        default:
            escaped << c;
            break;
        }
    }
    return escaped.str();
}

// Helper: Convert ScriptValue to JSON representation
std::string StateMachine::convertScriptValueToJson(const ScriptValue &value, bool quoteStrings) {
    if (std::holds_alternative<std::string>(value)) {
        const std::string &str = std::get<std::string>(value);
        if (quoteStrings) {
            return "\"" + escapeJsonString(str) + "\"";
        }
        return str;
    } else if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    } else if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    return "null";
}

// W3C SCXML 5.5 & 5.7: Evaluate donedata and return JSON event data
// Returns false if evaluation fails (error.execution should be raised)
bool StateMachine::evaluateDoneData(const std::string &finalStateId, std::string &outEventData) {
    outEventData = "";

    if (!model_) {
        return true;  // No donedata to evaluate
    }

    auto finalState = model_->findStateById(finalStateId);
    if (!finalState) {
        return true;  // No donedata to evaluate
    }

    const auto &doneData = finalState->getDoneData();

    // Check if donedata has content
    if (!doneData.getContent().empty()) {
        // W3C SCXML 5.5: <content> sets the entire _event.data value
        std::string content = doneData.getContent();
        LOG_DEBUG("W3C SCXML 5.5: Evaluating donedata content: '{}'", content);

        // Evaluate content as expression
        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, content);
        auto result = future.get();

        if (RSM::JSEngine::isSuccess(result)) {
            // Convert result to JSON string using helper
            const auto &value = result.getInternalValue();
            outEventData = convertScriptValueToJson(value, false);

            // For objects/arrays (null case), use original content as fallback
            if (outEventData == "null" && !std::holds_alternative<ScriptNull>(value)) {
                outEventData = content;
            }
            return true;
        } else {
            LOG_WARN("W3C SCXML 5.5: Failed to evaluate donedata content: {}", result.getErrorMessage());
            outEventData = content;  // Use literal content as fallback
            return true;
        }
    }

    // Check if donedata has params
    const auto &params = doneData.getParams();
    if (!params.empty()) {
        // W3C SCXML 5.5: <param> elements create an object with name:value pairs
        LOG_DEBUG("W3C SCXML 5.5: Evaluating {} donedata params", params.size());

        std::ostringstream jsonBuilder;
        jsonBuilder << "{";

        bool first = true;
        for (const auto &param : params) {
            if (!first) {
                jsonBuilder << ",";
            }
            first = false;

            const std::string &paramName = param.first;
            const std::string &paramExpr = param.second;

            // W3C SCXML 5.7: Empty location is invalid
            if (paramExpr.empty()) {
                LOG_ERROR("W3C SCXML 5.7: Empty param location/expression for param '{}'", paramName);

                if (eventRaiser_) {
                    eventRaiser_->raiseEvent("error.execution", "Empty param location or expression: " + paramName);
                }

                // W3C SCXML 5.7: Return false to skip done.state event generation
                return false;
            }

            // Evaluate param expression
            auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, paramExpr);
            auto result = future.get();

            if (RSM::JSEngine::isSuccess(result)) {
                const auto &value = result.getInternalValue();
                // Use helper to convert value to JSON with proper escaping
                jsonBuilder << "\"" << escapeJsonString(paramName) << "\":" << convertScriptValueToJson(value, true);
            } else {
                // W3C SCXML 5.7: Invalid location or expression error must generate error.execution
                LOG_ERROR("W3C SCXML 5.7: Failed to evaluate param '{}' expr/location '{}': {}", paramName, paramExpr,
                          result.getErrorMessage());

                if (eventRaiser_) {
                    eventRaiser_->raiseEvent("error.execution",
                                             "Invalid param location or expression: " + paramName + " = " + paramExpr);
                }

                // W3C SCXML 5.7: Return false to skip done.state event generation
                return false;
            }
        }

        jsonBuilder << "}";
        outEventData = jsonBuilder.str();
        return true;
    }

    // No donedata
    return true;
}

}  // namespace RSM
