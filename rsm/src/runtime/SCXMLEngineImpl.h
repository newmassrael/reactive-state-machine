#pragma once

#include "../SCXMLEngine.h"

#include "model/SCXMLModel.h"
#include "runtime/IActionExecutor.h"
#include "runtime/StateMachine.h"
#include "scripting/JSEngine.h"
#include <map>

namespace RSM {

/**
 * @brief Implementation of the public SCXML Engine interface
 *
 * This class bridges the public API with the internal JSEngine implementation,
 * providing a clean separation between public interface and internal details.
 */
class SCXMLEngineImpl : public SCXMLEngine {
public:
    SCXMLEngineImpl();
    ~SCXMLEngineImpl() override;

    // === Engine Lifecycle ===
    bool initialize() override;
    void shutdown() override;
    std::string getEngineInfo() const override;

    // === Session Management ===
    bool createSession(const std::string &sessionId, const std::string &parentSessionId = "") override;
    bool destroySession(const std::string &sessionId) override;
    bool hasSession(const std::string &sessionId) const override;
    std::vector<SessionInfo> getActiveSessions() const override;

    // === JavaScript Execution ===
    std::future<ExecutionResult> executeScript(const std::string &sessionId, const std::string &script) override;
    std::future<ExecutionResult> evaluateExpression(const std::string &sessionId,
                                                    const std::string &expression) override;

    // === Variable Management ===
    std::future<ExecutionResult> setVariable(const std::string &sessionId, const std::string &name,
                                             const ScriptValue &value) override;
    std::future<ExecutionResult> getVariable(const std::string &sessionId, const std::string &name) override;

    // === SCXML Event System ===
    std::future<ExecutionResult> setCurrentEvent(const std::string &sessionId, std::shared_ptr<Event> event) override;
    std::future<ExecutionResult> setupSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                                      const std::vector<std::string> &ioProcessors) override;

    // === High-Level SCXML State Machine API (NEW) ===
    bool loadSCXMLFromString(const std::string &scxmlContent, const std::string &sessionId = "") override;
    bool loadSCXMLFromFile(const std::string &scxmlFile, const std::string &sessionId = "") override;
    bool startStateMachine(const std::string &sessionId = "") override;
    void stopStateMachine(const std::string &sessionId = "") override;
    bool sendEventSync(const std::string &eventName, const std::string &sessionId = "",
                       const std::string &eventData = "") override;
    bool isStateMachineRunning(const std::string &sessionId = "") const override;
    std::string getCurrentStateSync(const std::string &sessionId = "") const override;
    bool isInStateSync(const std::string &stateId, const std::string &sessionId = "") const override;
    std::vector<std::string> getActiveStatesSync(const std::string &sessionId = "") const override;
    bool setVariableSync(const std::string &name, const std::string &value, const std::string &sessionId = "") override;
    std::string getVariableSync(const std::string &name, const std::string &sessionId = "") const override;
    std::string getLastStateMachineError(const std::string &sessionId = "") const override;

    // === Engine Information ===
    size_t getMemoryUsage() const override;
    void collectGarbage() override;

private:
    // Convert internal JSResult to public ExecutionResult
    ExecutionResult convertResult(const JSResult &jsResult) const;

    // Convert public Event to internal Event
    std::shared_ptr<Event> convertEvent(std::shared_ptr<Event> publicEvent) const;

    /**
     * @brief Execute onentry actions for a state according to W3C SCXML specification
     * @param stateId State ID to execute onentry actions for
     */
    void executeOnEntryActions(const std::string &stateId);

    /**
     * @brief Generate a unique session ID for internal use
     */
    std::string generateSessionId() const;

    bool initialized_ = false;
    std::shared_ptr<RSM::SCXMLModel> scxmlModel_;
    std::shared_ptr<IActionExecutor> actionExecutor_;
    std::string sessionId_;

    // === High-Level State Machine Support ===
    std::string defaultSessionId_;
    std::unique_ptr<StateMachine> stateMachine_;
    std::map<std::string, std::string> sessionErrors_;
};

}  // namespace RSM
