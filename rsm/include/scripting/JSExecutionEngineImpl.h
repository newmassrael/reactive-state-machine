#pragma once

#include "SCXMLTypes.h"
#include "runtime/ISessionObserver.h"
#include "scripting/IJSExecutionEngine.h"
#include "scripting/JSResult.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// QuickJS includes
extern "C" {
#include "quickjs.h"
}

namespace RSM {

// Forward declarations
class StateMachine;

/**
 * @brief Pure JavaScript execution engine implementation using QuickJS
 *
 * SOLID Architecture: Single Responsibility for JavaScript execution only.
 * Implements Observer pattern to synchronize with session lifecycle.
 */
class JSExecutionEngineImpl : public IJSExecutionEngine, public ISessionObserver {
public:
    JSExecutionEngineImpl();
    ~JSExecutionEngineImpl() override;

    // Non-copyable, non-movable for thread safety
    JSExecutionEngineImpl(const JSExecutionEngineImpl &) = delete;
    JSExecutionEngineImpl &operator=(const JSExecutionEngineImpl &) = delete;
    JSExecutionEngineImpl(JSExecutionEngineImpl &&) = delete;
    JSExecutionEngineImpl &operator=(JSExecutionEngineImpl &&) = delete;

    // === IJSExecutionEngine Implementation ===

    std::future<JSResult> executeScript(const std::string &sessionId, const std::string &script) override;
    std::future<JSResult> evaluateExpression(const std::string &sessionId, const std::string &expression) override;
    std::future<JSResult> validateExpression(const std::string &sessionId, const std::string &expression) override;

    std::future<JSResult> setVariable(const std::string &sessionId, const std::string &name,
                                      const ScriptValue &value) override;
    std::future<JSResult> getVariable(const std::string &sessionId, const std::string &name) override;

    std::future<JSResult> setCurrentEvent(const std::string &sessionId, const std::shared_ptr<Event> &event) override;
    std::future<JSResult> setupSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                               const std::vector<std::string> &ioProcessors) override;

    bool registerGlobalFunction(const std::string &functionName,
                                std::function<ScriptValue(const std::vector<ScriptValue> &)> callback) override;

    std::string getEngineInfo() const override;
    size_t getMemoryUsage() const override;
    void collectGarbage() override;

    bool initializeSessionContext(const std::string &sessionId, const std::string &parentSessionId = "") override;
    bool cleanupSessionContext(const std::string &sessionId) override;
    bool hasSessionContext(const std::string &sessionId) const override;
    bool isVariablePreInitialized(const std::string &sessionId, const std::string &variableName) const override;

    bool initialize() override;
    void shutdown() override;
    bool isInitialized() const override;

    // === ISessionObserver Implementation ===

    void onSessionCreated(const std::string &sessionId, const std::string &parentSessionId = "") override;
    void onSessionDestroyed(const std::string &sessionId) override;
    void onSessionSystemVariablesUpdated(const std::string &sessionId, const std::string &sessionName,
                                         const std::vector<std::string> &ioProcessors) override;

    // === StateMachine Integration ===

    /**
     * @brief Set StateMachine for In() function support
     * @param stateMachine Pointer to StateMachine instance
     * @param sessionId Session ID to associate with this state machine
     */
    void setStateMachine(StateMachine *stateMachine, const std::string &sessionId);

    /**
     * @brief Remove StateMachine association
     * @param sessionId Session ID to remove association for
     */
    void removeStateMachine(const std::string &sessionId);

private:
    // === Internal Types ===

    struct SessionContext {
        JSContext *jsContext = nullptr;
        std::string sessionId;
        std::string parentSessionId;
        std::shared_ptr<Event> currentEvent;
        std::string sessionName;
        std::vector<std::string> ioProcessors;
        std::unordered_set<std::string>
            preInitializedVars;  // Variables set before datamodel initialization (e.g., invoke data)

        SessionContext() = default;

        SessionContext(const std::string &id, const std::string &parent = "")
            : sessionId(id), parentSessionId(parent) {}
    };

    struct ExecutionRequest {
        enum Type {
            EXECUTE_SCRIPT,
            EVALUATE_EXPRESSION,
            VALIDATE_EXPRESSION,
            SET_VARIABLE,
            GET_VARIABLE,
            SET_CURRENT_EVENT,
            SETUP_SYSTEM_VARIABLES,
            COLLECT_GARBAGE,
            SHUTDOWN_ENGINE
        };

        Type type;
        std::string sessionId;
        std::string code;                       // for EXECUTE_SCRIPT, EVALUATE_EXPRESSION
        std::string variableName;               // for SET_VARIABLE, GET_VARIABLE
        ScriptValue variableValue;              // for SET_VARIABLE
        std::shared_ptr<Event> event;           // for SET_CURRENT_EVENT
        std::string sessionName;                // for SETUP_SYSTEM_VARIABLES
        std::vector<std::string> ioProcessors;  // for SETUP_SYSTEM_VARIABLES
        std::promise<JSResult> promise;

        ExecutionRequest(Type t, const std::string &sid) : type(t), sessionId(sid) {}
    };

    // === QuickJS Management ===
    JSRuntime *runtime_ = nullptr;
    std::unordered_map<std::string, SessionContext> contexts_;
    mutable std::mutex contextsMutex_;

    // === Thread-safe Execution ===
    mutable std::queue<std::unique_ptr<ExecutionRequest>> requestQueue_;
    mutable std::mutex queueMutex_;
    mutable std::condition_variable queueCondition_;
    std::thread executionThread_;
    std::atomic<bool> shouldStop_{false};
    std::atomic<bool> initialized_{false};

    // === Global Functions ===
    std::unordered_map<std::string, std::function<ScriptValue(const std::vector<ScriptValue> &)>> globalFunctions_;
    std::mutex globalFunctionsMutex_;

    // === StateMachine Integration ===
    std::unordered_map<std::string, StateMachine *> stateMachines_;  // sessionId -> StateMachine*
    mutable std::mutex stateMachinesMutex_;

    // === Internal Methods ===

    void executionWorker();
    void processExecutionRequest(std::unique_ptr<ExecutionRequest> request);

    // QuickJS helpers
    JSResult executeScriptInternal(const std::string &sessionId, const std::string &script);
    JSResult evaluateExpressionInternal(const std::string &sessionId, const std::string &expression);
    JSResult validateExpressionInternal(const std::string &sessionId, const std::string &expression);
    JSResult setVariableInternal(const std::string &sessionId, const std::string &name, const ScriptValue &value);
    JSResult getVariableInternal(const std::string &sessionId, const std::string &name);
    JSResult setCurrentEventInternal(const std::string &sessionId, const std::shared_ptr<Event> &event);
    JSResult setupSystemVariablesInternal(const std::string &sessionId, const std::string &sessionName,
                                          const std::vector<std::string> &ioProcessors);

    // Context management
    bool createSessionContextInternal(const std::string &sessionId, const std::string &parentSessionId);
    bool destroySessionContextInternal(const std::string &sessionId);
    SessionContext *getSessionContext(const std::string &sessionId);

    // QuickJS setup
    bool setupQuickJSContext(JSContext *ctx, const std::string &sessionId);
    void setupSCXMLBuiltins(JSContext *ctx, const std::string &sessionId);
    void setupEventObject(JSContext *ctx, const std::string &sessionId);
    void setupConsoleObject(JSContext *ctx);
    void setupMathObject(JSContext *ctx);
    void setupSystemVariables(JSContext *ctx);

    // Static callback functions for QuickJS
    static JSValue inFunctionWrapper(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);
    static JSValue consoleFunctionWrapper(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);
    static JSValue queueErrorEventWrapper(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    // Helper method for In() function
    bool checkStateActive(const std::string &stateName, const std::string &sessionId) const;

    // Type conversion
    ScriptValue quickJSToJSValue(JSContext *ctx, JSValue qjsValue);
    JSValue jsValueToQuickJS(JSContext *ctx, const ScriptValue &value);

    // Error handling
    JSResult createErrorFromException(JSContext *ctx);

    // Internal event system
    void queueInternalEvent(const std::string &sessionId, const std::string &eventName);

    // Memory management
    void collectGarbageInternal();
    size_t getMemoryUsageInternal() const;
};

}  // namespace RSM