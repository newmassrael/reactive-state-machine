#include "MockActionExecutor.h"
#include "actions/AssignAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include <algorithm>

namespace RSM {
namespace Test {

// MockActionExecutor implementation

MockActionExecutor::MockActionExecutor(const std::string &sessionId) : sessionId_(sessionId) {}

bool MockActionExecutor::executeScript(const std::string &script) {
    executedScripts_.push_back(script);
    return scriptExecutionResult_;
}

bool MockActionExecutor::assignVariable(const std::string &location, const std::string &expr) {
    assignedVariables_[location] = expr;
    return variableAssignmentResult_;
}

std::string MockActionExecutor::evaluateExpression(const std::string &expression) {
    evaluatedExpressions_.push_back(expression);

    auto it = expressionResults_.find(expression);
    if (it != expressionResults_.end()) {
        return it->second;
    }

    // Default behavior: return the expression itself for simple cases
    if (expression == "true" || expression == "false") {
        return expression;
    }
    if (expression.find_first_not_of("0123456789.") == std::string::npos) {
        return expression;  // Numeric literal
    }
    if (expression.front() == '"' && expression.back() == '"') {
        return expression.substr(1, expression.length() - 2);  // String literal
    }

    return "undefined";  // Default for unknown expressions
}

void MockActionExecutor::log(const std::string &level, const std::string &message) {
    logMessages_.emplace_back(level, message);
}

bool MockActionExecutor::raiseEvent(const std::string &eventName, const std::string &eventData) {
    raisedEvents_.emplace_back(eventName, eventData);
    return eventRaisingResult_;
}

bool MockActionExecutor::hasVariable(const std::string &location) {
    variableChecks_.push_back(location);

    auto it = variableExistence_.find(location);
    if (it != variableExistence_.end()) {
        return it->second;
    }

    // Default: check if we've assigned to this variable
    return assignedVariables_.find(location) != assignedVariables_.end();
}

std::string MockActionExecutor::getSessionId() const {
    return sessionId_;
}

const std::vector<std::string> &MockActionExecutor::getExecutedScripts() const {
    return executedScripts_;
}

const std::map<std::string, std::string> &MockActionExecutor::getAssignedVariables() const {
    return assignedVariables_;
}

const std::vector<std::string> &MockActionExecutor::getEvaluatedExpressions() const {
    return evaluatedExpressions_;
}

const std::vector<std::pair<std::string, std::string>> &MockActionExecutor::getLogMessages() const {
    return logMessages_;
}

const std::vector<std::pair<std::string, std::string>> &MockActionExecutor::getRaisedEvents() const {
    return raisedEvents_;
}

const std::vector<std::string> &MockActionExecutor::getVariableChecks() const {
    return variableChecks_;
}

void MockActionExecutor::setScriptExecutionResult(bool success) {
    scriptExecutionResult_ = success;
}

void MockActionExecutor::setVariableAssignmentResult(bool success) {
    variableAssignmentResult_ = success;
}

void MockActionExecutor::setEventRaisingResult(bool success) {
    eventRaisingResult_ = success;
}

void MockActionExecutor::setExpressionResult(const std::string &expression, const std::string &result) {
    expressionResults_[expression] = result;
}

void MockActionExecutor::setVariableExists(const std::string &location, bool exists) {
    variableExistence_[location] = exists;
}

void MockActionExecutor::setConditionResult(const std::string &condition, bool result) {
    conditionResults_[condition] = result;
}

void MockActionExecutor::clearHistory() {
    executedScripts_.clear();
    assignedVariables_.clear();
    evaluatedExpressions_.clear();
    logMessages_.clear();
    raisedEvents_.clear();
    variableChecks_.clear();
}

int MockActionExecutor::getOperationCount(const std::string &operation) const {
    if (operation == "script") {
        return static_cast<int>(executedScripts_.size());
    } else if (operation == "assign") {
        return static_cast<int>(assignedVariables_.size());
    } else if (operation == "eval") {
        return static_cast<int>(evaluatedExpressions_.size());
    } else if (operation == "log") {
        return static_cast<int>(logMessages_.size());
    } else if (operation == "raise") {
        return static_cast<int>(raisedEvents_.size());
    } else if (operation == "check") {
        return static_cast<int>(variableChecks_.size());
    }
    return 0;
}

// New action execution methods (Command pattern implementations)

bool MockActionExecutor::executeScriptAction(const ScriptAction &action) {
    return executeScript(action.getContent());
}

bool MockActionExecutor::executeAssignAction(const AssignAction &action) {
    return assignVariable(action.getLocation(), action.getExpr());
}

bool MockActionExecutor::executeLogAction(const LogAction &action) {
    std::string message;
    if (!action.getExpr().empty()) {
        message = evaluateExpression(action.getExpr());
    }

    if (!action.getLabel().empty()) {
        message = action.getLabel() + ": " + message;
    }

    std::string level = action.getLevel().empty() ? "info" : action.getLevel();
    log(level, message);
    return true;
}

bool MockActionExecutor::executeRaiseAction(const RaiseAction &action) {
    std::string eventData;
    if (!action.getData().empty()) {
        eventData = evaluateExpression(action.getData());
    }

    return raiseEvent(action.getEvent(), eventData);
}

bool MockActionExecutor::executeIfAction(const IfAction &action) {
    const auto &branches = action.getBranches();
    if (branches.empty()) {
        return true;
    }

    for (const auto &branch : branches) {
        bool shouldExecute = false;

        if (branch.isElseBranch) {
            shouldExecute = true;
        } else if (!branch.condition.empty()) {
            shouldExecute = evaluateCondition(branch.condition);
        }

        if (shouldExecute) {
            // For mock, we just return true - actual execution would happen in real context
            return true;
        }
    }

    return true;
}

bool MockActionExecutor::evaluateCondition(const std::string &condition) {
    if (condition.empty()) {
        return true;
    }

    // Check if we have a preset result for this condition
    auto it = conditionResults_.find(condition);
    if (it != conditionResults_.end()) {
        return it->second;
    }

    // Simple mock evaluation - evaluate as expression and convert to boolean
    std::string result = evaluateExpression(condition);

    if (result == "true") {
        return true;
    }
    if (result == "false") {
        return false;
    }
    if (result == "1") {
        return true;
    }
    if (result == "0") {
        return false;
    }
    if (!result.empty() && result != "undefined") {
        return true;
    }

    return false;
}

// MockExecutionContext implementation

MockExecutionContext::MockExecutionContext(std::shared_ptr<IActionExecutor> executor) : executor_(executor) {}

IActionExecutor &MockExecutionContext::getActionExecutor() {
    if (!executor_) {
        throw std::runtime_error("Mock action executor is null");
    }
    return *executor_;
}

std::string MockExecutionContext::getCurrentSessionId() const {
    return sessionId_;
}

std::string MockExecutionContext::getCurrentEventData() const {
    return currentEventData_;
}

std::string MockExecutionContext::getCurrentEventName() const {
    return currentEventName_;
}

std::string MockExecutionContext::getCurrentStateId() const {
    return currentStateId_;
}

bool MockExecutionContext::isValid() const {
    return executor_ != nullptr;
}

void MockExecutionContext::setCurrentEvent(const std::string &eventName, const std::string &eventData) {
    currentEventName_ = eventName;
    currentEventData_ = eventData;
}

void MockExecutionContext::setCurrentStateId(const std::string &stateId) {
    currentStateId_ = stateId;
}

void MockExecutionContext::setSessionId(const std::string &sessionId) {
    sessionId_ = sessionId;
}

}  // namespace Test
}  // namespace RSM