#include "runtime/StateMachine.h"
#include "common/Logger.h"
#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"
#include "parsing/XIncludeProcessor.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/ExecutionContextImpl.h"
#include "scripting/JSEngine.h"
#include "states/ConcurrentStateNode.h"
#include <fstream>
#include <random>
#include <sstream>

namespace RSM {

StateMachine::StateMachine() : isRunning_(false), jsEnvironmentReady_(false) {
    sessionId_ = generateSessionId();
    // JS 환경은 지연 초기화로 변경
    // ActionExecutor와 ExecutionContext는 setupJSEnvironment에서 초기화
}

StateMachine::~StateMachine() {
    if (isRunning_) {
        stop();
    }
    // JS 환경이 초기화된 경우에만 세션 정리
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
            Logger::error("StateMachine: Failed to parse SCXML file: " + filename);
            return false;
        }

        return initializeFromModel();
    } catch (const std::exception &e) {
        Logger::error("StateMachine: Exception loading SCXML: " + std::string(e.what()));
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
            Logger::error("StateMachine: Failed to parse SCXML content");
            return false;
        }

        return initializeFromModel();
    } catch (const std::exception &e) {
        Logger::error("StateMachine: Exception parsing SCXML content: " + std::string(e.what()));
        return false;
    }
}

bool StateMachine::start() {
    if (initialState_.empty()) {
        Logger::error("StateMachine: Cannot start - no initial state defined");
        return false;
    }

    // JS 환경 초기화 보장
    if (!ensureJSEnvironment()) {
        Logger::error("StateMachine: Cannot start - JavaScript environment initialization failed");
        return false;
    }

    Logger::debug("StateMachine: Starting with initial state: " + initialState_);

    // Set running state before entering initial state to handle immediate done.state events
    isRunning_ = true;

    if (!enterState(initialState_)) {
        Logger::error("StateMachine: Failed to enter initial state: " + initialState_);
        isRunning_ = false;  // Rollback on failure
        return false;
    }
    updateStatistics();

    Logger::info("StateMachine: Started successfully");
    return true;
}

void StateMachine::stop() {
    if (!isRunning_) {
        return;
    }

    Logger::debug("StateMachine: Stopping state machine");

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
    Logger::debug("StateMachine: Unregistered from JSEngine");

    updateStatistics();
    Logger::info("StateMachine: Stopped");
}

StateMachine::TransitionResult StateMachine::processEvent(const std::string &eventName, const std::string &eventData) {
    if (!isRunning_) {
        Logger::warn("StateMachine: Cannot process event - state machine not running");
        TransitionResult result;
        result.success = false;
        result.errorMessage = "State machine not running";
        return result;
    }

    // JS 환경 확인
    if (!jsEnvironmentReady_) {
        Logger::error("StateMachine: Cannot process event - JavaScript environment not ready");
        TransitionResult result;
        result.success = false;
        result.errorMessage = "JavaScript environment not ready";
        return result;
    }

    Logger::debug("StateMachine: Processing event: " + eventName);

    // Count this event
    stats_.totalEvents++;

    // Store event data for access in guards/actions
    currentEventData_ = eventData;

    // Set event data in JavaScript context
    RSM::JSEngine::instance().setVariable(sessionId_, "_event", ScriptValue{eventData});
    RSM::JSEngine::instance().setVariable(sessionId_, "_eventname", ScriptValue{eventName});

    // Find applicable transitions from SCXML model
    if (!model_) {
        Logger::error("StateMachine: No SCXML model available");
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
        Logger::debug("StateMachine: Current state not found in model: " + currentState);
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

        Logger::debug("StateMachine: Processing event '" + eventName + "' for parallel state: " + currentState);

        // SCXML W3C specification: First check transitions on the parallel state itself
        // This follows the same transition evaluation logic as non-parallel states
        auto stateTransitionResult = processStateTransitions(currentStateNode, eventName, eventData);
        if (stateTransitionResult.success) {
            Logger::debug("StateMachine: SCXML compliant - parallel state transition executed: " +
                          stateTransitionResult.fromState + " -> " + stateTransitionResult.toState);
            return stateTransitionResult;
        }

        // SCXML W3C specification: If no transition on parallel state, broadcast to all active regions
        Logger::debug("StateMachine: No transitions on parallel state, broadcasting to all regions");

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
            Logger::info("StateMachine: SCXML compliant parallel region processing succeeded. Transitions: [" +
                         std::to_string(successfulTransitions.size()) + "/" + std::to_string(results.size()) + "]");

            // Return success with parallel state as context
            TransitionResult finalResult;
            finalResult.success = true;
            finalResult.fromState = currentState;
            finalResult.toState = currentState;  // Parallel state remains active
            finalResult.eventName = eventName;
            return finalResult;
        } else {
            Logger::debug("StateMachine: No transitions executed in any region for event: " + eventName);
            stats_.failedTransitions++;
            TransitionResult result;
            result.success = false;
            result.fromState = getCurrentState();
            result.eventName = eventName;
            result.errorMessage = "No valid transitions found";
            return result;
        }
    }

    // Non-parallel state: process state transitions using unified logic
    return processStateTransitions(currentStateNode, eventName, eventData);

    // This should not be reached, but just in case
    TransitionResult result;
    result.success = false;
    result.fromState = getCurrentState();
    result.eventName = eventName;
    result.errorMessage = "Unexpected end of transition processing";
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

    Logger::debug("StateMachine: Checking " + std::to_string(transitions.size()) + " transitions for event '" +
                  eventName + "' on state: " + stateNode->getId());

    // Execute first valid transition (SCXML W3C specification)
    for (const auto &transitionNode : transitions) {
        // Check if this transition matches the event
        if (transitionNode->getEvent() != eventName) {
            continue;
        }

        const auto &targets = transitionNode->getTargets();
        if (targets.empty()) {
            Logger::debug("StateMachine: Skipping transition with no targets");
            continue;
        }

        std::string targetState = targets[0];
        std::string condition = transitionNode->getGuard();

        Logger::debug("StateMachine: Checking transition: " + stateNode->getId() + " -> " + targetState +
                      " with condition: '" + condition + "'");

        bool conditionResult = condition.empty() || evaluateCondition(condition);
        Logger::debug("StateMachine: Condition result: " + std::string(conditionResult ? "true" : "false"));

        if (conditionResult) {
            std::string fromState = getCurrentState();  // Save the original state
            Logger::debug("StateMachine: Executing SCXML compliant transition from " + fromState + " to " +
                          targetState);

            // Exit current state
            if (!exitState(fromState)) {
                Logger::error("StateMachine: Failed to exit state: " + fromState);
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
                Logger::debug("StateMachine: Executing transition actions");
                for (const auto &actionNode : actionNodes) {
                    if (actionNode && actionNode->execute(*executionContext_)) {
                        Logger::debug("StateMachine: Successfully executed ActionNode: " + actionNode->getActionType());
                    } else {
                        Logger::warn("StateMachine: ActionNode execution failed: " +
                                     (actionNode ? actionNode->getActionType() : "null") + ", but continuing");
                    }
                }
            } else {
                Logger::debug("StateMachine: No transition actions for this transition");
            }

            // Enter new state
            if (!enterState(targetState)) {
                Logger::error("StateMachine: Failed to enter state: " + targetState);
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

            Logger::info("StateMachine: Successfully transitioned from " + fromState + " to " + targetState);
            return TransitionResult(true, fromState, targetState, eventName);
        }
    }

    // No valid transitions found
    Logger::debug("StateMachine: No valid transitions found for event: " + eventName +
                  " from state: " + stateNode->getId());

    // CRITICAL: Increment failed transitions counter for statistics
    stats_.failedTransitions++;
    Logger::debug("StateMachine: Incremented failedTransitions counter to: " +
                  std::to_string(stats_.failedTransitions));

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

std::string StateMachine::getCurrentEventData() const {
    return currentEventData_;
}

const std::string &StateMachine::getSessionId() const {
    return sessionId_;
}

StateMachine::Statistics StateMachine::getStatistics() const {
    return stats_;
}

std::string StateMachine::generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    return "sm_" + std::to_string(dis(gen));
}

bool StateMachine::initializeFromModel() {
    Logger::debug("StateMachine: Initializing from SCXML model");

    // Clear existing state
    initialState_.clear();

    // Get initial state
    initialState_ = model_->getInitialState();
    if (initialState_.empty()) {
        Logger::error("StateMachine: No initial state defined in SCXML model");
        return false;
    }

    // Extract all states from the model
    const auto &allStates = model_->getAllStates();
    if (allStates.empty()) {
        Logger::error("StateMachine: No states found in SCXML model");
        return false;
    }

    try {
        // Initialize hierarchy manager for hierarchical state support
        hierarchyManager_ = std::make_unique<StateHierarchyManager>(model_);

        // Set up completion callbacks for parallel states (SCXML W3C compliance)
        setupParallelStateCallbacks();

        Logger::debug("StateMachine: Model initialized with initial state: " + initialState_);
        Logger::info("StateMachine: Model initialized with " + std::to_string(allStates.size()) + " states");
        return true;
    } catch (const std::exception &e) {
        Logger::error("StateMachine: Failed to extract model: " + std::string(e.what()));
        return false;
    }
}

bool StateMachine::evaluateCondition(const std::string &condition) {
    if (condition.empty()) {
        return true;
    }

    try {
        // Debug: Check counter value before evaluating condition
        auto counterCheck = RSM::JSEngine::instance().evaluateExpression(sessionId_, "counter").get();
        if (RSM::JSEngine::isSuccess(counterCheck)) {
            auto counterValue = RSM::JSEngine::resultToValue<double>(counterCheck);
            if (counterValue.has_value()) {
                Logger::debug("StateMachine: Current counter value before condition '" + condition +
                              "': " + std::to_string(counterValue.value()));
            }
        }

        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, condition);
        auto result = future.get();

        if (!RSM::JSEngine::isSuccess(result)) {
            Logger::error("StateMachine: Failed to evaluate condition: evaluation failed");
            return false;
        }

        // Convert result to boolean using integrated JSEngine method
        return RSM::JSEngine::resultToBool(result);

        return false;
    } catch (const std::exception &e) {
        Logger::error("StateMachine: Exception evaluating condition: " + std::string(e.what()));
        return false;
    }
}

bool StateMachine::enterState(const std::string &stateId) {
    Logger::debug("StateMachine: Entering state: " + stateId);

    // SCXML W3C specification: hierarchy manager is required for compliant state entry
    assert(hierarchyManager_ && "SCXML violation: hierarchy manager required for state management");

    bool hierarchyResult = hierarchyManager_->enterState(stateId);
    assert(hierarchyResult && "SCXML violation: state entry must succeed");
    (void)hierarchyResult;  // Suppress unused variable warning in release builds

    // SCXML W3C specification section 3.4: Execute entry actions in correct order for parallel states
    auto stateNode = model_->findStateById(stateId);
    assert(stateNode && "SCXML violation: state must exist in model");

    if (stateNode->getType() == Type::PARALLEL) {
        // SCXML W3C section 3.4: Execute parallel state entry in proper phases
        Logger::debug("StateMachine: Executing parallel state entry in proper phases");

        // Phase 1: Execute entry actions (parallel state first, then regions)
        bool parallelEntryResult = executeEntryActions(stateId);
        assert(parallelEntryResult && "SCXML violation: parallel state entry actions must succeed");
        (void)parallelEntryResult;

        // Phase 2: Execute entry actions for all child regions
        auto activeStates = getActiveStates();
        for (const auto &activeState : activeStates) {
            if (activeState != stateId) {  // Skip the parallel state itself (already executed)
                bool childEntryResult = executeEntryActions(activeState);
                assert(childEntryResult && "SCXML violation: child region entry actions must succeed");
                (void)childEntryResult;
            }
        }

        // Parallel state entry phases completed
        Logger::debug("StateMachine: Parallel state entry phases completed");

        // SCXML W3C specification: Check for completion after initial entry
        // If all regions are immediately in final states, generate done.state event
        auto parallelState = static_cast<ConcurrentStateNode *>(stateNode);
        parallelState->areAllRegionsComplete();  // Completion check after initial entry
    } else {
        // For non-parallel states: Execute entry actions for all entered states
        auto activeStates = getActiveStates();
        for (const auto &activeState : activeStates) {
            bool entryResult = executeEntryActions(activeState);
            assert(entryResult && "SCXML violation: state entry actions must succeed");
            (void)entryResult;  // Suppress unused variable warning in release builds
        }
    }

    // Set state variable in JavaScript context
    RSM::JSEngine::instance().setVariable(sessionId_, "_state", ScriptValue{getCurrentState()});

    Logger::debug("StateMachine: Successfully entered state using hierarchy manager: " + stateId +
                  " (current: " + getCurrentState() + ")");
    return true;
}

bool StateMachine::exitState(const std::string &stateId) {
    Logger::debug("StateMachine: Exiting state: " + stateId);

    // SCXML W3C specification section 3.4: Execute exit actions in correct order for parallel states
    auto stateNode = model_->findStateById(stateId);
    if (stateNode && stateNode->getType() == Type::PARALLEL) {
        // For parallel states: Child regions exit FIRST, then parallel state exits
        Logger::debug("StateMachine: SCXML W3C compliant - executing parallel state exit actions in correct order");

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

    // Use hierarchy manager for SCXML-compliant state exit
    assert(hierarchyManager_ && "SCXML violation: hierarchy manager required for state management");
    hierarchyManager_->exitState(stateId);

    // State management fully delegated to StateHierarchyManager

    Logger::debug("StateMachine: Successfully exited state: " + stateId);
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
    Logger::debug("StateMachine: JSEngine automatically initialized via RAII at address: {}",
                  static_cast<void *>(&jsEngine));

    // Create JavaScript session
    if (!RSM::JSEngine::instance().createSession(sessionId_)) {
        Logger::error("StateMachine: Failed to create JavaScript session");
        return false;
    }

    // Set up basic variables
    RSM::JSEngine::instance().setVariable(sessionId_, "_sessionid", ScriptValue{sessionId_});
    RSM::JSEngine::instance().setVariable(sessionId_, "_name", ScriptValue{std::string("StateMachine")});

    // Register this StateMachine instance with JSEngine for In() function support
    RSM::JSEngine::instance().setStateMachine(this, sessionId_);
    Logger::debug("StateMachine: Registered with JSEngine for In() function support");

    // Initialize data model variables from SCXML model
    if (model_) {
        const auto &dataModelItems = model_->getDataModelItems();
        for (const auto &item : dataModelItems) {
            std::string id = item->getId();
            std::string expr = item->getExpr();

            if (!expr.empty()) {
                // Execute the expression to get initial value
                auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, expr);
                auto result = future.get();

                if (RSM::JSEngine::isSuccess(result)) {
                    // Direct access to value through friend class access
                    RSM::JSEngine::instance().setVariable(sessionId_, id, result.getInternalValue());
                    Logger::debug("StateMachine: Initialized data model variable: " + id + " = " + expr);
                } else {
                    // Default to 0 for numeric expressions
                    RSM::JSEngine::instance().setVariable(sessionId_, id, ScriptValue{static_cast<long>(0)});
                    Logger::debug("StateMachine: Set default value for data model variable: " + id);
                }
            }
        }
    }

    // Initialize ActionExecutor and ExecutionContext
    if (!initializeActionExecutor()) {
        Logger::error("StateMachine: Failed to initialize action executor");
        return false;
    }

    jsEnvironmentReady_ = true;
    Logger::debug("StateMachine: JavaScript environment setup completed");
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

        // Create ExecutionContext with shared_ptr and sessionId
        executionContext_ = std::make_shared<ExecutionContextImpl>(actionExecutor_, sessionId_);

        Logger::debug("StateMachine: ActionExecutor and ExecutionContext initialized for session: " + sessionId_);
        return true;
    } catch (const std::exception &e) {
        Logger::error("StateMachine: Failed to initialize ActionExecutor: " + std::string(e.what()));
        return false;
    }
}

bool StateMachine::executeActionNodes(const std::vector<std::shared_ptr<RSM::IActionNode>> &actions) {
    if (!executionContext_) {
        Logger::warn("StateMachine: ExecutionContext not initialized, skipping action node execution");
        return true;  // Not a failure, just no actions to execute
    }

    bool anySuccess = false;
    bool allSucceeded = true;

    for (const auto &action : actions) {
        if (!action) {
            Logger::warn("StateMachine: Null action node encountered, skipping");
            continue;
        }

        try {
            Logger::debug("StateMachine: Executing action: " + action->getActionType());
            if (action->execute(*executionContext_)) {
                Logger::debug("StateMachine: Successfully executed action: " + action->getActionType());
                anySuccess = true;
            } else {
                Logger::warn("StateMachine: Failed to execute action: " + action->getActionType() +
                             " (continuing with remaining actions)");
                allSucceeded = false;
            }
        } catch (const std::exception &e) {
            Logger::warn("StateMachine: Exception executing action " + action->getActionType() + ": " +
                         std::string(e.what()) + " (continuing with remaining actions)");
            allSucceeded = false;
        }
    }

    // Return true if we had no actions or if at least some actions succeeded
    // This allows the state machine to continue even if some actions fail
    return actions.empty() || anySuccess || allSucceeded;
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

    Logger::debug("StateMachine: Executing entry actions for state: " + stateId);

    // SCXML W3C specification section 3.4: Parallel states require special handling
    if (stateNode->getType() == Type::PARALLEL) {
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        // SCXML W3C specification section 3.4: Execute parallel state's own onentry actions FIRST
        const auto &parallelEntryActions = parallelState->getEntryActionNodes();
        if (!parallelEntryActions.empty()) {
            Logger::debug("StateMachine: SCXML W3C compliant - executing " +
                          std::to_string(parallelEntryActions.size()) +
                          " entry actions for parallel state itself: " + stateId);
            if (!executeActionNodes(parallelEntryActions)) {
                Logger::error("StateMachine: Failed to execute parallel state entry actions for: " + stateId);
                return false;
            }
        }

        // provide ExecutionContext to all regions for action execution
        if (executionContext_) {
            parallelState->setExecutionContextForRegions(executionContext_);
            Logger::debug("StateMachine: Injected ExecutionContext into all regions of parallel state: " + stateId);
        }

        // SCXML W3C specification: ALL child regions MUST have their entry actions executed AFTER parallel state
        const auto &regions = parallelState->getRegions();
        assert(!regions.empty() && "SCXML violation: parallel state must have at least one region");

        Logger::debug("StateMachine: SCXML W3C compliant - executing entry actions for " +
                      std::to_string(regions.size()) + " child regions in parallel state: " + stateId);

        // Execute entry actions for each region's root state
        for (const auto &region : regions) {
            assert(region && "SCXML violation: parallel state cannot have null regions");

            auto rootState = region->getRootState();
            assert(rootState && "SCXML violation: region must have root state");

            // Execute entry actions for the region's root state
            const auto &regionEntryActions = rootState->getEntryActionNodes();
            if (!regionEntryActions.empty()) {
                Logger::debug("StateMachine: Executing " + std::to_string(regionEntryActions.size()) +
                              " entry actions for region: " + region->getId());
                if (!executeActionNodes(regionEntryActions)) {
                    Logger::error("StateMachine: Failed to execute entry actions for region: " + region->getId());
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

                    Logger::debug("StateMachine: Entering initial child state for INACTIVE region " + region->getId() +
                                  ": " + initialChild);

                    // Execute entry actions for the initial child state
                    auto childState = model_->findStateById(initialChild);
                    if (childState) {
                        const auto &childEntryActions = childState->getEntryActionNodes();
                        if (!childEntryActions.empty()) {
                            Logger::debug("StateMachine: Executing " + std::to_string(childEntryActions.size()) +
                                          " entry actions for initial child state: " + initialChild);
                            if (!executeActionNodes(childEntryActions)) {
                                Logger::error(
                                    "StateMachine: Failed to execute entry actions for initial child state: " +
                                    initialChild);
                                return false;
                            }
                        }
                    }
                } else {
                    // SCXML W3C 사양 준수: 이미 활성화된 영역은 초기 상태로 재진입하지 않음
                    auto concreteRegion = std::dynamic_pointer_cast<ConcurrentRegion>(region);
                    std::string currentState = concreteRegion ? concreteRegion->getCurrentState() : "unknown";

                    Logger::debug("StateMachine: SCXML W3C compliance - skipping initial state entry for already "
                                  "ACTIVE region: " +
                                  region->getId() + " (current state: " + currentState + ")");

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
        Logger::debug("StateMachine: Executing " + std::to_string(entryActions.size()) +
                      " entry action nodes for state: " + stateId);
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
        Logger::debug("StateMachine: State " + stateId + " not found in SCXML model, skipping exit actions");
        return true;  // Not an error if state not found in model
    }

    // SCXML W3C specification section 3.4: Parallel states require special exit sequence
    if (stateNode->getType() == Type::PARALLEL) {
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        const auto &regions = parallelState->getRegions();
        assert(!regions.empty() && "SCXML violation: parallel state must have at least one region");

        Logger::debug("StateMachine: SCXML W3C compliant - executing exit sequence for parallel state: " + stateId);

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
                            Logger::debug("StateMachine: SCXML W3C compliant - executing " +
                                          std::to_string(childExitActions.size()) +
                                          " exit actions for active child state: " + child->getId());
                            if (!executeActionNodes(childExitActions)) {
                                Logger::error("StateMachine: Failed to execute exit actions for child state: " +
                                              child->getId());
                                return false;
                            }
                        }
                        break;
                    }
                }

                // Execute exit actions for the region's root state
                const auto &regionExitActions = rootState->getExitActionNodes();
                if (!regionExitActions.empty()) {
                    Logger::debug("StateMachine: SCXML W3C compliant - executing " +
                                  std::to_string(regionExitActions.size()) +
                                  " exit actions for region: " + region->getId());
                    if (!executeActionNodes(regionExitActions)) {
                        Logger::error("StateMachine: Failed to execute exit actions for region: " + region->getId());
                        return false;
                    }
                }
            }
        }

        // SCXML W3C specification: Execute parallel state's own onexit actions LAST
        const auto &parallelExitActions = parallelState->getExitActionNodes();
        if (!parallelExitActions.empty()) {
            Logger::debug("StateMachine: SCXML W3C compliant - executing " +
                          std::to_string(parallelExitActions.size()) +
                          " exit actions for parallel state itself: " + stateId);
            return executeActionNodes(parallelExitActions);
        }

        return true;
    }

    // Execute IActionNode-based exit actions for non-parallel states
    const auto &exitActions = stateNode->getExitActionNodes();
    if (!exitActions.empty()) {
        Logger::debug("StateMachine: Executing " + std::to_string(exitActions.size()) +
                      " exit action nodes for state: " + stateId);
        return executeActionNodes(exitActions);
    }

    return true;
}

void StateMachine::handleParallelStateCompletion(const std::string &stateId) {
    Logger::debug("StateMachine: Handling parallel state completion for: " + stateId);

    // Generate done.state.{stateId} event according to SCXML W3C specification section 3.4
    std::string doneEventName = "done.state." + stateId;

    Logger::info("StateMachine: Generating done.state event: " + doneEventName +
                 " for completed parallel state: " + stateId);

    // Process the done.state event to trigger any transitions waiting for it
    if (isRunning_) {
        auto result = processEvent(doneEventName, "");
        if (result.success) {
            Logger::debug("StateMachine: Successfully processed done.state event: " + doneEventName);
        } else {
            Logger::debug("StateMachine: No transitions found for done.state event: " + doneEventName +
                          " (this is normal if no transitions are waiting for this event)");
        }
    } else {
        Logger::warn("StateMachine: Cannot process done.state event " + doneEventName +
                     " - state machine is not running");
    }
}

void StateMachine::setupParallelStateCallbacks() {
    if (!model_) {
        Logger::warn("StateMachine: Cannot setup parallel state callbacks - no model available");
        return;
    }

    Logger::debug("StateMachine: Setting up completion callbacks for parallel states");

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
                Logger::debug("StateMachine: Set up completion callback for parallel state: " + state->getId());
            } else {
                Logger::warn("StateMachine: Found parallel state that is not a ConcurrentStateNode: " + state->getId());
            }
        }
    }

    Logger::info("StateMachine: Set up completion callbacks for " + std::to_string(parallelStateCount) +
                 " parallel states");
}

}  // namespace RSM
