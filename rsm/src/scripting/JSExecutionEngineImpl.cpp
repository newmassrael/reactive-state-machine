#include "scripting/JSExecutionEngineImpl.h"
#include "SCXMLTypes.h"
#include "common/Logger.h"
#include "runtime/StateMachine.h"
#include <chrono>
#include <cstring>
#include <sstream>

namespace RSM {

JSExecutionEngineImpl::JSExecutionEngineImpl() {
    LOG_DEBUG("JSExecutionEngineImpl: Constructor started");
}

JSExecutionEngineImpl::~JSExecutionEngineImpl() {
    shutdown();
}

bool JSExecutionEngineImpl::initialize() {
    if (initialized_.load()) {
        LOG_DEBUG("JSExecutionEngineImpl: Already initialized");
        return true;
    }

    LOG_DEBUG("JSExecutionEngineImpl: Starting initialization...");

    // Create QuickJS runtime
    runtime_ = JS_NewRuntime();
    if (!runtime_) {
        LOG_ERROR("JSExecutionEngineImpl: Failed to create QuickJS runtime");
        return false;
    }

    // Start execution thread
    shouldStop_ = false;
    executionThread_ = std::thread(&JSExecutionEngineImpl::executionWorker, this);

    initialized_ = true;
    LOG_DEBUG("JSExecutionEngineImpl: Initialization completed");
    return true;
}

void JSExecutionEngineImpl::shutdown() {
    LOG_DEBUG("JSExecutionEngineImpl: shutdown() called - shouldStop: {}", shouldStop_.load());

    if (shouldStop_) {
        LOG_DEBUG("JSExecutionEngineImpl: Already shutting down, returning");
        return;
    }

    // Send shutdown request to worker thread
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::SHUTDOWN_ENGINE, "");
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSExecutionEngineImpl: Queue operation - before enqueue: size={}", requestQueue_.size());
        requestQueue_.push(std::move(request));
        LOG_DEBUG("JSExecutionEngineImpl: Queue operation - after enqueue: size={}", requestQueue_.size());
    }
    queueCondition_.notify_one();

    // Wait for shutdown to complete
    try {
        future.get();
    } catch (const std::exception &e) {
        LOG_ERROR("JSExecutionEngineImpl: Exception during shutdown: {}", e.what());
    }

    // Signal thread to stop and wait for it
    shouldStop_ = true;
    queueCondition_.notify_all();

    if (executionThread_.joinable()) {
        LOG_DEBUG("JSExecutionEngineImpl: Attempting to join worker thread...");
        auto start = std::chrono::steady_clock::now();
        executionThread_.join();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        LOG_DEBUG("JSExecutionEngineImpl: Worker thread joined successfully in {}ms", duration);
    }

    initialized_ = false;
    LOG_DEBUG("JSExecutionEngineImpl: Shutdown complete");
}

bool JSExecutionEngineImpl::isInitialized() const {
    return initialized_.load();
}

// === Core JavaScript Execution ===

std::future<JSResult> JSExecutionEngineImpl::executeScript(const std::string &sessionId, const std::string &script) {
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

std::future<JSResult> JSExecutionEngineImpl::evaluateExpression(const std::string &sessionId,
                                                                const std::string &expression) {
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

std::future<JSResult> JSExecutionEngineImpl::validateExpression(const std::string &sessionId,
                                                                const std::string &expression) {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::VALIDATE_EXPRESSION, sessionId);
    request->code = expression;
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    return future;
}

std::future<JSResult> JSExecutionEngineImpl::setVariable(const std::string &sessionId, const std::string &name,
                                                         const ScriptValue &value) {
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

std::future<JSResult> JSExecutionEngineImpl::getVariable(const std::string &sessionId, const std::string &name) {
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

std::future<JSResult> JSExecutionEngineImpl::setCurrentEvent(const std::string &sessionId,
                                                             const std::shared_ptr<Event> &event) {
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

std::future<JSResult> JSExecutionEngineImpl::setupSystemVariables(const std::string &sessionId,
                                                                  const std::string &sessionName,
                                                                  const std::vector<std::string> &ioProcessors) {
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

bool JSExecutionEngineImpl::registerGlobalFunction(
    const std::string &functionName, std::function<ScriptValue(const std::vector<ScriptValue> &)> callback) {
    if (functionName.empty() || !callback) {
        LOG_ERROR("JSExecutionEngineImpl: Invalid function name or callback for global function registration");
        return false;
    }

    std::lock_guard<std::mutex> lock(globalFunctionsMutex_);
    globalFunctions_[functionName] = std::move(callback);

    LOG_DEBUG("JSExecutionEngineImpl: Registered global function: {}", functionName);
    return true;
}

std::string JSExecutionEngineImpl::getEngineInfo() const {
    return "JSExecutionEngineImpl (QuickJS-based)";
}

size_t JSExecutionEngineImpl::getMemoryUsage() const {
    if (!runtime_) {
        return 0;
    }
    return getMemoryUsageInternal();
}

void JSExecutionEngineImpl::collectGarbage() {
    auto request = std::make_unique<ExecutionRequest>(ExecutionRequest::COLLECT_GARBAGE, "");
    auto future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    queueCondition_.notify_one();

    try {
        future.get();  // Wait for completion
    } catch (const std::exception &e) {
        LOG_ERROR("JSExecutionEngineImpl: Exception during garbage collection: {}", e.what());
    }
}

// === Session Context Management ===

bool JSExecutionEngineImpl::initializeSessionContext(const std::string &sessionId, const std::string &parentSessionId) {
    return createSessionContextInternal(sessionId, parentSessionId);
}

bool JSExecutionEngineImpl::cleanupSessionContext(const std::string &sessionId) {
    return destroySessionContextInternal(sessionId);
}

bool JSExecutionEngineImpl::hasSessionContext(const std::string &sessionId) const {
    std::lock_guard<std::mutex> lock(contextsMutex_);
    return contexts_.find(sessionId) != contexts_.end();
}

bool JSExecutionEngineImpl::isVariablePreInitialized(const std::string &sessionId,
                                                     const std::string &variableName) const {
    std::lock_guard<std::mutex> lock(contextsMutex_);
    auto it = contexts_.find(sessionId);
    if (it == contexts_.end()) {
        return false;
    }
    return it->second.preInitializedVars.find(variableName) != it->second.preInitializedVars.end();
}

// === ISessionObserver Implementation ===

void JSExecutionEngineImpl::onSessionCreated(const std::string &sessionId, const std::string &parentSessionId) {
    LOG_DEBUG("JSExecutionEngineImpl: Observer notification - session created: {}", sessionId);

    if (!createSessionContextInternal(sessionId, parentSessionId)) {
        LOG_ERROR("JSExecutionEngineImpl: Failed to create JavaScript context for session: {}", sessionId);
    }
}

void JSExecutionEngineImpl::onSessionDestroyed(const std::string &sessionId) {
    LOG_DEBUG("JSExecutionEngineImpl: Observer notification - session destroyed: {}", sessionId);

    if (!destroySessionContextInternal(sessionId)) {
        LOG_ERROR("JSExecutionEngineImpl: Failed to cleanup JavaScript context for session: {}", sessionId);
    }
}

void JSExecutionEngineImpl::onSessionSystemVariablesUpdated(const std::string &sessionId,
                                                            const std::string &sessionName,
                                                            const std::vector<std::string> &ioProcessors) {
    LOG_DEBUG("JSExecutionEngineImpl: Observer notification - system variables updated for session: {}", sessionId);

    // Update internal session info and setup system variables
    auto future = setupSystemVariables(sessionId, sessionName, ioProcessors);
    try {
        auto result = future.get();
        if (!result.isSuccess()) {
            LOG_ERROR("JSExecutionEngineImpl: Failed to update system variables for session: {}", sessionId);
        }
    } catch (const std::exception &e) {
        LOG_ERROR("JSExecutionEngineImpl: Exception updating system variables for session {}: {}", sessionId, e.what());
    }
}

// === StateMachine Integration ===

void JSExecutionEngineImpl::setStateMachine(StateMachine *stateMachine, const std::string &sessionId) {
    if (!stateMachine || sessionId.empty()) {
        LOG_ERROR("JSExecutionEngineImpl: Invalid parameters for StateMachine registration");
        return;
    }

    std::lock_guard<std::mutex> lock(stateMachinesMutex_);
    stateMachines_[sessionId] = stateMachine;
    LOG_DEBUG("JSExecutionEngineImpl: Registered StateMachine for session: {}", sessionId);
}

void JSExecutionEngineImpl::removeStateMachine(const std::string &sessionId) {
    std::lock_guard<std::mutex> lock(stateMachinesMutex_);
    auto removed = stateMachines_.erase(sessionId);
    if (removed > 0) {
        LOG_DEBUG("JSExecutionEngineImpl: Removed StateMachine for session: {}", sessionId);
    }
}

// === Private Implementation ===

void JSExecutionEngineImpl::executionWorker() {
    LOG_DEBUG("JSExecutionEngineImpl: Worker LOOP START - Thread ID: {}",
              std::hash<std::thread::id>{}(std::this_thread::get_id()));

    if (!runtime_) {
        LOG_ERROR("JSExecutionEngineImpl: Worker thread started without QuickJS runtime");
        return;
    }

    LOG_DEBUG("JSExecutionEngineImpl: QuickJS runtime ready in worker thread");
    LOG_DEBUG("JSExecutionEngineImpl: Worker thread initialization complete");

    while (!shouldStop_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        LOG_DEBUG("JSExecutionEngineImpl: Worker loop iteration - shouldStop: {}, queue size: {}", shouldStop_.load(),
                  requestQueue_.size());

        queueCondition_.wait(lock, [this] { return !requestQueue_.empty() || shouldStop_; });

        if (shouldStop_) {
            LOG_DEBUG("JSExecutionEngineImpl: Worker woke up - shouldStop: true, queue size: {}", requestQueue_.size());
            break;
        }

        if (!requestQueue_.empty()) {
            LOG_DEBUG("JSExecutionEngineImpl: Worker woke up - shouldStop: false, queue size: {}",
                      requestQueue_.size());
            auto request = std::move(requestQueue_.front());
            requestQueue_.pop();
            lock.unlock();

            LOG_DEBUG("JSExecutionEngineImpl: Processing request type: {}", static_cast<int>(request->type));
            try {
                processExecutionRequest(std::move(request));
                LOG_DEBUG("JSExecutionEngineImpl: Request processed successfully");
            } catch (const std::exception &e) {
                LOG_ERROR("JSExecutionEngineImpl: Exception processing request: {}", e.what());
            }
        }
    }

    LOG_DEBUG("JSExecutionEngineImpl: Worker LOOP END - shouldStop: {}", shouldStop_.load());
}

size_t JSExecutionEngineImpl::getMemoryUsageInternal() const {
    if (!runtime_) {
        return 0;
    }

    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime_, &usage);
    return usage.memory_used_size;
}

void JSExecutionEngineImpl::collectGarbageInternal() {
    if (runtime_) {
        JS_RunGC(runtime_);
        LOG_DEBUG("JSExecutionEngineImpl: Garbage collection completed");
    }
}

}  // namespace RSM