#pragma once

#include "../SCXMLEngine.h"

#include "scripting/JSEngine.h"

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

    // === Engine Information ===
    size_t getMemoryUsage() const override;
    void collectGarbage() override;

private:
    // Convert internal JSResult to public ExecutionResult
    ExecutionResult convertResult(const JSResult &jsResult) const;

    // Convert public Event to internal Event
    std::shared_ptr<Event> convertEvent(std::shared_ptr<Event> publicEvent) const;

    bool initialized_ = false;
};

}  // namespace RSM
