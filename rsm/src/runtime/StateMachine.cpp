#include "runtime/StateMachine.h"
#include "common/Logger.h"
#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"
#include "parsing/XIncludeProcessor.h"
#include "scripting/JSEngine.h"
#include <fstream>
#include <random>
#include <sstream>

namespace RSM {

StateMachine::StateMachine() : isRunning_(false), jsEnvironmentReady_(false) {
    sessionId_ = generateSessionId();
    // JS 환경은 지연 초기화로 변경
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

    // Find applicable transitions
    auto transitions = findTransitions(currentState_, eventName);
    if (transitions.empty()) {
        Logger::debug("StateMachine: No transitions found for event: " + eventName + " from state: " + currentState_);
        stats_.failedTransitions++;
        TransitionResult result;
        result.success = false;
        result.fromState = currentState_;
        result.eventName = eventName;
        result.errorMessage = "No transitions found";
        return result;
    }

    // Execute first valid transition (with priority handling)
    for (const auto &transition : transitions) {
        Logger::debug("StateMachine: Checking transition: " + transition.fromState + " -> " + transition.toState +
                      " with condition: '" + transition.condition + "'");
        bool conditionResult = transition.condition.empty() || evaluateCondition(transition.condition);
        Logger::debug("StateMachine: Condition result: " + std::string(conditionResult ? "true" : "false"));

        if (conditionResult) {
            Logger::debug("StateMachine: Executing transition from " + transition.fromState + " to " +
                          transition.toState);

            // Exit current state
            if (!exitState(currentState_)) {
                Logger::error("StateMachine: Failed to exit state: " + currentState_);
                TransitionResult result;
                result.success = false;
                result.fromState = currentState_;
                result.eventName = eventName;
                result.errorMessage = "Failed to exit state: " + currentState_;
                return result;
            }

            // Execute transition action
            if (!transition.action.empty()) {
                Logger::debug("StateMachine: Found transition action: " + transition.action);
                if (!executeAction(transition.action)) {
                    Logger::warn("StateMachine: Transition action failed, but continuing");
                }
            } else {
                Logger::debug("StateMachine: No transition action for this transition");
            }

            // Enter new state
            if (!enterState(transition.toState)) {
                Logger::error("StateMachine: Failed to enter state: " + transition.toState);
                TransitionResult result;
                result.success = false;
                result.fromState = transition.fromState;
                result.toState = transition.toState;
                result.eventName = eventName;
                result.errorMessage = "Failed to enter state: " + transition.toState;
                return result;
            }

            updateStatistics();
            stats_.totalTransitions++;

            Logger::info("StateMachine: Successfully transitioned from " + transition.fromState + " to " +
                         transition.toState);
            return TransitionResult(true, transition.fromState, transition.toState, eventName);
        }
    }

    Logger::debug("StateMachine: No valid transitions found (conditions not met)");
    stats_.failedTransitions++;
    TransitionResult result;
    result.success = false;
    result.fromState = currentState_;
    result.eventName = eventName;
    result.errorMessage = "No valid transitions found (conditions not met)";
    return result;
}

std::string StateMachine::getCurrentState() const {
    return currentState_;
}

std::vector<std::string> StateMachine::getActiveStates() const {
    return activeStates_;
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
    transitions_.clear();
    states_.clear();
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
        // Extract each state individually
        for (const auto &state : allStates) {
            extractStatesBasic(state);
        }

        Logger::debug("StateMachine: Model initialized with initial state: " + initialState_);
        Logger::info("StateMachine: Extracted " + std::to_string(states_.size()) + " states and " +
                     std::to_string(transitions_.size()) + " transitions");
        return true;
    } catch (const std::exception &e) {
        Logger::error("StateMachine: Failed to extract model: " + std::string(e.what()));
        return false;
    }
}

std::vector<StateMachine::Transition> StateMachine::findTransitions(const std::string &fromState,
                                                                    const std::string &event) {
    std::vector<Transition> result;

    for (const auto &transition : transitions_) {
        if (transition.fromState == fromState && (transition.event.empty() || transition.event == event)) {
            result.push_back(transition);
        }
    }

    // Sort by priority (higher priority first)
    std::sort(result.begin(), result.end(),
              [](const Transition &a, const Transition &b) { return a.priority > b.priority; });

    return result;
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

    auto stateIt = states_.find(stateId);
    if (stateIt == states_.end()) {
        // Create basic state if not found
        State newState;
        newState.id = stateId;
        states_[stateId] = newState;
        stateIt = states_.find(stateId);
    }

    // Execute onEntry action
    if (!stateIt->second.onEntryAction.empty()) {
        if (!executeAction(stateIt->second.onEntryAction)) {
            Logger::error("StateMachine: Failed to execute onEntry action for state: " + stateId);
            return false;
        }
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

    auto stateIt = states_.find(stateId);
    if (stateIt != states_.end()) {
        // Execute onExit action
        if (!stateIt->second.onExitAction.empty()) {
            if (!executeAction(stateIt->second.onExitAction)) {
                Logger::error("StateMachine: Failed to execute onExit action for state: " + stateId);
                return false;
            }
        }
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
    // JSEngine 초기화 보장
    if (!RSM::JSEngine::instance().initialize()) {
        Logger::error("StateMachine: Failed to initialize JavaScript engine");
        return false;
    }

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

    jsEnvironmentReady_ = true;
    Logger::debug("StateMachine: JavaScript environment setup completed");
    return true;
}

void StateMachine::updateStatistics() {
    stats_.currentState = currentState_;
    stats_.isRunning = isRunning_;
}

void StateMachine::extractStatesBasic(std::shared_ptr<IStateNode> stateNode) {
    if (!stateNode) {
        return;
    }

    // Create basic state
    State state;
    state.id = stateNode->getId();
    state.isFinal = stateNode->isFinalState();

    // Extract onEntry/onExit actions
    auto entryActions = stateNode->getEntryActions();
    if (!entryActions.empty()) {
        // Combine multiple actions into single script
        std::string combinedEntry;
        for (size_t i = 0; i < entryActions.size(); ++i) {
            if (i > 0) {
                combinedEntry += "; ";
            }
            combinedEntry += entryActions[i];
        }
        state.onEntryAction = combinedEntry;
    }

    auto exitActions = stateNode->getExitActions();
    if (!exitActions.empty()) {
        std::string combinedExit;
        for (size_t i = 0; i < exitActions.size(); ++i) {
            if (i > 0) {
                combinedExit += "; ";
            }
            combinedExit += exitActions[i];
        }
        state.onExitAction = combinedExit;
    }

    states_[state.id] = state;

    // Extract transitions from this state
    auto transitions = stateNode->getTransitions();
    for (const auto &transitionNode : transitions) {
        Transition transition;
        transition.fromState = state.id;
        transition.event = transitionNode->getEvent();

        auto targets = transitionNode->getTargets();
        if (!targets.empty()) {
            transition.toState = targets[0];  // Use first target
        }

        transition.condition = transitionNode->getGuard();

        // Extract and combine transition actions
        auto actions = transitionNode->getActions();
        if (!actions.empty()) {
            std::string combinedAction;
            for (size_t i = 0; i < actions.size(); ++i) {
                if (i > 0) {
                    combinedAction += "; ";
                }
                combinedAction += actions[i];
            }
            transition.action = combinedAction;
        }

        transition.priority = 0;  // Could be enhanced with actual priority

        transitions_.push_back(transition);
    }

    // Recursively process children
    auto children = stateNode->getChildren();
    for (const auto &child : children) {
        extractStatesBasic(child);  // Use shared_ptr directly
    }
}

}  // namespace RSM
