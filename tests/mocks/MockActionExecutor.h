#pragma once

#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace RSM {
namespace Test {

/**
 * @brief Mock implementation of IActionExecutor for testing
 *
 * This mock captures all operations for verification in tests
 * and allows simulation of various success/failure scenarios.
 */
class MockActionExecutor : public IActionExecutor {
public:
    /**
     * @brief Construct mock executor with session ID
     * @param sessionId Session identifier to use
     */
    explicit MockActionExecutor(const std::string &sessionId = "test_session");

    /**
     * @brief Destructor
     */
    virtual ~MockActionExecutor() = default;

    // IActionExecutor implementation
    bool executeScript(const std::string &script) override;
    bool assignVariable(const std::string &location, const std::string &expr) override;
    std::string evaluateExpression(const std::string &expression) override;
    void log(const std::string &level, const std::string &message) override;
    bool raiseEvent(const std::string &eventName, const std::string &eventData = "") override;
    bool hasVariable(const std::string &location) override;
    std::string getSessionId() const override;

    // New action execution methods
    bool executeScriptAction(const ScriptAction &action) override;
    bool executeAssignAction(const AssignAction &action) override;
    bool executeLogAction(const LogAction &action) override;
    bool executeRaiseAction(const RaiseAction &action) override;
    bool executeIfAction(const IfAction &action) override;
    bool executeSendAction(const SendAction &action) override;
    bool executeCancelAction(const CancelAction &action) override;
    bool executeForeachAction(const ForeachAction &action) override;
    bool evaluateCondition(const std::string &condition) override;

    // Test verification methods

    /**
     * @brief Get all executed scripts
     * @return Vector of executed script strings
     */
    const std::vector<std::string> &getExecutedScripts() const;

    /**
     * @brief Get all variable assignments
     * @return Map of location -> expression pairs
     */
    const std::map<std::string, std::string> &getAssignedVariables() const;

    /**
     * @brief Get all evaluated expressions
     * @return Vector of evaluated expression strings
     */
    const std::vector<std::string> &getEvaluatedExpressions() const;

    /**
     * @brief Get all log messages
     * @return Vector of log entries (level + message)
     */
    const std::vector<std::pair<std::string, std::string>> &getLogMessages() const;

    /**
     * @brief Get all raised events
     * @return Vector of event entries (name + data)
     */
    const std::vector<std::pair<std::string, std::string>> &getRaisedEvents() const;

    /**
     * @brief Get all variable existence checks
     * @return Vector of checked variable locations
     */
    const std::vector<std::string> &getVariableChecks() const;

    // Test configuration methods

    /**
     * @brief Set whether script execution should succeed
     * @param success True for success, false for failure
     */
    void setScriptExecutionResult(bool success);

    /**
     * @brief Set whether variable assignment should succeed
     * @param success True for success, false for failure
     */
    void setVariableAssignmentResult(bool success);

    /**
     * @brief Set whether event raising should succeed
     * @param success True for success, false for failure
     */
    // REMOVED: setEventRaisingResult was deprecated and violated SCXML fire-and-forget model

    /**
     * @brief Set result for expression evaluation
     * @param expression Expression to configure
     * @param result Result string to return
     */
    void setExpressionResult(const std::string &expression, const std::string &result);

    /**
     * @brief Set whether a variable exists
     * @param location Variable location
     * @param exists True if variable should exist
     */
    void setVariableExists(const std::string &location, bool exists);

    /**
     * @brief Set result for condition evaluation
     * @param condition Condition expression to configure
     * @param result Boolean result to return
     */
    void setConditionResult(const std::string &condition, bool result);

    /**
     * @brief Clear all recorded operations
     */
    void clearHistory();

    /**
     * @brief Get count of specific operation type
     * @param operation Operation type ("script", "assign", "eval", "log", "raise", "check")
     * @return Number of times operation was called
     */
    int getOperationCount(const std::string &operation) const;

private:
    std::string sessionId_;

    // Recorded operations
    std::vector<std::string> executedScripts_;
    std::map<std::string, std::string> assignedVariables_;
    std::vector<std::string> evaluatedExpressions_;
    std::vector<std::pair<std::string, std::string>> logMessages_;
    std::vector<std::pair<std::string, std::string>> raisedEvents_;
    std::vector<std::string> variableChecks_;

    // Test configuration
    bool scriptExecutionResult_ = true;
    bool variableAssignmentResult_ = true;
    // REMOVED: eventRaisingResult_ was legacy variable for deprecated setEventRaisingResult method
    std::map<std::string, std::string> expressionResults_;
    std::map<std::string, bool> variableExistence_;
    std::map<std::string, bool> conditionResults_;
};

/**
 * @brief Mock implementation of IExecutionContext for testing
 */
class MockExecutionContext : public IExecutionContext {
public:
    /**
     * @brief Construct mock context with executor
     * @param executor Action executor to use (typically MockActionExecutor)
     */
    explicit MockExecutionContext(std::shared_ptr<IActionExecutor> executor);

    /**
     * @brief Destructor
     */
    virtual ~MockExecutionContext() = default;

    // IExecutionContext implementation
    IActionExecutor &getActionExecutor() override;
    std::string getCurrentSessionId() const override;
    std::string getCurrentEventData() const override;
    std::string getCurrentEventName() const override;
    std::string getCurrentStateId() const override;
    bool isValid() const override;

    // Test configuration methods
    void setCurrentEvent(const std::string &eventName, const std::string &eventData);
    void setCurrentStateId(const std::string &stateId);
    void setSessionId(const std::string &sessionId);

private:
    std::shared_ptr<IActionExecutor> executor_;
    std::string sessionId_ = "test_session";
    std::string currentEventName_;
    std::string currentEventData_;
    std::string currentStateId_ = "test_state";
};

}  // namespace Test
}  // namespace RSM