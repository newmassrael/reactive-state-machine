#include "scripting/JSEngine.h"
#include "common/Logger.h"
#include "common/UniqueIdGenerator.h"
#include "events/EventRaiserRegistry.h"
#include "events/EventRaiserService.h"
#include "events/IEventDispatcher.h"
#include "quickjs.h"
#include "runtime/StateMachine.h"
#include "scripting/DOMBinding.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>

namespace RSM {

// Static instance
JSEngine &JSEngine::instance() {
    static JSEngine instance;
    return instance;
}

JSEngine::JSEngine() {
    LOG_DEBUG("JSEngine: Starting initialization in constructor...");
    initializeInternal();

    // Initialize EventRaiserService with dependency injection
    initializeEventRaiserService();

    LOG_DEBUG("JSEngine: Constructor completed - fully initialized");
}

JSEngine::~JSEngine() {
    shutdown();
}

void JSEngine::shutdown() {
    LOG_DEBUG("JSEngine: shutdown() called - shouldStop: {}", shouldStop_.load());

    if (shouldStop_) {
        LOG_DEBUG("JSEngine: Already shutting down, returning");
        return;  // Already shutting down
    }

    // RAII: No need to reset worker state

    // Send shutdown request to worker thread
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::SHUTDOWN_ENGINE, "");
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    // Wait for worker thread to process shutdown
    future.get();

    // Now stop the worker thread
    LOG_DEBUG("JSEngine: Setting shouldStop = true");
    shouldStop_ = true;
    queueCondition_.notify_all();

    if (executionThread_.joinable()) {
        LOG_DEBUG("JSEngine: Attempting to join worker thread...");
        auto start = std::chrono::steady_clock::now();

        // Try join with timeout using future
        std::future<void> joinFuture = std::async(std::launch::async, [this]() { executionThread_.join(); });

        if (joinFuture.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            LOG_ERROR("JSEngine: Worker thread join TIMEOUT - thread is stuck!");
            // Cannot force terminate, but at least we know what happened
        } else {
            auto elapsed = std::chrono::steady_clock::now() - start;
            LOG_DEBUG("JSEngine: Worker thread joined successfully in {}ms",
                      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        }
    }

    LOG_DEBUG("JSEngine: Shutdown complete");
}

void JSEngine::reset() {
    LOG_DEBUG("JSEngine: Starting reset for test isolation...");

    // First ensure complete shutdown
    shutdown();

    // Clear any remaining state
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        sessions_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(globalFunctionsMutex_);
        globalFunctions_.clear();
    }

    // NOTE: Do NOT clear stateMachines_ during reset - StateMachine registrations
    // should persist across JSEngine resets for SCXML W3C compliance.
    // StateMachines register themselves automatically when created.

    // Clear request queue
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        size_t queueSize = requestQueue_.size();
        LOG_DEBUG("JSEngine: Clearing request queue - size: {}", queueSize);
        while (!requestQueue_.empty()) {
            requestQueue_.pop();
        }
        LOG_DEBUG("JSEngine: Request queue cleared");
    }

    // Clear EventRaiser registry for complete test isolation
    clearEventRaiserRegistry();

    // Reinitialize
    initializeInternal();

    LOG_DEBUG("JSEngine: Reset completed - ready for fresh start");
}

void JSEngine::initializeInternal() {
    // Initialize member variables
    runtime_ = nullptr;
    shouldStop_ = false;

    // Start execution thread and wait for complete initialization
    executionThread_ = std::thread(&JSEngine::executionWorker, this);

    // Wait for worker thread to be fully ready
    std::unique_lock<std::mutex> lock(queueMutex_);
    queueCondition_.wait(lock, [this] { return runtime_ != nullptr; });
}

// === Session Management ===

bool JSEngine::createSession(const std::string &sessionId, const std::string &parentSessionId) {
    // Runtime is now created in worker thread, so no need to check here
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::CREATE_SESSION, sessionId);
    request->parentSessionId = parentSessionId;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    auto result = future.get();
    return result.isSuccess();
}

bool JSEngine::destroySession(const std::string &sessionId) {
    // Check if JSEngine is already shutdown to prevent deadlock
    if (shouldStop_.load()) {
        LOG_DEBUG("JSEngine: Already shutdown, skipping destroySession for: {}", sessionId);
        return true;
    }

    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::DESTROY_SESSION, sessionId);
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    auto result = future.get();
    return result.isSuccess();
}

bool JSEngine::hasSession(const std::string &sessionId) const {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::HAS_SESSION, sessionId);
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        const_cast<JSEngine *>(this)->requestQueue_.push(std::move(request));
    }
    const_cast<JSEngine *>(this)->queueCondition_.notify_one();

    auto result = future.get();
    return result.isSuccess();
}

std::vector<std::string> JSEngine::getActiveSessions() const {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::GET_ACTIVE_SESSIONS, "");
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        const_cast<JSEngine *>(this)->requestQueue_.push(std::move(request));
    }
    const_cast<JSEngine *>(this)->queueCondition_.notify_one();

    auto result = future.get();
    // Parse comma-separated session IDs from result
    std::vector<std::string> sessions;
    if (result.isSuccess() && std::holds_alternative<std::string>(result.value_internal)) {
        std::string sessionIds = std::get<std::string>(result.value_internal);
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

std::string JSEngine::getParentSessionId(const std::string &sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second.parentSessionId;
    }

    return "";  // Session not found or no parent
}

// === Session ID Generation ===

uint64_t JSEngine::generateSessionId() const {
    // REFACTOR: Use centralized UniqueIdGenerator for consistency
    return UniqueIdGenerator::generateNumericSessionId();
}

std::string JSEngine::generateSessionIdString(const std::string &prefix) const {
    // REFACTOR: Use centralized UniqueIdGenerator instead of duplicate logic
    return UniqueIdGenerator::generateSessionId(prefix);
}

// === Session Cleanup Hooks ===

void JSEngine::registerEventDispatcher(const std::string &sessionId,
                                       std::shared_ptr<IEventDispatcher> eventDispatcher) {
    if (!eventDispatcher) {
        LOG_WARN("JSEngine: Attempted to register null EventDispatcher for session: {}", sessionId);
        return;
    }

    std::lock_guard<std::mutex> lock(eventDispatchersMutex_);
    eventDispatchers_[sessionId] = eventDispatcher;
    LOG_DEBUG("JSEngine: Registered EventDispatcher for session: {}", sessionId);
}

void JSEngine::unregisterEventDispatcher(const std::string &sessionId) {
    std::lock_guard<std::mutex> lock(eventDispatchersMutex_);
    auto it = eventDispatchers_.find(sessionId);
    if (it != eventDispatchers_.end()) {
        eventDispatchers_.erase(it);
        LOG_DEBUG("JSEngine: Unregistered EventDispatcher for session: {}", sessionId);
    }
}

// === JavaScript Execution ===

std::future<JSResult> JSEngine::executeScript(const std::string &sessionId, const std::string &script) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::EXECUTE_SCRIPT, sessionId);
    request->code = script;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::evaluateExpression(const std::string &sessionId, const std::string &expression) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::EVALUATE_EXPRESSION, sessionId);
    request->code = expression;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::validateExpression(const std::string &sessionId, const std::string &expression) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::VALIDATE_EXPRESSION, sessionId);
    request->code = expression;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::setVariable(const std::string &sessionId, const std::string &name,
                                            const ScriptValue &value) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::SET_VARIABLE, sessionId);
    request->variableName = name;
    request->variableValue = value;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::setVariableAsDOM(const std::string &sessionId, const std::string &name,
                                                 const std::string &xmlContent) {
    // W3C SCXML B.2: Set variable to XML DOM object
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::SET_VARIABLE, sessionId);
    request->variableName = name;
    request->code = xmlContent;
    request->isDOMObject = true;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue setVariableAsDOM operation - size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::getVariable(const std::string &sessionId, const std::string &name) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::GET_VARIABLE, sessionId);
    request->variableName = name;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSEngine::setCurrentEvent(const std::string &sessionId, const std::shared_ptr<Event> &event) {
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

std::future<JSResult> JSEngine::setCurrentEvent(const std::string &sessionId, const std::string &eventName,
                                                const std::string &eventData, const std::string &eventType,
                                                const std::string &sendId, const std::string &origin) {
    // For AOT engine: Create simple Event object from string parameters
    auto event = std::make_shared<Event>(eventName, eventType);
    if (!eventData.empty()) {
        event->setRawJsonData(eventData);
    }
    // W3C SCXML 5.10.1: Set sendid if provided (test332)
    if (!sendId.empty()) {
        event->setSendId(sendId);
    }
    // W3C SCXML 5.10.1: Set origin if provided (test336)
    if (!origin.empty()) {
        event->setOrigin(origin);
    }

    // Delegate to Event object version
    return setCurrentEvent(sessionId, event);
}

std::future<JSResult> JSEngine::setupSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                                     const std::vector<std::string> &ioProcessors) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::SETUP_SYSTEM_VARIABLES, sessionId);
    request->sessionName = sessionName;
    request->ioProcessors = ioProcessors;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
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
        const_cast<JSEngine *>(this)->requestQueue_.push(std::move(request));
    }
    const_cast<JSEngine *>(this)->queueCondition_.notify_one();

    auto result = future.get();
    if (result.isSuccess() && std::holds_alternative<int64_t>(result.value_internal)) {
        return static_cast<size_t>(std::get<int64_t>(result.value_internal));
    }
    return 0;
}

void JSEngine::collectGarbage() {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::COLLECT_GARBAGE, "");
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSEngine: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSEngine: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    // Wait for completion but ignore result
    future.get();
}

// === Thread-safe Execution Worker ===

void JSEngine::executionWorker() {
    LOG_DEBUG("JSEngine: Worker LOOP START - Thread ID: {}",
              static_cast<size_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())));

    // Create QuickJS runtime in worker thread to ensure thread safety
    JSRuntime *tempRuntime = JS_NewRuntime();
    if (!tempRuntime) {
        LOG_ERROR("JSEngine: Failed to create QuickJS runtime in worker thread");
        return;
    }
    LOG_DEBUG("JSEngine: QuickJS runtime created in worker thread");

    // RAII: Signal constructor that initialization is complete with proper synchronization
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        runtime_ = tempRuntime;
        queueCondition_.notify_all();
    }
    LOG_DEBUG("JSEngine: Worker thread initialization complete");

    while (!shouldStop_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCondition_.wait(lock, [this] { return !requestQueue_.empty() || shouldStop_; });

        LOG_DEBUG("JSEngine: Worker woke up - shouldStop: {}, queue size: {}", shouldStop_.load(),
                  requestQueue_.size());

        while (!requestQueue_.empty() && !shouldStop_) {
            auto request = std::move(requestQueue_.front());
            requestQueue_.pop();
            lock.unlock();

            LOG_DEBUG("JSEngine: Processing request type: {}", static_cast<int>(request->type));
            try {
                processExecutionRequest(std::move(request));
                LOG_DEBUG("JSEngine: Request processed successfully");
            } catch (const std::exception &e) {
                LOG_ERROR("JSEngine: EXCEPTION in worker thread: {}", e.what());
            }

            lock.lock();
        }
    }

    // Cleanup all sessions with forced garbage collection
    for (auto &pair : sessions_) {
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
        LOG_DEBUG("JSEngine: Worker thread cleaned up QuickJS resources");
    }

    LOG_DEBUG("JSEngine: Worker LOOP END - shouldStop: {}", shouldStop_.load());
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
        case ExecutionRequest::VALIDATE_EXPRESSION:
            result = validateExpressionInternal(request->sessionId, request->code);
            break;
        case ExecutionRequest::SET_VARIABLE: {
            // W3C SCXML B.2: Check if this is a DOM object request
            if (request->isDOMObject) {
                // Create DOM object from XML content
                SessionContext *session = getSession(request->sessionId);
                if (session && session->jsContext) {
                    JSContext *ctx = session->jsContext;
                    ::JSValue domObject = RSM::DOMBinding::createDOMObject(ctx, request->code);

                    if (JS_IsException(domObject)) {
                        result = createErrorFromException(ctx);
                    } else {
                        // Set the variable
                        ::JSValue global = JS_GetGlobalObject(ctx);
                        int setResult = JS_SetPropertyStr(ctx, global, request->variableName.c_str(), domObject);
                        JS_FreeValue(ctx, global);

                        if (setResult < 0) {
                            result = JSResult::createError("Failed to set DOM variable: " + request->variableName);
                        } else {
                            session->preInitializedVars.insert(request->variableName);
                            result = JSResult::createSuccess();
                        }
                    }
                } else {
                    result = JSResult::createError("Session not found: " + request->sessionId);
                }
            } else {
                // Normal variable setting
                result = setVariableInternal(request->sessionId, request->variableName, request->variableValue);
            }
        } break;
        case ExecutionRequest::GET_VARIABLE:
            result = getVariableInternal(request->sessionId, request->variableName);
            break;
        case ExecutionRequest::SET_CURRENT_EVENT:
            result = setCurrentEventInternal(request->sessionId, request->event);
            break;
        case ExecutionRequest::SETUP_SYSTEM_VARIABLES:
            result = setupSystemVariablesInternal(request->sessionId, request->sessionName, request->ioProcessors);
            break;
        case ExecutionRequest::CREATE_SESSION: {
            bool success = createSessionInternal(request->sessionId, request->parentSessionId);
            result = success ? JSResult::createSuccess() : JSResult::createError("Failed to create session");
        } break;
        case ExecutionRequest::DESTROY_SESSION: {
            bool success = destroySessionInternal(request->sessionId);
            result = success ? JSResult::createSuccess() : JSResult::createError("Failed to destroy session");
        } break;
        case ExecutionRequest::HAS_SESSION: {
            LOG_DEBUG("JSEngine: HAS_SESSION check for '{}' - sessions_ map size: {}", request->sessionId,
                      sessions_.size());
            bool exists = sessions_.find(request->sessionId) != sessions_.end();
            LOG_DEBUG("JSEngine: Session '{}' exists: {}", request->sessionId, exists);
            result = exists ? JSResult::createSuccess() : JSResult::createError("Session not found");
        } break;
        case ExecutionRequest::GET_ACTIVE_SESSIONS: {
            std::string sessionIds;
            for (const auto &[sessionId, _] : sessions_) {
                if (!sessionIds.empty()) {
                    sessionIds += ",";
                }
                sessionIds += sessionId;
            }
            result = JSResult::createSuccess(sessionIds);
        } break;
        case ExecutionRequest::GET_MEMORY_USAGE: {
            if (runtime_) {
                JSMemoryUsage usage;
                JS_ComputeMemoryUsage(runtime_, &usage);
                result = JSResult::createSuccess(static_cast<int64_t>(usage.memory_used_size));
            } else {
                result = JSResult::createSuccess(static_cast<int64_t>(0));
            }
        } break;
        case ExecutionRequest::COLLECT_GARBAGE: {
            if (runtime_) {
                JS_RunGC(runtime_);
            }
            result = JSResult::createSuccess();
        } break;
        case ExecutionRequest::SHUTDOWN_ENGINE: {
            // Cleanup all sessions
            for (auto &[sessionId, session] : sessions_) {
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
            LOG_DEBUG("JSEngine: Worker thread cleaned up QuickJS resources");
        } break;
        }

        request->promise.set_value(result);

    } catch (const std::exception &e) {
        request->promise.set_value(JSResult::createError("Exception: " + std::string(e.what())));
    }
}

// === Internal Implementation (Part 1) ===

bool JSEngine::createSessionInternal(const std::string &sessionId, const std::string &parentSessionId) {
    // Validate session ID is not empty
    if (sessionId.empty()) {
        LOG_ERROR("JSEngine: Session ID cannot be empty");
        return false;
    }

    if (sessions_.find(sessionId) != sessions_.end()) {
        LOG_ERROR("JSEngine: Session already exists: {}", sessionId);
        return false;
    }

    // Runtime is guaranteed to exist in worker thread
    // Create QuickJS context
    JSContext *ctx = JS_NewContext(runtime_);
    if (!ctx) {
        LOG_ERROR("JSEngine: Failed to create context for session: {}", sessionId);
        return false;
    }

    // Setup context
    if (!setupQuickJSContext(ctx, sessionId)) {
        JS_FreeContext(ctx);
        return false;
    }

    // Create session info
    SessionContext session;
    session.jsContext = ctx;
    session.sessionId = sessionId;
    session.parentSessionId = parentSessionId;

    sessions_[sessionId] = std::move(session);

    LOG_DEBUG("JSEngine: Created session '{}' - sessions_ map size now: {}", sessionId, sessions_.size());
    return true;
}

bool JSEngine::destroySessionInternal(const std::string &sessionId) {
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }

    // W3C SCXML 6.2: Cancel delayed events for terminating session
    {
        std::lock_guard<std::mutex> lock(eventDispatchersMutex_);
        auto dispatcherIt = eventDispatchers_.find(sessionId);
        if (dispatcherIt != eventDispatchers_.end()) {
            auto eventDispatcher = dispatcherIt->second.lock();
            if (eventDispatcher) {
                size_t cancelledCount = eventDispatcher->cancelEventsForSession(sessionId);
                LOG_DEBUG("JSEngine: Cancelled {} delayed events for session: {}", cancelledCount, sessionId);
            }
            // Remove the registry entry regardless of dispatcher availability
            eventDispatchers_.erase(dispatcherIt);
        }
    }

    // Clean up session file path mapping
    unregisterSessionFilePath(sessionId);

    if (it->second.jsContext) {
        // Force garbage collection before freeing context
        if (runtime_) {
            JS_RunGC(runtime_);
        }
        JS_FreeContext(it->second.jsContext);
    }

    sessions_.erase(it);

    // Clean up EventRaiser from global registry to prevent memory leaks
    auto registry = getEventRaiserRegistry();
    if (registry && registry->hasEventRaiser(sessionId)) {
        bool unregistered = registry->unregisterEventRaiser(sessionId);
        if (unregistered) {
            LOG_DEBUG("JSEngine: Cleaned up EventRaiser for destroyed session: {}", sessionId);
        } else {
            LOG_WARN("JSEngine: Failed to clean up EventRaiser for destroyed session: {}", sessionId);
        }
    }

    LOG_DEBUG("JSEngine: Destroyed session '{}'", sessionId);
    return true;
}

JSEngine::SessionContext *JSEngine::getSession(const std::string &sessionId) {
    auto it = sessions_.find(sessionId);
    return (it != sessions_.end()) ? &it->second : nullptr;
}

bool JSEngine::setupQuickJSContext(JSContext *ctx, const std::string &sessionId) {
    // Set engine instance as context opaque for callbacks
    JS_SetContextOpaque(ctx, this);

    // Setup SCXML-specific builtin functions and objects
    setupSCXMLBuiltins(ctx, sessionId);

    return true;
}

// === SCXML-specific Setup ===

void JSEngine::setupSCXMLBuiltins(JSContext *ctx, [[maybe_unused]] const std::string &sessionId) {
    ::JSValue global = JS_GetGlobalObject(ctx);

    // Setup In() function for state checking
    ::JSValue inFunction = JS_NewCFunction(ctx, inFunctionWrapper, "In", 1);
    JS_SetPropertyStr(ctx, global, "In", inFunction);

    // Setup console object
    setupConsoleObject(ctx);

    // NOTE: QuickJS already has Math object built-in, no need to set it up
    // Removing setupMathObject() improves session creation performance by ~10-15%

    // Setup system variables
    setupSystemVariables(ctx);

    // W3C SCXML 5.10: _event is bound lazily on first event (see JSEngineImpl::setCurrentEventInternal)

    // Bind all registered global functions
    {
        std::lock_guard<std::mutex> lock(globalFunctionsMutex_);
        for (const auto &[name, callback] : globalFunctions_) {
            ::JSValue funcName = JS_NewString(ctx, name.c_str());
            ::JSValue func = JS_NewCFunctionData(ctx, globalFunctionWrapper, -1, 0, 1, &funcName);
            JS_SetPropertyStr(ctx, global, name.c_str(), func);
            JS_FreeValue(ctx, funcName);  // Free the string after using it
            LOG_DEBUG("JSEngine: Bound registered global function '{}' to JavaScript context", name);
        }
    }

    JS_FreeValue(ctx, global);
}

void JSEngine::setupEventObject(JSContext *ctx, const std::string &sessionId) {
    ::JSValue global = JS_GetGlobalObject(ctx);

    // Register native function for error event queueing (SOLID: Interface Segregation)
    ::JSValue queueErrorFunc = JS_NewCFunction(ctx, queueErrorEventWrapper, "_queueErrorEvent", 2);
    JS_SetPropertyStr(ctx, global, "_queueErrorEvent", queueErrorFunc);

    // Create a SCXML W3C compliant read-only _event object using JavaScript
    // This approach uses Object.defineProperty with getters to enforce read-only behavior
    std::string eventSetupCode = R"(
        (function() {
            var sessionId = ')" + sessionId +
                                 R"(';
            // Global event data object that C++ can access directly
            this.__eventData = {
                name: '',
                type: '',
                sendid: '',
                origin: '',
                origintype: '',
                invokeid: '',
                data: null
            };

            // Create the _event object with read-only properties
            var eventObject = {};
            Object.defineProperty(this, '_event', {
                get: function() { return eventObject; },
                set: function(value) {
                    // SCXML W3C Spec: Attempts to modify system variables should fail
                    console.log('RSM Error: Attempt to assign to read-only system variable _event');
                    // Queue error.execution event per SCXML W3C specification
                    _queueErrorEvent(sessionId, 'error.execution');
                    throw new Error('Cannot assign to read-only system variable _event');
                },
                enumerable: true,
                configurable: false
            });

            // Define each property with getter only to make them read-only
            var eventProps = ['name', 'type', 'sendid', 'origin', 'origintype', 'invokeid', 'data'];
            for (var i = 0; i < eventProps.length; i++) {
                (function(propName) {
                    Object.defineProperty(_event, propName, {
                        get: function() { return __eventData[propName]; },
                        set: function(value) {
                            // SCXML W3C Spec: Attempts to modify system variables should fail
                            // and place 'error.execution' on internal event queue
                            console.log('RSM Error: Attempt to modify read-only system variable _event.' + propName);
                            // Queue error.execution event per SCXML W3C specification
                            _queueErrorEvent(sessionId, 'error.execution');
                            throw new Error('Cannot modify read-only system variable _event.' + propName);
                        },
                        enumerable: true,
                        configurable: false
                    });
                })(eventProps[i]);
            }

            // C++ directly accesses __eventData, no helper function needed

            return true;
        }).call(this);
    )";

    ::JSValue result =
        JS_Eval(ctx, eventSetupCode.c_str(), eventSetupCode.length(), "<event_setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        LOG_ERROR("JSEngine: Failed to setup _event object");
        ::JSValue exception = JS_GetException(ctx);
        const char *errorStr = JS_ToCString(ctx, exception);
        if (errorStr) {
            LOG_ERROR("JSEngine: _event setup error: {}", errorStr);
            JS_FreeCString(ctx, errorStr);
        }
        JS_FreeValue(ctx, exception);
    }
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, global);
}

void JSEngine::setupConsoleObject(JSContext *ctx) {
    ::JSValue global = JS_GetGlobalObject(ctx);
    ::JSValue consoleObj = JS_NewObject(ctx);

    // Setup console.log function
    ::JSValue logFunction = JS_NewCFunction(ctx, consoleFunctionWrapper, "log", 1);
    JS_SetPropertyStr(ctx, consoleObj, "log", logFunction);

    // Set console in global scope
    JS_SetPropertyStr(ctx, global, "console", consoleObj);
    JS_FreeValue(ctx, global);
}

void JSEngine::setupMathObject(JSContext *ctx) {
    // Add basic Math object support through JavaScript
    const char *mathCode = R"(
        if (typeof Math === 'undefined') {
            Math = {
                max: function() {
                    var max = arguments[0];
                    for (var i = 1; i < arguments.length; i++) {
                        if (arguments[i] > max) max = arguments[i];
                    }
                    return max;
                },
                min: function() {
                    var min = arguments[0];
                    for (var i = 1; i < arguments.length; i++) {
                        if (arguments[i] < min) min = arguments[i];
                    }
                    return min;
                },
                PI: 3.141592653589793,
                abs: function(x) { return x < 0 ? -x : x; },
                floor: function(x) { return Math.floor ? Math.floor(x) : parseInt(x); },
                ceil: function(x) { return Math.ceil ? Math.ceil(x) : parseInt(x) + (x > parseInt(x) ? 1 : 0); }
            };
        }
    )";

    ::JSValue result = JS_Eval(ctx, mathCode, strlen(mathCode), "<math>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(ctx, result);
}

void JSEngine::setupSystemVariables(JSContext *ctx) {
    ::JSValue global = JS_GetGlobalObject(ctx);

    // Setup _sessionid (unique identifier for this session)
    // In a real implementation, this would be provided by the SCXML engine
    std::string sessionId = "session_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                            std::chrono::system_clock::now().time_since_epoch())
                                                            .count());
    JS_SetPropertyStr(ctx, global, "_sessionid", JS_NewString(ctx, sessionId.c_str()));

    // Setup _name (from <scxml> element name attribute)
    JS_SetPropertyStr(ctx, global, "_name", JS_NewString(ctx, "RSMStateMachine"));

    // Setup _ioprocessors (Event I/O Processors)
    ::JSValue ioprocessors = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, global, "_ioprocessors", ioprocessors);

    JS_FreeValue(ctx, global);
}

// === Static callback functions ===

::JSValue JSEngine::inFunctionWrapper(JSContext *ctx, JSValue /*this_val*/, int argc, JSValue *argv) {
    if (argc != 1) {
        JS_ThrowSyntaxError(ctx, "In() function requires exactly one argument");
        return JS_EXCEPTION;
    }

    // Get the state name argument
    const char *stateName = JS_ToCString(ctx, argv[0]);
    if (!stateName) {
        JS_ThrowTypeError(ctx, "In() function argument must be a string");
        return JS_EXCEPTION;
    }

    // SCXML W3C Section 5.9.2: In() predicate function
    std::string stateNameStr(stateName);
    bool result = JSEngine::instance().checkStateActive(stateNameStr);

    JS_FreeCString(ctx, stateName);
    return JS_NewBool(ctx, result);
}

::JSValue JSEngine::consoleFunctionWrapper(JSContext *ctx, JSValue /*this_val*/, int argc, JSValue *argv) {
    std::stringstream ss;

    for (int i = 0; i < argc; i++) {
        if (i > 0) {
            ss << " ";
        }

        const char *str = JS_ToCString(ctx, argv[i]);
        if (str) {
            ss << str;
            JS_FreeCString(ctx, str);
        } else {
            ss << "[object]";
        }
    }

    // Log to our RSM logging system
    // For now, just print to stderr for testing
    LOG_INFO("RSM console.log: {}", ss.str());
    return JS_UNDEFINED;
}

::JSValue JSEngine::queueErrorEventWrapper(JSContext *ctx, JSValue /*this_val*/, int argc, JSValue *argv) {
    if (argc < 2) {
        return JS_UNDEFINED;
    }

    // Get sessionId from first argument
    const char *sessionId = JS_ToCString(ctx, argv[0]);
    // Get event name from second argument
    const char *eventName = JS_ToCString(ctx, argv[1]);

    if (sessionId && eventName) {
        // Get JSEngine instance through static access (SOLID: Dependency Inversion)
        JSEngine::instance().queueInternalEvent(std::string(sessionId), std::string(eventName));
        LOG_DEBUG("JSEngine: Queued internal event '{}' for session '{}'", eventName, sessionId);
    }

    if (sessionId) {
        JS_FreeCString(ctx, sessionId);
    }
    if (eventName) {
        JS_FreeCString(ctx, eventName);
    }

    return JS_UNDEFINED;
}

::JSValue JSEngine::globalFunctionWrapper(JSContext *ctx, JSValue this_val, int argc, JSValue *argv, int magic,
                                          JSValue *func_data) {
    (void)this_val;  // Unused parameter
    (void)magic;     // Unused parameter

    // 1. Extract function name from func_data[0]
    const char *funcName = JS_ToCString(ctx, func_data[0]);
    if (!funcName) {
        return JS_ThrowTypeError(ctx, "Invalid function data");
    }

    // 2. Get JSEngine instance and find callback in globalFunctions_ map
    JSEngine *engine = static_cast<JSEngine *>(JS_GetContextOpaque(ctx));
    if (!engine) {
        JS_FreeCString(ctx, funcName);
        return JS_ThrowInternalError(ctx, "Engine instance not found in context");
    }

    std::function<ScriptValue(const std::vector<ScriptValue> &)> callback;
    {
        std::lock_guard<std::mutex> lock(engine->globalFunctionsMutex_);
        auto it = engine->globalFunctions_.find(funcName);
        if (it == engine->globalFunctions_.end()) {
            JS_FreeCString(ctx, funcName);
            return JS_ThrowReferenceError(ctx, "Function not found: %s", funcName);
        }
        callback = it->second;
    }

    LOG_DEBUG("JSEngine: Calling registered global function: {}", funcName);
    JS_FreeCString(ctx, funcName);

    // 3. Convert JSValue arguments to ScriptValue vector
    std::vector<ScriptValue> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(engine->quickJSToJSValue(ctx, argv[i]));
    }

    // 4. Call C++ callback
    try {
        ScriptValue result = callback(args);

        // 5. Convert ScriptValue result back to JSValue
        return engine->jsValueToQuickJS(ctx, result);
    } catch (const std::exception &e) {
        return JS_ThrowInternalError(ctx, "Global function execution failed: %s", e.what());
    }
}

bool JSEngine::checkStateActive(const std::string &stateName) const {
    std::lock_guard<std::mutex> lock(stateMachinesMutex_);

    // Check all registered StateMachines for the given state
    for (const auto &pair : stateMachines_) {
        StateMachine *sm = pair.second;
        if (sm && sm->isStateActive(stateName)) {
            return true;
        }
    }
    return false;
}

bool JSEngine::registerGlobalFunction(const std::string &functionName,
                                      std::function<ScriptValue(const std::vector<ScriptValue> &)> callback) {
    if (functionName.empty() || !callback) {
        LOG_ERROR("JSEngine: Invalid function name or callback for global function registration");
        return false;
    }

    std::lock_guard<std::mutex> lock(globalFunctionsMutex_);
    globalFunctions_[functionName] = std::move(callback);

    LOG_DEBUG("JSEngine: Registered global function: {}", functionName);
    return true;
}

void JSEngine::queueInternalEvent(const std::string &sessionId, const std::string &eventName) {
    std::lock_guard<std::mutex> lock(internalEventQueuesMutex_);

    // Create queue for session if it doesn't exist
    if (internalEventQueues_.find(sessionId) == internalEventQueues_.end()) {
        internalEventQueues_[sessionId] = InternalEventQueue{};
    }

    std::lock_guard<std::mutex> queueLock(*internalEventQueues_[sessionId].mutex);
    internalEventQueues_[sessionId].events.push(eventName);

    LOG_DEBUG("JSEngine: Queued internal event '{}' for session '{}'", eventName, sessionId);
}

void JSEngine::setStateMachine(StateMachine *stateMachine, const std::string &sessionId) {
    std::lock_guard<std::mutex> lock(stateMachinesMutex_);
    if (stateMachine) {
        stateMachines_[sessionId] = stateMachine;
        LOG_DEBUG("JSEngine: StateMachine set for session: {}", sessionId);
    } else {
        auto it = stateMachines_.find(sessionId);
        if (it != stateMachines_.end()) {
            stateMachines_.erase(it);
            LOG_DEBUG("JSEngine: StateMachine removed for session: {}", sessionId);
        }
    }
}

// ===================================================================
// INTEGRATED RESULT PROCESSING IMPLEMENTATION
// ===================================================================

bool JSEngine::resultToBool(const JSResult &result) {
    if (!result.success_internal) {
        return false;
    }

    // REUSE: Proven boolean conversion logic from ActionExecutorImpl
    if (std::holds_alternative<bool>(result.value_internal)) {
        return std::get<bool>(result.value_internal);
    } else if (std::holds_alternative<long>(result.value_internal)) {
        return std::get<long>(result.value_internal) != 0;
    } else if (std::holds_alternative<double>(result.value_internal)) {
        return std::get<double>(result.value_internal) != 0.0;
    } else if (std::holds_alternative<std::string>(result.value_internal)) {
        return !std::get<std::string>(result.value_internal).empty();
    }
    return false;
}

std::string JSEngine::resultToString(const JSResult &result, const std::string &sessionId,
                                     const std::string &originalExpression) {
    if (!result.success_internal) {
        return "";
    }

    if (std::holds_alternative<std::string>(result.value_internal)) {
        return result.getValue<std::string>();
    } else if (std::holds_alternative<double>(result.value_internal)) {
        double val = result.getValue<double>();
        // Check if it's an integer value
        if (val == std::floor(val)) {
            return std::to_string(static_cast<int64_t>(val));
        } else {
            // SCXML W3C Compliance: Use ECMAScript-compatible number formatting
            std::ostringstream oss;
            oss << std::noshowpoint << val;
            std::string str = oss.str();

            // Remove trailing zeros after decimal point (JavaScript behavior)
            if (str.find('.') != std::string::npos) {
                str.erase(str.find_last_not_of('0') + 1, std::string::npos);
                if (str.back() == '.') {
                    str.pop_back();
                }
            }
            return str;
        }
    } else if (std::holds_alternative<int64_t>(result.value_internal)) {
        return std::to_string(result.getValue<int64_t>());
    } else if (std::holds_alternative<bool>(result.value_internal)) {
        return result.getValue<bool>() ? "true" : "false";
    } else if (!sessionId.empty() && !originalExpression.empty()) {
        // REUSE: Proven JSON.stringify fallback logic
        std::string stringifyExpr = "JSON.stringify(" + originalExpression + ")";
        auto stringifyResult = RSM::JSEngine::instance().evaluateExpression(sessionId, stringifyExpr).get();
        if (stringifyResult.isSuccess()) {
            return stringifyResult.getValue<std::string>();
        }
        return "[object]";
    }
    return "[conversion_error]";
}

std::vector<std::string> JSEngine::resultToStringArray(const JSResult &result, const std::string &sessionId) {
    // SOLID: Delegate to expression-aware version (Single Responsibility)
    return resultToStringArray(result, sessionId, "");
}

std::vector<std::string> JSEngine::resultToStringArray(const JSResult &result, const std::string &sessionId,
                                                       const std::string &originalExpression) {
    std::vector<std::string> arrayValues;

    LOG_DEBUG("resultToStringArray: Starting with sessionId='{}', originalExpression='{}'", sessionId,
              originalExpression);

    if (!result.success_internal) {
        LOG_DEBUG("resultToStringArray: Result not successful, returning empty array");
        return arrayValues;
    }

    std::string arrayStr;

    // SOLID: Handle all ScriptValue types internally (Single Responsibility)
    if (std::holds_alternative<std::string>(result.value_internal)) {
        arrayStr = std::get<std::string>(result.value_internal);
        LOG_DEBUG("resultToStringArray: Got string result: '{}'", arrayStr);
    } else {
        LOG_DEBUG("resultToStringArray: Result is not string type, attempting JSON.stringify conversion");
        // SOLID: For non-string types, convert to JSON string using proven logic
        // This handles array objects, numbers, booleans, etc.
        if (!sessionId.empty() && !originalExpression.empty()) {
            // Use JSON.stringify for reliable array conversion
            std::string stringifyExpr = "JSON.stringify(" + originalExpression + ")";
            LOG_DEBUG("resultToStringArray: Evaluating stringify expression: '{}'", stringifyExpr);
            auto stringifyResult = RSM::JSEngine::instance().evaluateExpression(sessionId, stringifyExpr).get();
            if (stringifyResult.isSuccess() && std::holds_alternative<std::string>(stringifyResult.value_internal)) {
                arrayStr = std::get<std::string>(stringifyResult.value_internal);
                LOG_DEBUG("resultToStringArray: JSON.stringify succeeded, result: '{}'", arrayStr);
            } else {
                LOG_DEBUG("resultToStringArray: JSON.stringify failed or returned non-string");
                return arrayValues;  // Failed to convert to JSON string
            }
        } else {
            LOG_DEBUG("resultToStringArray: Missing sessionId or originalExpression for non-string type");
            return arrayValues;  // Cannot process non-string types without session context
        }
    }

    LOG_DEBUG("resultToStringArray: Final arrayStr before processing: '{}'", arrayStr);

    // SOLID: Use JSON-based approach for reliable array parsing
    // This correctly handles nested arrays like [[1,2],[3,4]] and all JavaScript types
    if (!arrayStr.empty() && !sessionId.empty()) {
        LOG_DEBUG("resultToStringArray: Processing array using JSON approach");

        try {
            // W3C SCXML B.2 (test 457): Validate that value is actually an array
            // Must check instanceof Array before attempting to iterate
            std::string arrayCheckExpr = originalExpression + " instanceof Array";
            LOG_DEBUG("resultToStringArray: Validating array type with expression: '{}'", arrayCheckExpr);
            auto arrayCheckResult = RSM::JSEngine::instance().evaluateExpression(sessionId, arrayCheckExpr).get();

            if (!arrayCheckResult.isSuccess() || !std::holds_alternative<bool>(arrayCheckResult.value_internal) ||
                !std::get<bool>(arrayCheckResult.value_internal)) {
                LOG_DEBUG(
                    "resultToStringArray: Value is not an array (instanceof Array check failed), returning empty");
                return arrayValues;  // Not an array, caller should check and raise error.execution
            }

            // SCXML W3C Compliance: Use original expression to preserve null/undefined distinction
            std::string setVarExpr = "var _tempArray = " + originalExpression + "; _tempArray.length";
            LOG_DEBUG("resultToStringArray: Evaluating temp variable length expression: '{}'", setVarExpr);
            auto lengthResult = RSM::JSEngine::instance().evaluateExpression(sessionId, setVarExpr).get();

            LOG_DEBUG("resultToStringArray: Length result type index: {}", lengthResult.value_internal.index());

            int64_t arrayLength = 0;
            bool lengthValid = false;

            if (lengthResult.isSuccess()) {
                if (std::holds_alternative<int64_t>(lengthResult.value_internal)) {
                    arrayLength = std::get<int64_t>(lengthResult.value_internal);
                    lengthValid = true;
                    LOG_DEBUG("resultToStringArray: Got int64_t array length: {}", arrayLength);
                } else if (std::holds_alternative<double>(lengthResult.value_internal)) {
                    double doubleLength = std::get<double>(lengthResult.value_internal);
                    arrayLength = static_cast<int64_t>(doubleLength);
                    lengthValid = true;
                    LOG_DEBUG("resultToStringArray: Got double array length: {} -> {}", doubleLength, arrayLength);
                }
            }

            if (lengthValid) {
                // Iterate through array elements using temporary variable approach
                for (int64_t i = 0; i < arrayLength; ++i) {
                    // SCXML W3C: Check for undefined first, then use JSON.stringify for other types
                    std::string typeCheckExpr = "typeof _tempArray[" + std::to_string(i) + "]";
                    auto typeResult = RSM::JSEngine::instance().evaluateExpression(sessionId, typeCheckExpr).get();

                    if (typeResult.isSuccess() && std::holds_alternative<std::string>(typeResult.value_internal)) {
                        std::string typeStr = std::get<std::string>(typeResult.value_internal);

                        if (typeStr == "undefined") {
                            // Preserve undefined values exactly
                            arrayValues.push_back("undefined");
                            LOG_DEBUG("resultToStringArray: Element {} is undefined", i);
                            continue;
                        }
                    }

                    // For non-undefined values, use JSON.stringify
                    std::string elementExpr = "JSON.stringify(_tempArray[" + std::to_string(i) + "])";
                    LOG_DEBUG("resultToStringArray: Element {} expression: '{}'", i, elementExpr);
                    auto elementResult = RSM::JSEngine::instance().evaluateExpression(sessionId, elementExpr).get();

                    if (elementResult.isSuccess() &&
                        std::holds_alternative<std::string>(elementResult.value_internal)) {
                        std::string elementStr = std::get<std::string>(elementResult.value_internal);
                        LOG_DEBUG("resultToStringArray: Element {} result: '{}'", i, elementStr);
                        // Remove quotes for primitive types, keep JSON for complex types
                        if (elementStr.length() >= 2 && elementStr.front() == '"' && elementStr.back() == '"') {
                            // String value - remove quotes
                            arrayValues.push_back(elementStr.substr(1, elementStr.length() - 2));
                        } else {
                            // Non-string value (number, boolean, array, object) - keep as JSON
                            arrayValues.push_back(elementStr);
                        }
                    }
                }
            } else {
                LOG_DEBUG("resultToStringArray: Length evaluation failed - success: {}, error: '{}'",
                          lengthResult.isSuccess(),
                          lengthResult.isSuccess() ? "no error" : lengthResult.errorMessage_internal);
            }
        } catch (const std::exception &e) {
            LOG_ERROR("resultToStringArray: Exception during JSON processing: {}", e.what());
        }
    }

    LOG_DEBUG("resultToStringArray: Returning {} elements", arrayValues.size());
    return arrayValues;
}

void JSEngine::requireSuccess(const JSResult &result, const std::string &operation) {
    if (!result.success_internal) {
        throw std::runtime_error("JSEngine operation failed: " + operation + " - " + result.errorMessage_internal);
    }
}

bool JSEngine::isSuccess(const JSResult &result) noexcept {
    return result.success_internal;
}

bool JSEngine::isVariablePreInitialized(const std::string &sessionId, const std::string &variableName) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }
    return it->second.preInitializedVars.find(variableName) != it->second.preInitializedVars.end();
}

// === Invoke Session Management Implementation ===

void JSEngine::registerInvokeMapping(const std::string &parentSessionId, const std::string &invokeId,
                                     const std::string &childSessionId) {
    std::lock_guard<std::mutex> lock(invokeMappingsMutex_);
    invokeMappings_[parentSessionId][invokeId] = childSessionId;
    LOG_DEBUG("JSEngine: Registered invoke mapping - parent: {}, invoke: {}, child: {}", parentSessionId, invokeId,
              childSessionId);
}

std::string JSEngine::getInvokeSessionId(const std::string &parentSessionId, const std::string &invokeId) const {
    std::lock_guard<std::mutex> lock(invokeMappingsMutex_);

    auto parentIt = invokeMappings_.find(parentSessionId);
    if (parentIt == invokeMappings_.end()) {
        LOG_DEBUG("JSEngine: No invoke mappings found for parent session: {}", parentSessionId);
        return "";
    }

    auto invokeIt = parentIt->second.find(invokeId);
    if (invokeIt == parentIt->second.end()) {
        LOG_DEBUG("JSEngine: Invoke ID '{}' not found in parent session: {}", invokeId, parentSessionId);
        return "";
    }

    LOG_DEBUG("JSEngine: Found invoke mapping - parent: {}, invoke: {}, child: {}", parentSessionId, invokeId,
              invokeIt->second);
    return invokeIt->second;
}

void JSEngine::unregisterInvokeMapping(const std::string &parentSessionId, const std::string &invokeId) {
    std::lock_guard<std::mutex> lock(invokeMappingsMutex_);

    auto parentIt = invokeMappings_.find(parentSessionId);
    if (parentIt != invokeMappings_.end()) {
        parentIt->second.erase(invokeId);

        // Clean up empty parent entries
        if (parentIt->second.empty()) {
            invokeMappings_.erase(parentIt);
        }

        LOG_DEBUG("JSEngine: Unregistered invoke mapping - parent: {}, invoke: {}", parentSessionId, invokeId);
    }
}

std::string JSEngine::getInvokeIdForChildSession(const std::string &childSessionId) const {
    std::lock_guard<std::mutex> lock(invokeMappingsMutex_);

    // W3C SCXML 5.10 test 338: Reverse lookup childSessionId -> invokeId
    // Iterate through all parent sessions to find the invokeId that created this child
    for (const auto &parentEntry : invokeMappings_) {
        for (const auto &invokeEntry : parentEntry.second) {
            if (invokeEntry.second == childSessionId) {
                LOG_DEBUG("JSEngine: Found invokeId '{}' for child session '{}' in parent '{}'", invokeEntry.first,
                          childSessionId, parentEntry.first);
                return invokeEntry.first;
            }
        }
    }

    LOG_DEBUG("JSEngine: No invokeId found for child session: {}", childSessionId);
    return "";
}

void JSEngine::registerSessionFilePath(const std::string &sessionId, const std::string &filePath) {
    std::lock_guard<std::mutex> lock(sessionFilePathsMutex_);
    sessionFilePaths_[sessionId] = filePath;
    LOG_DEBUG("JSEngine: Registered session file path - session: {}, path: {}", sessionId, filePath);
}

std::string JSEngine::getSessionFilePath(const std::string &sessionId) const {
    std::lock_guard<std::mutex> lock(sessionFilePathsMutex_);

    auto it = sessionFilePaths_.find(sessionId);
    if (it == sessionFilePaths_.end()) {
        LOG_DEBUG("JSEngine: No file path found for session: {}", sessionId);
        return "";
    }

    LOG_DEBUG("JSEngine: Found session file path - session: {}, path: {}", sessionId, it->second);
    return it->second;
}

void JSEngine::unregisterSessionFilePath(const std::string &sessionId) {
    std::lock_guard<std::mutex> lock(sessionFilePathsMutex_);

    auto it = sessionFilePaths_.find(sessionId);
    if (it != sessionFilePaths_.end()) {
        sessionFilePaths_.erase(it);
        LOG_DEBUG("JSEngine: Unregistered session file path - session: {}", sessionId);
    }
}

void JSEngine::initializeEventRaiserService() {
    try {
        // Create registry and use JSEngine directly as session manager
        auto registry = std::make_shared<EventRaiserRegistry>();

        // JSEngine implements ISessionManager directly - no adapter needed
        EventRaiserService::initialize(registry, std::shared_ptr<ISessionManager>(this, [](ISessionManager *) {
                                           // Custom deleter that does nothing - JSEngine is a singleton
                                       }));

        LOG_DEBUG("JSEngine: EventRaiserService initialized with dependency injection");
    } catch (const std::exception &e) {
        LOG_ERROR("JSEngine: Failed to initialize EventRaiserService: {}", e.what());
        throw;
    }
}

std::shared_ptr<IEventRaiserRegistry> JSEngine::getEventRaiserRegistry() {
    // Delegate to EventRaiserService for consistency
    try {
        return EventRaiserService::getInstance().getRegistry();
    } catch (const std::exception &e) {
        LOG_ERROR("JSEngine: Failed to get EventRaiserRegistry: {}", e.what());
        // Fallback to static creation for backward compatibility
        static std::shared_ptr<IEventRaiserRegistry> fallbackRegistry = std::make_shared<EventRaiserRegistry>();
        return fallbackRegistry;
    }
}

void JSEngine::clearEventRaiserRegistry() {
    try {
        EventRaiserService::getInstance().clearAll();
        LOG_DEBUG("JSEngine: EventRaiser registry cleared via EventRaiserService");
    } catch (const std::exception &e) {
        LOG_ERROR("JSEngine: Failed to clear EventRaiser registry: {}", e.what());
        // Fallback to old method for backward compatibility
        auto registry = getEventRaiserRegistry();
        if (registry) {
            auto concreteRegistry = std::dynamic_pointer_cast<EventRaiserRegistry>(registry);
            if (concreteRegistry) {
                concreteRegistry->clear();
                LOG_DEBUG("JSEngine: EventRaiser registry cleared using fallback method");
            }
        }
    }
}

// === Observer Pattern Support (Temporary implementation until Facade refactoring) ===

void JSEngine::addObserver(ISessionObserver *observer) {
    // Temporary implementation - will be delegated to SessionManager after refactoring
    (void)observer;  // Suppress unused parameter warning
    LOG_DEBUG("JSEngine: Observer support not yet implemented in current architecture");
    // TODO: Delegate to internal SessionManager after Facade pattern implementation
}

void JSEngine::removeObserver(ISessionObserver *observer) {
    // Temporary implementation - will be delegated to SessionManager after refactoring
    (void)observer;  // Suppress unused parameter warning
    LOG_DEBUG("JSEngine: Observer support not yet implemented in current architecture");
    // TODO: Delegate to internal SessionManager after Facade pattern implementation
}

// JSEngine internal functions are implemented in JSEngineImpl.cpp

}  // namespace RSM
