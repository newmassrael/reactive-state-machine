#include "runtime/StateMachine.h"
#include "common/Logger.h"
#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"
#include "parsing/XIncludeProcessor.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/ExecutionContextImpl.h"
#include "scripting/JSEngine.h"
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

    if (!enterState(initialState_)) {
        Logger::error("StateMachine: Failed to enter initial state: " + initialState_);
        return false;
    }

    isRunning_ = true;
    updateStatistics();

    Logger::info("StateMachine: Started successfully");
    return true;
}

void StateMachine::stop() {
    if (!isRunning_) {
        return;
    }

    Logger::debug("StateMachine: Stopping state machine");

    if (!currentState_.empty()) {
        exitState(currentState_);
    }

    isRunning_ = false;
    currentState_.clear();
    activeStates_.clear();

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
        result.fromState = currentState_;
        result.eventName = eventName;
        result.errorMessage = "No SCXML model available";
        return result;
    }

    auto currentStateNode = model_->findStateById(currentState_);
    if (!currentStateNode) {
        Logger::debug("StateMachine: Current state not found in model: " + currentState_);
        TransitionResult result;
        result.success = false;
        result.fromState = currentState_;
        result.eventName = eventName;
        result.errorMessage = "Current state not found in model";
        return result;
    }

    const auto &transitions = currentStateNode->getTransitions();
    bool transitionFound = false;

    // Execute first valid transition
    for (const auto &transitionNode : transitions) {
        // Check if this transition matches the event
        if (transitionNode->getEvent() != eventName) {
            continue;
        }

        const auto &targets = transitionNode->getTargets();
        if (targets.empty()) {
            continue;
        }

        std::string targetState = targets[0];
        std::string condition = transitionNode->getGuard();

        Logger::debug("StateMachine: Checking transition: " + currentState_ + " -> " + targetState +
                      " with condition: '" + condition + "'");
        bool conditionResult = condition.empty() || evaluateCondition(condition);
        Logger::debug("StateMachine: Condition result: " + std::string(conditionResult ? "true" : "false"));

        if (conditionResult) {
            std::string fromState = currentState_;  // Save the original state
            Logger::debug("StateMachine: Executing transition from " + fromState + " to " + targetState);
            transitionFound = true;

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

            // Execute transition actions
            const auto &actions = transitionNode->getActions();
            if (!actions.empty()) {
                Logger::debug("StateMachine: Executing transition actions");
                for (const auto &action : actions) {
                    if (!executeAction(action)) {
                        Logger::warn("StateMachine: Transition action failed: " + action + ", but continuing");
                    }
                }
            } else {
                Logger::debug("StateMachine: No transition action for this transition");
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

    if (!transitionFound) {
        Logger::debug("StateMachine: No valid transitions found for event: " + eventName +
                      " from state: " + currentState_);
        stats_.failedTransitions++;
        TransitionResult result;
        result.success = false;
        result.fromState = currentState_;
        result.eventName = eventName;
        result.errorMessage = "No valid transitions found";
        return result;
    }

    // This should not be reached, but just in case
    TransitionResult result;
    result.success = false;
    result.fromState = currentState_;
    result.eventName = eventName;
    result.errorMessage = "Unexpected end of transition processing";
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
    return currentState_ == stateId;
}

std::string StateMachine::getCurrentEventData() const {
    return currentEventData_;
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
        if (counterCheck.success && std::holds_alternative<double>(counterCheck.value)) {
            Logger::debug("StateMachine: Current counter value before condition '" + condition +
                          "': " + std::to_string(std::get<double>(counterCheck.value)));
        }

        auto future = RSM::JSEngine::instance().evaluateExpression(sessionId_, condition);
        auto result = future.get();

        if (!result.success) {
            Logger::error("StateMachine: Failed to evaluate condition: " + result.errorMessage);
            return false;
        }

        // Convert result to boolean
        if (std::holds_alternative<bool>(result.value)) {
            return std::get<bool>(result.value);
        } else if (std::holds_alternative<long>(result.value)) {
            return std::get<long>(result.value) != 0;
        } else if (std::holds_alternative<double>(result.value)) {
            return std::get<double>(result.value) != 0.0;
        } else if (std::holds_alternative<std::string>(result.value)) {
            return !std::get<std::string>(result.value).empty();
        }

        return false;
    } catch (const std::exception &e) {
        Logger::error("StateMachine: Exception evaluating condition: " + std::string(e.what()));
        return false;
    }
}

bool StateMachine::executeAction(const std::string &action) {
    if (action.empty()) {
        return true;
    }

    try {
        Logger::debug("StateMachine: Executing action: " + action);

        // Debug: Check counter value before action
        auto counterBefore = RSM::JSEngine::instance().evaluateExpression(sessionId_, "counter").get();
        if (counterBefore.success && std::holds_alternative<double>(counterBefore.value)) {
            Logger::debug("StateMachine: Counter before action: " +
                          std::to_string(std::get<double>(counterBefore.value)));
        }

        auto future = RSM::JSEngine::instance().executeScript(sessionId_, action);
        auto result = future.get();

        if (!result.success) {
            Logger::error("StateMachine: Action execution failed: " + result.errorMessage);
            return false;
        }

        // Debug: Check counter value after action
        auto counterAfter = RSM::JSEngine::instance().evaluateExpression(sessionId_, "counter").get();
        if (counterAfter.success && std::holds_alternative<double>(counterAfter.value)) {
            Logger::debug("StateMachine: Counter after action: " +
                          std::to_string(std::get<double>(counterAfter.value)));
        }

        return true;
    } catch (const std::exception &e) {
        Logger::error("StateMachine: Exception executing action: " + std::string(e.what()));
        return false;
    }
}

bool StateMachine::enterState(const std::string &stateId) {
    Logger::debug("StateMachine: Entering state: " + stateId);

    // Use hierarchy manager for SCXML-compliant state entry
    if (hierarchyManager_) {
        if (hierarchyManager_->enterState(stateId)) {
            // Update legacy currentState_ for compatibility
            currentState_ = hierarchyManager_->getCurrentState();
            activeStates_ = hierarchyManager_->getActiveStates();

            // Execute entry actions for all entered states
            for (const auto &activeState : activeStates_) {
                executeEntryActions(activeState);
            }

            // Set state variable in JavaScript context
            RSM::JSEngine::instance().setVariable(sessionId_, "_state", ScriptValue{currentState_});

            Logger::debug("StateMachine: Successfully entered state using hierarchy manager: " + stateId +
                          " (current: " + currentState_ + ")");
            return true;
        } else {
            Logger::error("StateMachine: Failed to enter state via hierarchy manager: " + stateId);
            return false;
        }
    }

    // Execute IActionNode-based entry actions
    if (!executeEntryActions(stateId)) {
        Logger::error("StateMachine: Failed to execute IActionNode-based entry actions for state: " + stateId);
        return false;
    }

    // Update current state
    currentState_ = stateId;

    // Update active states (simple implementation - just current state)
    activeStates_.clear();
    activeStates_.push_back(stateId);

    // Set state variable in JavaScript context
    RSM::JSEngine::instance().setVariable(sessionId_, "_state", ScriptValue{stateId});

    Logger::debug("StateMachine: Successfully entered state: " + stateId);
    return true;
}

bool StateMachine::exitState(const std::string &stateId) {
    Logger::debug("StateMachine: Exiting state: " + stateId);

    // Use hierarchy manager for SCXML-compliant state exit
    if (hierarchyManager_) {
        hierarchyManager_->exitState(stateId);
        // Update legacy state variables for compatibility
        currentState_ = hierarchyManager_->getCurrentState();
        activeStates_ = hierarchyManager_->getActiveStates();
    }

    // Execute IActionNode-based exit actions
    if (!executeExitActions(stateId)) {
        Logger::error("StateMachine: Failed to execute IActionNode-based exit actions for state: " + stateId);
        return false;
    }

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
    RSM::JSEngine::instance();  // RAII 보장
    Logger::debug("StateMachine: JSEngine automatically initialized via RAII");

    // Create JavaScript session
    if (!RSM::JSEngine::instance().createSession(sessionId_)) {
        Logger::error("StateMachine: Failed to create JavaScript session");
        return false;
    }

    // Set up basic variables
    RSM::JSEngine::instance().setVariable(sessionId_, "_sessionid", ScriptValue{sessionId_});
    RSM::JSEngine::instance().setVariable(sessionId_, "_name", ScriptValue{std::string("StateMachine")});

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

                if (result.success) {
                    RSM::JSEngine::instance().setVariable(sessionId_, id, result.value);
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
    stats_.currentState = currentState_;
    stats_.isRunning = isRunning_;
}

bool StateMachine::initializeActionExecutor() {
    try {
        // Create ActionExecutor using the same session as StateMachine
        actionExecutor_ = std::make_shared<ActionExecutorImpl>(sessionId_);

        // Create ExecutionContext with shared_ptr and sessionId
        executionContext_ = std::make_unique<ExecutionContextImpl>(actionExecutor_, sessionId_);

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
        return true;  // No model, no actions to execute
    }

    // Find the StateNode in the SCXML model
    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        Logger::debug("StateMachine: State " + stateId + " not found in SCXML model, skipping entry actions");
        return true;  // Not an error if state not found in model
    }

    // Execute IActionNode-based entry actions
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

    // Execute IActionNode-based exit actions
    const auto &exitActions = stateNode->getExitActionNodes();
    if (!exitActions.empty()) {
        Logger::debug("StateMachine: Executing " + std::to_string(exitActions.size()) +
                      " exit action nodes for state: " + stateId);
        return executeActionNodes(exitActions);
    }

    return true;
}

}  // namespace RSM
