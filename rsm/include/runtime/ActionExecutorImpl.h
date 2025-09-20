#pragma once

#include "IActionExecutor.h"
#include "common/Logger.h"
#include "scripting/JSEngine.h"
#include <functional>
#include <memory>
#include <string>

namespace RSM {

/**
 * @brief Concrete implementation of IActionExecutor using JSEngine
 *
 * This implementation bridges the action execution interface with
 * the existing JSEngine infrastructure, providing SCXML executable
 * content capabilities while maintaining compatibility with current
 * architecture.
 */
class ActionExecutorImpl : public IActionExecutor {
public:
    /**
     * @brief Construct executor for given session
     * @param sessionId JavaScript session identifier
     */
    explicit ActionExecutorImpl(const std::string &sessionId);

    /**
     * @brief Destructor
     */
    virtual ~ActionExecutorImpl() = default;

    // High-level action execution methods (Command pattern)
    bool executeScriptAction(const ScriptAction &action) override;
    bool executeAssignAction(const AssignAction &action) override;
    bool executeLogAction(const LogAction &action) override;
    bool executeRaiseAction(const RaiseAction &action) override;
    bool executeIfAction(const IfAction &action) override;

    // Low-level primitives
    bool executeScript(const std::string &script) override;
    bool assignVariable(const std::string &location, const std::string &expr) override;
    std::string evaluateExpression(const std::string &expression) override;
    bool evaluateCondition(const std::string &condition) override;
    void log(const std::string &level, const std::string &message) override;
    bool raiseEvent(const std::string &eventName, const std::string &eventData = "") override;
    bool hasVariable(const std::string &location) override;
    std::string getSessionId() const override;

    /**
     * @brief Set callback for event raising (dependency injection)
     * @param callback Function to call when raising events
     */
    void setEventRaiseCallback(std::function<bool(const std::string &, const std::string &)> callback);

    /**
     * @brief Set current event data for _event variable access
     * @param eventName Current event name
     * @param eventData Current event data as JSON string
     */
    void setCurrentEvent(const std::string &eventName, const std::string &eventData);

    /**
     * @brief Clear current event data
     */
    void clearCurrentEvent();

    /**
     * @brief Check if session is ready for execution
     * @return true if session exists and is operational
     */
    bool isSessionReady() const;

private:
    std::string sessionId_;
    std::function<bool(const std::string &, const std::string &)> eventRaiseCallback_;
    std::string currentEventName_;
    std::string currentEventData_;

    /**
     * @brief Validate variable location syntax
     * @param location Variable location string
     * @return true if location is valid
     */
    bool isValidLocation(const std::string &location) const;

    /**
     * @brief Handle JavaScript execution errors
     * @param operation Operation description for logging
     * @param errorMessage Error message from JSEngine
     */
    void handleJSError(const std::string &operation, const std::string &errorMessage) const;

    /**
     * @brief Ensure current event is set in JavaScript context
     * @return true if _event variable was set successfully
     */
    bool ensureCurrentEventSet();
};

}  // namespace RSM