#pragma once

#include "../SCXMLTypes.h"
#include "JSResult.h"
#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Forward declarations for QuickJS
struct JSRuntime;
struct JSContext;
struct JSValue;

// JSValueConst is defined by QuickJS, no need to redefine

namespace RSM {

/**
 * @brief Thread-safe session-based JavaScript engine
 *
 * Global singleton that manages multiple isolated JavaScript contexts (sessions).
 * Each session has its own variable space, event context, and system variables.
 * All JavaScript execution happens in a single background thread for QuickJS thread safety.
 */
class JSEngine {
public:
    /**
     * @brief Get the global JSEngine instance
     */
    static JSEngine &instance();

    /**
     * @brief Reset the JavaScript engine for test isolation
     * Reinitializes the engine after shutdown, allowing fresh start between tests
     */
    void reset();

    /**
     * @brief Shutdown the JavaScript engine and cleanup all sessions
     */
    void shutdown();

    // === Session Management ===

    /**
     * @brief Create a new JavaScript session with isolated context
     * @param sessionId Unique identifier for the session
     * @param parentSessionId Optional parent session for hierarchical contexts
     * @return true if session created successfully
     */
    bool createSession(const std::string &sessionId, const std::string &parentSessionId = "");

    /**
     * @brief Destroy a JavaScript session and cleanup its context
     * @param sessionId Session to destroy
     * @return true if session destroyed successfully
     */
    bool destroySession(const std::string &sessionId);

    /**
     * @brief Check if a session exists
     * @param sessionId Session to check
     * @return true if session exists
     */
    bool hasSession(const std::string &sessionId) const;

    /**
     * @brief Get list of all active sessions
     * @return Vector of session IDs
     */
    std::vector<std::string> getActiveSessions() const;

    // === Thread-safe JavaScript Execution ===

    /**
     * @brief Execute JavaScript script in the specified session
     * @param sessionId Target session
     * @param script JavaScript code to execute
     * @return Future with execution result
     */
    std::future<JSResult> executeScript(const std::string &sessionId, const std::string &script);

    /**
     * @brief Evaluate JavaScript expression in the specified session
     * @param sessionId Target session
     * @param expression JavaScript expression to evaluate
     * @return Future with evaluation result
     */
    std::future<JSResult> evaluateExpression(const std::string &sessionId, const std::string &expression);

    // === Session-specific Variable Management ===

    /**
     * @brief Set a variable in the specified session
     * @param sessionId Target session
     * @param name Variable name
     * @param value Variable value
     * @return Future indicating success/failure
     */
    std::future<JSResult> setVariable(const std::string &sessionId, const std::string &name, const ScriptValue &value);

    /**
     * @brief Get a variable from the specified session
     * @param sessionId Target session
     * @param name Variable name
     * @return Future with variable value or error
     */
    std::future<JSResult> getVariable(const std::string &sessionId, const std::string &name);

    // === SCXML-specific Features ===

    /**
     * @brief Set the current event for a session (_event variable)
     * @param sessionId Target session
     * @param event Current event to set
     * @return Future indicating success/failure
     */
    std::future<JSResult> setCurrentEvent(const std::string &sessionId, const std::shared_ptr<Event> &event);

    /**
     * @brief Setup SCXML system variables for a session
     * @param sessionId Target session
     * @param sessionName Human-readable session name
     * @param ioProcessors List of available I/O processors
     * @return Future indicating success/failure
     */
    std::future<JSResult> setupSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                               const std::vector<std::string> &ioProcessors);

    /**
     * @brief Register a native function accessible from JavaScript
     * @param functionName Name of the function in JavaScript
     * @param callback Native function implementation
     * @return true if registration successful
     */
    bool registerGlobalFunction(const std::string &functionName,
                                std::function<ScriptValue(const std::vector<ScriptValue> &)> callback);

    // === Engine Information ===

    /**
     * @brief Get engine name and version
     */
    std::string getEngineInfo() const;

    /**
     * @brief Get current memory usage in bytes
     */
    size_t getMemoryUsage() const;

    /**
     * @brief Trigger garbage collection
     */
    void collectGarbage();

    /**
     * @brief Validate JavaScript expression syntax without executing it
     * @param sessionId Target session for context
     * @param expression JavaScript expression to validate
     * @return Future with validation result (true if syntax is valid)
     */
    std::future<JSResult> validateExpression(const std::string &sessionId, const std::string &expression);

private:
    JSEngine();  // 생성자에서 완전 초기화
    ~JSEngine();

    // Non-copyable, non-movable
    JSEngine(const JSEngine &) = delete;
    JSEngine &operator=(const JSEngine &) = delete;
    JSEngine(JSEngine &&) = delete;
    JSEngine &operator=(JSEngine &&) = delete;

    // === Internal Types ===

    struct SessionContext {
        JSContext *jsContext = nullptr;
        std::string sessionId;
        std::string parentSessionId;
        std::shared_ptr<Event> currentEvent;
        std::string sessionName;
        std::vector<std::string> ioProcessors;
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
            CREATE_SESSION,
            DESTROY_SESSION,
            HAS_SESSION,
            GET_ACTIVE_SESSIONS,
            GET_MEMORY_USAGE,
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
        std::string parentSessionId;            // for CREATE_SESSION
        std::promise<JSResult> promise;

        ExecutionRequest(Type t, const std::string &sid) : type(t), sessionId(sid) {}
    };

    // === QuickJS Management ===
    JSRuntime *runtime_ = nullptr;
    std::unordered_map<std::string, SessionContext> sessions_;
    mutable std::mutex sessionsMutex_;

    // === Thread-safe Execution ===
    mutable std::queue<std::unique_ptr<ExecutionRequest>> requestQueue_;
    mutable std::mutex queueMutex_;
    mutable std::condition_variable queueCondition_;
    std::thread executionThread_;
    std::atomic<bool> shouldStop_{false};

    // === Global Functions ===
    std::unordered_map<std::string, std::function<ScriptValue(const std::vector<ScriptValue> &)>> globalFunctions_;
    std::mutex globalFunctionsMutex_;

    // === Internal Methods ===
    void executionWorker();
    void processExecutionRequest(std::unique_ptr<ExecutionRequest> request);
    void initializeInternal();  // 공통 초기화 로직

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
    bool createSessionInternal(const std::string &sessionId, const std::string &parentSessionId);
    bool destroySessionInternal(const std::string &sessionId);
    SessionContext *getSession(const std::string &sessionId);

    // QuickJS setup
    bool setupQuickJSContext(JSContext *ctx);
    void setupSCXMLBuiltins(JSContext *ctx);
    void setupEventObject(JSContext *ctx);
    void setupConsoleObject(JSContext *ctx);
    void setupMathObject(JSContext *ctx);
    void setupSystemVariables(JSContext *ctx);

    // Static callback functions for QuickJS
    static JSValue inFunctionWrapper(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);
    static JSValue consoleFunctionWrapper(JSContext *ctx, JSValue this_val, int argc, JSValue *argv);

    // Type conversion
    ScriptValue quickJSToJSValue(JSContext *ctx, JSValue qjsValue);
    JSValue jsValueToQuickJS(JSContext *ctx, const ScriptValue &value);

    // Error handling
    JSResult createErrorFromException(JSContext *ctx);
};

}  // namespace RSM
