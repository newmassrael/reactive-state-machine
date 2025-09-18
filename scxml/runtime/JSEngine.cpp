#include "JSEngine.h"
#include "Event.h"
#include "quickjs.h"
#include <iostream>
#include <sstream>

namespace SCXML::Runtime {

// Static instance
JSEngine& JSEngine::instance() {
    static JSEngine instance;
    return instance;
}

JSEngine::~JSEngine() {
    shutdown();
}

bool JSEngine::initialize() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    if (runtime_) {
        return true;  // Already initialized
    }

    // Create QuickJS runtime
    runtime_ = JS_NewRuntime();
    if (!runtime_) {
        std::cerr << "JSEngine: Failed to create QuickJS runtime" << std::endl;
        return false;
    }

    // Start execution thread
    shouldStop_ = false;
    executionThread_ = std::thread(&JSEngine::executionWorker, this);

    std::cout << "JSEngine: Initialized with QuickJS runtime" << std::endl;
    return true;
}

void JSEngine::shutdown() {
    // Stop execution thread
    shouldStop_ = true;
    queueCondition_.notify_all();

    if (executionThread_.joinable()) {
        executionThread_.join();
    }

    std::lock_guard<std::mutex> lock(sessionsMutex_);

    // Cleanup all sessions
    for (auto& [sessionId, session] : sessions_) {
        if (session.jsContext) {
            JS_FreeContext(session.jsContext);
        }
    }
    sessions_.clear();

    // Cleanup runtime
    if (runtime_) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }

    std::cout << "JSEngine: Shutdown complete" << std::endl;
}

// === Session Management ===

bool JSEngine::createSession(const std::string& sessionId, const std::string& parentSessionId) {
    auto future = std::async(std::launch::async, [this, sessionId, parentSessionId]() {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        return createSessionInternal(sessionId, parentSessionId);
    });
    return future.get();
}

bool JSEngine::destroySession(const std::string& sessionId) {
    auto future = std::async(std::launch::async, [this, sessionId]() {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        return destroySessionInternal(sessionId);
    });
    return future.get();
}

bool JSEngine::hasSession(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.find(sessionId) != sessions_.end();
}

std::vector<std::string> JSEngine::getActiveSessions() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    std::vector<std::string> result;
    result.reserve(sessions_.size());
    for (const auto& [sessionId, _] : sessions_) {
        result.push_back(sessionId);
    }
    return result;
}

// === JavaScript Execution ===

std::future<JSResult> JSEngine::executeScript(const std::string& sessionId, const std::string& script) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::EXECUTE_SCRIPT, sessionId);
    request->code = script;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::evaluateExpression(const std::string& sessionId, const std::string& expression) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::EVALUATE_EXPRESSION, sessionId);
    request->code = expression;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::setVariable(const std::string& sessionId, const std::string& name, const ScriptValue& value) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::SET_VARIABLE, sessionId);
    request->variableName = name;
    request->variableValue = value;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::getVariable(const std::string& sessionId, const std::string& name) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::GET_VARIABLE, sessionId);
    request->variableName = name;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::setCurrentEvent(const std::string& sessionId, const std::shared_ptr<Event>& event) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::SET_CURRENT_EVENT, sessionId);
    request->event = event;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::setupSystemVariables(const std::string& sessionId,
                                                     const std::string& sessionName,
                                                     const std::vector<std::string>& ioProcessors) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::SETUP_SYSTEM_VARIABLES, sessionId);
    request->sessionName = sessionName;
    request->ioProcessors = ioProcessors;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    return future;
}

// === Engine Information ===

std::string JSEngine::getEngineInfo() const {
    return "QuickJS Session-based Engine v1.0";
}

size_t JSEngine::getMemoryUsage() const {
    if (!runtime_) return 0;

    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime_, &usage);
    return usage.memory_used_size;
}

void JSEngine::collectGarbage() {
    if (runtime_) {
        JS_RunGC(runtime_);
    }
}

// === Thread-safe Execution Worker ===

void JSEngine::executionWorker() {
    std::cout << "JSEngine: Execution worker thread started" << std::endl;

    while (!shouldStop_) {
        std::unique_lock<std::mutex> lock(queueMutex_);

        queueCondition_.wait(lock, [this] {
            return !requestQueue_.empty() || shouldStop_;
        });

        while (!requestQueue_.empty() && !shouldStop_) {
            auto request = std::move(requestQueue_.front());
            requestQueue_.pop();
            lock.unlock();

            processExecutionRequest(std::move(request));

            lock.lock();
        }
    }

    std::cout << "JSEngine: Execution worker thread stopped" << std::endl;
}

void JSEngine::processExecutionRequest(std::unique_ptr<ExecutionRequest> request) {
    try {
        JSResult result;

        switch (request->type) {
            case ExecutionRequest::EXECUTE_SCRIPT:
                result = executeScriptInternal(request->sessionId, request->code);
                break;
            case ExecutionRequest::EVALUATE_EXPRESSION:
                result = evaluateExpressionInternal(request->sessionId, request->code);
                break;
            case ExecutionRequest::SET_VARIABLE:
                result = setVariableInternal(request->sessionId, request->variableName, request->variableValue);
                break;
            case ExecutionRequest::GET_VARIABLE:
                result = getVariableInternal(request->sessionId, request->variableName);
                break;
            case ExecutionRequest::SET_CURRENT_EVENT:
                result = setCurrentEventInternal(request->sessionId, request->event);
                break;
            case ExecutionRequest::SETUP_SYSTEM_VARIABLES:
                result = setupSystemVariablesInternal(request->sessionId, request->sessionName, request->ioProcessors);
                break;
        }

        request->promise.set_value(result);

    } catch (const std::exception& e) {
        request->promise.set_value(JSResult::createError("Exception: " + std::string(e.what())));
    }
}

// === Internal Implementation (Part 1) ===

bool JSEngine::createSessionInternal(const std::string& sessionId, const std::string& parentSessionId) {
    if (sessions_.find(sessionId) != sessions_.end()) {
        std::cerr << "JSEngine: Session already exists: " << sessionId << std::endl;
        return false;
    }

    if (!runtime_) {
        std::cerr << "JSEngine: Runtime not initialized" << std::endl;
        return false;
    }

    // Create QuickJS context
    JSContext* ctx = JS_NewContext(runtime_);
    if (!ctx) {
        std::cerr << "JSEngine: Failed to create context for session: " << sessionId << std::endl;
        return false;
    }

    // Setup context
    if (!setupQuickJSContext(ctx)) {
        JS_FreeContext(ctx);
        return false;
    }

    // Create session info
    SessionContext session;
    session.jsContext = ctx;
    session.sessionId = sessionId;
    session.parentSessionId = parentSessionId;

    sessions_[sessionId] = std::move(session);

    std::cout << "JSEngine: Created session '" << sessionId << "'" << std::endl;
    return true;
}

bool JSEngine::destroySessionInternal(const std::string& sessionId) {
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }

    if (it->second.jsContext) {
        JS_FreeContext(it->second.jsContext);
    }

    sessions_.erase(it);
    std::cout << "JSEngine: Destroyed session '" << sessionId << "'" << std::endl;
    return true;
}

JSEngine::SessionContext* JSEngine::getSession(const std::string& sessionId) {
    auto it = sessions_.find(sessionId);
    return (it != sessions_.end()) ? &it->second : nullptr;
}

bool JSEngine::setupQuickJSContext(JSContext* ctx) {
    // Set engine instance as context opaque for callbacks
    JS_SetContextOpaque(ctx, this);

    // Setup SCXML builtins
    setupSCXMLBuiltins(ctx);
    setupEventObject(ctx);

    return true;
}

// === SCXML-specific Setup ===

void JSEngine::setupSCXMLBuiltins(JSContext* ctx) {
    ::JSValue global = JS_GetGlobalObject(ctx);

    // TODO: Setup In() function for state checking
    // JSValue inFunction = JS_NewCFunction(ctx, inFunctionWrapper, "In", 1);
    // JS_SetPropertyStr(ctx, global, "In", inFunction);

    // Setup console.log
    ::JSValue console = JS_NewObject(ctx);
    // TODO: Setup console.log function
    JS_SetPropertyStr(ctx, global, "console", console);

    JS_FreeValue(ctx, global);
}

void JSEngine::setupEventObject(JSContext* ctx) {
    ::JSValue global = JS_GetGlobalObject(ctx);
    ::JSValue eventObj = JS_NewObject(ctx);

    // Initialize _event with default properties
    JS_SetPropertyStr(ctx, eventObj, "name", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, eventObj, "type", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, eventObj, "sendid", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, eventObj, "origin", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, eventObj, "origintype", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, eventObj, "invokeid", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, eventObj, "data", JS_UNDEFINED);

    JS_SetPropertyStr(ctx, global, "_event", eventObj);
    JS_FreeValue(ctx, global);
}

}  // namespace SCXML::Runtime