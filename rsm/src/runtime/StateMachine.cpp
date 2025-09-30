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
#include "states/ConcurrentStateNode.h"
#include <fstream>
#include <random>
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

    LOG_DEBUG("StateMachine: Processing event: '{}' with data: '{}' in session: '{}'", eventName, eventData,
              sessionId_);

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

        // SCXML W3C specification: If no transition on parallel state, broadcast to all active regions
        LOG_DEBUG("StateMachine: No transitions on parallel state, broadcasting to all regions");

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
            // For normal event processing, support wildcard (*) and exact matching
            eventMatches = (transitionEvent == eventName) || (transitionEvent == "*");
        }

        if (!eventMatches) {
            continue;
        }

        const auto &targets = transitionNode->getTargets();
        if (targets.empty()) {
            LOG_DEBUG("StateMachine: Skipping transition with no targets");
            continue;
        }

        std::string targetState = targets[0];
        std::string condition = transitionNode->getGuard();

        LOG_DEBUG("Checking transition: {} -> {} with condition: '{}' (event: '{}')", stateNode->getId(), targetState,
                  condition, transitionEvent);

        bool conditionResult = condition.empty() || evaluateCondition(condition);
        LOG_DEBUG("Condition result: {}", conditionResult ? "true" : "false");

        if (conditionResult) {
            std::string fromState = getCurrentState();  // Save the original state
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
            const auto &actionNodes = transitionNode->getActionNodes();
            if (!actionNodes.empty()) {
                LOG_DEBUG("StateMachine: Executing transition actions in session: '{}'", sessionId_);
                for (const auto &actionNode : actionNodes) {
                    if (actionNode && actionNode->execute(*executionContext_)) {
                        LOG_DEBUG("Successfully executed ActionNode: {} in session: '{}'", actionNode->getActionType(),
                                  sessionId_);
                    } else {
                        LOG_WARN("ActionNode execution failed: {}, but continuing",
                                 actionNode ? actionNode->getActionType() : "null");
                    }
                }
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

            // W3C SCXML compliance: Execute deferred invokes after transition completes
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

StateMachine::Statistics StateMachine::getStatistics() const {
    return stats_;
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

    // SCXML W3C compliant: Provide ExecutionContext to parallel states for action execution
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && stateNode->getType() == Type::PARALLEL && executionContext_) {
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
            if (parallelState) {
                parallelState->setExecutionContextForRegions(executionContext_);
                LOG_DEBUG("SCXML compliant: Injected ExecutionContext into parallel state regions: {}", stateId);
            }
        }
    }

    bool hierarchyResult = hierarchyManager_->enterState(stateId);
    assert(hierarchyResult && "SCXML violation: state entry must succeed");
    (void)hierarchyResult;  // Suppress unused variable warning in release builds

    // SCXML W3C macrostep compliance: Check if reentrant transition occurred during state entry
    // This handles cases where done.state events cause immediate transitions (e.g., parallel state completion)
    std::string actualCurrentState = getCurrentState();
    if (actualCurrentState != stateId) {
        LOG_DEBUG("SCXML macrostep: State transition occurred during entry (expected: {}, actual: {})", stateId,
                  actualCurrentState);
        LOG_DEBUG("This indicates a valid internal transition (e.g., done.state event) - macrostep continuing");

        // Note: Invokes are executed immediately during state entry        // Clear guard flag - macrostep will
        // complete with the actual final state
        isEnteringState_ = false;
        return true;
    }

    // W3C SCXML: onentry actions (including invokes) are executed via callback from StateHierarchyManager
    // This ensures proper execution order per W3C specification

    // Set state variable in JavaScript context
    RSM::JSEngine::instance().setVariable(sessionId_, "_state", ScriptValue{getCurrentState()});

    LOG_DEBUG("Successfully entered state using hierarchy manager: {} (current: {})", stateId, getCurrentState());

    // Clear guard flag before checking automatic transitions
    isEnteringState_ = false;

    // SCXML W3C specification: After entering a state, check for automatic transitions (eventless transitions)
    if (model_) {
        auto currentStateNode = model_->findStateById(stateId);
        if (currentStateNode) {
            const auto &transitions = currentStateNode->getTransitions();
            bool hasAutomaticTransitions = false;

            // Check if there are any eventless transitions
            for (const auto &transition : transitions) {
                if (transition->getEvent().empty()) {
                    hasAutomaticTransitions = true;
                    break;
                }
            }

            if (hasAutomaticTransitions) {
                LOG_DEBUG("State {} has automatic transitions, checking conditions...", stateId);

                // Process automatic transitions (with empty event name)
                auto transitionResult = processStateTransitions(currentStateNode, "", "");
                if (transitionResult.success) {
                    LOG_INFO("Automatic transition executed: {} -> {}", transitionResult.fromState,
                             transitionResult.toState);

                    // Invoke execution consolidated to key lifecycle points
                } else {
                    LOG_DEBUG("No automatic transition conditions met for state: {}", stateId);
                }
            } else {
                LOG_DEBUG("State {} has no automatic transitions", stateId);
            }
        }
    }

    return true;
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
        assert(exitResult && "SCXML violation: parallel state exit actions must succeed");
        (void)exitResult;  // Suppress unused variable warning in release builds
    } else {
        // Execute IActionNode-based exit actions for non-parallel states
        bool exitResult = executeExitActions(stateId);
        assert(exitResult && "SCXML violation: state exit actions must succeed");
        (void)exitResult;  // Suppress unused variable warning in release builds
    }

    // Record history before exiting compound states (SCXML W3C specification section 3.6)
    if (historyManager_ && hierarchyManager_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && (stateNode->getType() == Type::COMPOUND || stateNode->getType() == Type::PARALLEL)) {
            // Get current active states before exiting
            auto activeStates = hierarchyManager_->getActiveStates();

            // Record history for this compound state
            bool recorded = historyManager_->recordHistory(stateId, activeStates);
            if (recorded) {
                LOG_DEBUG("Recorded history for compound state: {}", stateId);
            }
        }
    }

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

    // Initialize data model variables from SCXML model
    if (model_) {
        const auto &dataModelItems = model_->getDataModelItems();
        LOG_DEBUG("StateMachine: Found {} data model items to initialize", dataModelItems.size());

        for (const auto &item : dataModelItems) {
            std::string id = item->getId();
            std::string expr = item->getExpr();
            std::string content = item->getContent();

            LOG_DEBUG("StateMachine: Processing data model item '{}' - expr: '{}', content: '{}'", id, expr, content);

            if (!expr.empty()) {
                LOG_DEBUG("StateMachine: Evaluating expression '{}' for variable '{}'", expr, id);
                // Execute the expression to get initial value
                auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, expr);
                auto result = future.get();

                if (RSM::JSEngine::isSuccess(result)) {
                    // Direct access to value through friend class access
                    RSM::JSEngine::instance().setVariable(sessionId_, id, result.getInternalValue());
                    LOG_DEBUG("StateMachine: Initialized data model variable '{}' from expression '{}' with result", id,
                              expr);
                } else {
                    LOG_ERROR("StateMachine: Failed to evaluate expression '{}' for variable '{}': {}", expr, id,
                              result.getErrorMessage());
                    // Default to 0 for numeric expressions
                    RSM::JSEngine::instance().setVariable(sessionId_, id, ScriptValue{static_cast<long>(0)});
                    LOG_DEBUG("StateMachine: Set default value for data model variable '{}'", id);
                }
            } else if (!content.empty()) {
                LOG_DEBUG("StateMachine: Evaluating content '{}' for variable '{}'", content, id);
                // Use content as expression
                auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, content);
                auto result = future.get();

                if (RSM::JSEngine::isSuccess(result)) {
                    RSM::JSEngine::instance().setVariable(sessionId_, id, result.getInternalValue());
                    LOG_DEBUG("StateMachine: Initialized data model variable '{}' from content '{}' with result", id,
                              content);
                } else {
                    LOG_ERROR("StateMachine: Failed to evaluate content '{}' for variable '{}': {}", content, id,
                              result.getErrorMessage());
                    // Try setting content as string literal
                    RSM::JSEngine::instance().setVariable(sessionId_, id, ScriptValue{content});
                    LOG_DEBUG("StateMachine: Set data model variable '{}' as string literal from content", id);
                }
            } else {
                LOG_DEBUG("StateMachine: No expression or content for variable '{}', skipping", id);
            }
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

bool StateMachine::executeActionNodes(const std::vector<std::shared_ptr<RSM::IActionNode>> &actions) {
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

        // SCXML W3C specification: Execute child region exit actions FIRST
        for (const auto &region : regions) {
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
                            if (!executeActionNodes(childExitActions)) {
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
                    if (!executeActionNodes(regionExitActions)) {
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
            return executeActionNodes(parallelExitActions);
        }

        return true;
    }

    // Execute IActionNode-based exit actions for non-parallel states
    const auto &exitActions = stateNode->getExitActionNodes();
    if (!exitActions.empty()) {
        LOG_DEBUG("Executing {} exit action nodes for state: {}", exitActions.size(), stateId);
        return executeActionNodes(exitActions);
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
    }
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
    LOG_DEBUG("StateMachine: DETAILED DEBUG - deferInvokeExecution called for session {}, stateId: {}, invokeCount: {}",
              sessionId_, stateId, invokes.size());

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
    // Thread-safe copy of pending invokes
    std::vector<DeferredInvoke> invokesToExecute;
    {
        std::lock_guard<std::mutex> lock(pendingInvokesMutex_);
        if (pendingInvokes_.empty()) {
            LOG_DEBUG("StateMachine: No pending invokes to execute");
            return;
        }

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

}  // namespace RSM
