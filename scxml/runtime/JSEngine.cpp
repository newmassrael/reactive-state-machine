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
    std::cout << "JSEngine: Starting initialization..." << std::endl;
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    if (runtime_) {
        std::cout << "JSEngine: Already initialized" << std::endl;
        return true;  // Already initialized
    }

    // JSRuntime will be created in worker thread to ensure thread safety
    runtime_ = nullptr;

    // Start execution thread
    shouldStop_ = false;
    executionThread_ = std::thread(&JSEngine::executionWorker, this);

    std::cout << "JSEngine: Successfully initialized with QuickJS runtime" << std::endl;
    return true;
}

void JSEngine::shutdown() {
    if (shouldStop_) {
        return; // Already shutting down
    }

    // Send shutdown request to worker thread
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::SHUTDOWN_ENGINE, "");
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    // Wait for worker thread to process shutdown
    future.get();

    // Now stop the worker thread
    shouldStop_ = true;
    queueCondition_.notify_all();

    if (executionThread_.joinable()) {
        executionThread_.join();
    }

    std::cout << "JSEngine: Shutdown complete" << std::endl;
}

// === Session Management ===

bool JSEngine::createSession(const std::string& sessionId, const std::string& parentSessionId) {
    // Runtime is now created in worker thread, so no need to check here
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::CREATE_SESSION, sessionId);
    request->parentSessionId = parentSessionId;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    auto result = future.get();
    return result.success;
}

bool JSEngine::destroySession(const std::string& sessionId) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::DESTROY_SESSION, sessionId);
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    auto result = future.get();
    return result.success;
}

bool JSEngine::hasSession(const std::string& sessionId) const {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::HAS_SESSION, sessionId);
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        const_cast<JSEngine*>(this)->requestQueue_.push(std::move(request));
    }
    const_cast<JSEngine*>(this)->queueCondition_.notify_one();

    auto result = future.get();
    return result.success;
}

std::vector<std::string> JSEngine::getActiveSessions() const {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::GET_ACTIVE_SESSIONS, "");
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        const_cast<JSEngine*>(this)->requestQueue_.push(std::move(request));
    }
    const_cast<JSEngine*>(this)->queueCondition_.notify_one();

    auto result = future.get();
    
    // Parse comma-separated session IDs from result
    std::vector<std::string> sessions;
    if (result.success && std::holds_alternative<std::string>(result.value)) {
        std::string sessionIds = std::get<std::string>(result.value);
        if (!sessionIds.empty()) {
            std::stringstream ss(sessionIds);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) {
                    sessions.push_back(item);
                }
            }
        }
    }
    return sessions;
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
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::GET_MEMORY_USAGE, "");
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        const_cast<JSEngine*>(this)->requestQueue_.push(std::move(request));
    }
    const_cast<JSEngine*>(this)->queueCondition_.notify_one();

    auto result = future.get();
    if (result.success && std::holds_alternative<int64_t>(result.value)) {
        return static_cast<size_t>(std::get<int64_t>(result.value));
    }
    return 0;
}

void JSEngine::collectGarbage() {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::COLLECT_GARBAGE, "");
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    // Wait for completion but ignore result
    future.get();
}

// === Thread-safe Execution Worker ===

void JSEngine::executionWorker() {
    std::cout << "JSEngine: Execution worker thread started" << std::endl;
    
    // Create QuickJS runtime in worker thread to ensure thread safety
    runtime_ = JS_NewRuntime();
    if (!runtime_) {
        std::cerr << "JSEngine: Failed to create QuickJS runtime in worker thread" << std::endl;
        return;
    }
    std::cout << "JSEngine: QuickJS runtime created in worker thread" << std::endl;

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

    // Cleanup all sessions with forced garbage collection
    for (auto& pair : sessions_) {
        if (pair.second.jsContext) {
            // Force garbage collection before freeing context
            JS_RunGC(runtime_);
            JS_FreeContext(pair.second.jsContext);
        }
    }
    sessions_.clear();
    
    // Final garbage collection and cleanup
    if (runtime_) {
        // Multiple GC passes to ensure all objects are collected
        for (int i = 0; i < 3; ++i) {
            JS_RunGC(runtime_);
        }
        
        // Free runtime
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
        std::cout << "JSEngine: Worker thread cleaned up QuickJS resources" << std::endl;
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
            case ExecutionRequest::CREATE_SESSION:
                {
                    bool success = createSessionInternal(request->sessionId, request->parentSessionId);
                    result = success ? JSResult::createSuccess() : JSResult::createError("Failed to create session");
                }
                break;
            case ExecutionRequest::DESTROY_SESSION:
                {
                    bool success = destroySessionInternal(request->sessionId);
                    result = success ? JSResult::createSuccess() : JSResult::createError("Failed to destroy session");
                }
                break;
            case ExecutionRequest::HAS_SESSION:
                {
                    bool exists = sessions_.find(request->sessionId) != sessions_.end();
                    result = exists ? JSResult::createSuccess() : JSResult::createError("Session not found");
                }
                break;
            case ExecutionRequest::GET_ACTIVE_SESSIONS:
                {
                    std::string sessionIds;
                    for (const auto& [sessionId, _] : sessions_) {
                        if (!sessionIds.empty()) sessionIds += ",";
                        sessionIds += sessionId;
                    }
                    result = JSResult::createSuccess(sessionIds);
                }
                break;
            case ExecutionRequest::GET_MEMORY_USAGE:
                {
                    if (runtime_) {
                        JSMemoryUsage usage;
                        JS_ComputeMemoryUsage(runtime_, &usage);
                        result = JSResult::createSuccess(static_cast<int64_t>(usage.memory_used_size));
                    } else {
                        result = JSResult::createSuccess(static_cast<int64_t>(0));
                    }
                }
                break;
            case ExecutionRequest::COLLECT_GARBAGE:
                {
                    if (runtime_) {
                        JS_RunGC(runtime_);
                    }
                    result = JSResult::createSuccess();
                }
                break;
            case ExecutionRequest::SHUTDOWN_ENGINE:
                {
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
                    
                    result = JSResult::createSuccess();
                    std::cout << "JSEngine: Worker thread cleaned up QuickJS resources" << std::endl;
                }
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

    // Runtime is guaranteed to exist in worker thread
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
        // Force garbage collection before freeing context
        if (runtime_) {
            JS_RunGC(runtime_);
        }
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

    // Setup SCXML-specific objects (only if needed)
    setupEventObject(ctx);

    return true;
}

// === SCXML-specific Setup ===

void JSEngine::setupSCXMLBuiltins(JSContext* ctx) {
    // Minimal setup - no SCXML-specific functions for now
    // Just let QuickJS work with its default global objects
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