#define SCXML_ENGINE_EXPORTS
#include "SCXMLEngineImpl.h"
#include "common/Logger.h"
#include "common/UniqueIdGenerator.h"
#include "runtime/ExecutionContextImpl.h"
#include "runtime/StateMachineFactory.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace RSM {

// === ExecutionResult Implementation ===

::std::string ExecutionResult::getValueAsString() const {
    return ::std::visit(
        [](const auto &v) -> ::std::string {
            using T = ::std::decay_t<decltype(v)>;
            if constexpr (::std::is_same_v<T, ::std::string>) {
                return v;
            } else if constexpr (::std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (::std::is_same_v<T, int64_t>) {
                return ::std::to_string(v);
            } else if constexpr (::std::is_same_v<T, double>) {
                return ::std::to_string(v);
            } else {
                return "undefined";
            }
        },
        value);
}

// === Event Implementation ===

Event::Event(const ::std::string &name, const ::std::string &type) : name_(name), type_(type) {}

// === SCXMLEngineImpl Implementation ===

SCXMLEngineImpl::SCXMLEngineImpl() = default;

SCXMLEngineImpl::~SCXMLEngineImpl() {
    if (initialized_) {
        shutdown();
    }
}

bool SCXMLEngineImpl::initialize() {
    LOG_DEBUG("SCXMLEngineImpl: Starting initialization...");
    if (initialized_) {
        LOG_DEBUG("SCXMLEngineImpl: Already initialized");
        return true;
    }

    // JSEngine은 생성자에서 자동 초기화됨 (RAII)
    // instance() 호출만으로 완전 초기화된 엔진을 얻을 수 있음
    RSM::JSEngine::instance();  // RAII 보장
    LOG_DEBUG("SCXMLEngineImpl: JSEngine automatically initialized via RAII");
    initialized_ = true;
    return true;
}

void SCXMLEngineImpl::shutdown() {
    if (initialized_) {
        RSM::JSEngine::instance().shutdown();
        initialized_ = false;
    }
}

::std::string SCXMLEngineImpl::getEngineInfo() const {
    return RSM::JSEngine::instance().getEngineInfo() + " (SCXML C++ API v1.0)";
}

bool SCXMLEngineImpl::createSession(const ::std::string &sessionId, const ::std::string &parentSessionId) {
    return RSM::JSEngine::instance().createSession(sessionId, parentSessionId);
}

bool SCXMLEngineImpl::destroySession(const ::std::string &sessionId) {
    return RSM::JSEngine::instance().destroySession(sessionId);
}

bool SCXMLEngineImpl::hasSession(const ::std::string &sessionId) const {
    return RSM::JSEngine::instance().hasSession(sessionId);
}

::std::vector<SessionInfo> SCXMLEngineImpl::getActiveSessions() const {
    auto sessionIds = RSM::JSEngine::instance().getActiveSessions();
    ::std::vector<SessionInfo> result;
    result.reserve(sessionIds.size());

    for (const auto &id : sessionIds) {
        SessionInfo info;
        info.sessionId = id;
        info.isActive = true;
        result.push_back(::std::move(info));
    }

    return result;
}

::std::future<ExecutionResult> SCXMLEngineImpl::executeScript(const ::std::string &sessionId,
                                                              const ::std::string &script) {
    auto jsFuture = RSM::JSEngine::instance().executeScript(sessionId, script);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}

::std::future<ExecutionResult> SCXMLEngineImpl::evaluateExpression(const ::std::string &sessionId,
                                                                   const ::std::string &expression) {
    auto jsFuture = RSM::JSEngine::instance().evaluateExpression(sessionId, expression);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}

::std::future<ExecutionResult> SCXMLEngineImpl::setVariable(const ::std::string &sessionId, const ::std::string &name,
                                                            const ScriptValue &value) {
    // Convert public ScriptValue to internal ScriptValue (same type)
    auto jsFuture = RSM::JSEngine::instance().setVariable(sessionId, name, value);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}

::std::future<ExecutionResult> SCXMLEngineImpl::getVariable(const ::std::string &sessionId, const ::std::string &name) {
    auto jsFuture = RSM::JSEngine::instance().getVariable(sessionId, name);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}

::std::future<ExecutionResult> SCXMLEngineImpl::setCurrentEvent(const ::std::string &sessionId,
                                                                ::std::shared_ptr<Event> event) {
    auto internalEvent = event;
    auto jsFuture = RSM::JSEngine::instance().setCurrentEvent(sessionId, internalEvent);
    return ::std::async(::std::launch::deferred, [this, jsFuture = ::std::move(jsFuture)]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}

::std::future<ExecutionResult> SCXMLEngineImpl::setupSystemVariables(const ::std::string &sessionId,
                                                                     const ::std::string &sessionName,
                                                                     const ::std::vector<::std::string> &ioProcessors) {
    auto jsFuture = RSM::JSEngine::instance().setupSystemVariables(sessionId, sessionName, ioProcessors);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}

size_t SCXMLEngineImpl::getMemoryUsage() const {
    return RSM::JSEngine::instance().getMemoryUsage();
}

void SCXMLEngineImpl::collectGarbage() {
    RSM::JSEngine::instance().collectGarbage();
}

ExecutionResult SCXMLEngineImpl::convertResult(const JSResult &jsResult) const {
    ExecutionResult result;
    result.success = jsResult.isSuccess();
    // Direct access to value through internal member (friend class access)
    result.value = jsResult.getInternalValue();
    result.errorMessage = jsResult.isSuccess() ? "" : "Execution failed";
    return result;
}

::std::shared_ptr<Event> SCXMLEngineImpl::convertEvent(::std::shared_ptr<Event> publicEvent) const {
    if (!publicEvent) {
        return nullptr;
    }

    auto internalEvent = ::std::make_shared<Event>(publicEvent->getName(), publicEvent->getType());
    internalEvent->setSendId(publicEvent->getSendId());
    internalEvent->setOrigin(publicEvent->getOrigin());
    internalEvent->setOriginType(publicEvent->getOriginType());
    internalEvent->setInvokeId(publicEvent->getInvokeId());
    if (publicEvent->hasData()) {
        internalEvent->setRawJsonData(publicEvent->getDataAsString());
    }

    return internalEvent;
}

// === Factory Functions ===

::std::unique_ptr<SCXMLEngine> createSCXMLEngine() {
    return ::std::make_unique<SCXMLEngineImpl>();
}

::std::string getSCXMLVersion() {
    return "1.0.0";
}

void SCXMLEngineImpl::executeOnEntryActions(const std::string &stateId) {
    if (!scxmlModel_) {
        LOG_ERROR("Cannot execute onentry actions: SCXML model is null");
        return;
    }

    // Find the state node
    auto stateNode = scxmlModel_->findStateById(stateId);
    if (!stateNode) {
        LOG_ERROR("Cannot find state node for onentry execution: {}", stateId);
        return;
    }

    // Get entry actions from the state
    const auto &entryActions = stateNode->getEntryActionNodes();
    if (entryActions.empty()) {
        LOG_DEBUG("No onentry actions to execute for state: {}", stateId);
        return;
    }

    LOG_DEBUG("Executing {} onentry actions for state: {}", entryActions.size(), stateId);

    // W3C SCXML: Execute onentry handlers in document order
    for (const auto &action : entryActions) {
        if (!action) {
            LOG_WARN("Null onentry action found in state: {}", stateId);
            continue;
        }

        LOG_DEBUG("Executing onentry action: {} in state: {}", action->getActionType(), stateId);

        // Create execution context for the action
        if (actionExecutor_) {
            auto sharedActionExecutor =
                std::shared_ptr<IActionExecutor>(actionExecutor_.get(), [](IActionExecutor *) {});
            ExecutionContextImpl context(sharedActionExecutor, sessionId_);

            // Execute the action
            if (!action->execute(context)) {
                LOG_ERROR("Failed to execute onentry action: {} in state: {}", action->getActionType(), stateId);
                // W3C SCXML: Continue with other actions even if one fails
            }
        } else {
            LOG_ERROR("Cannot execute onentry action: ActionExecutor is null");
        }
    }
}

// === High-Level SCXML State Machine API Implementation ===

std::string SCXMLEngineImpl::generateSessionId() const {
    // REFACTOR: Use centralized UniqueIdGenerator instead of duplicate logic
    return UniqueIdGenerator::generateSessionId("scxml");
}

bool SCXMLEngineImpl::loadSCXMLFromString(const std::string &scxmlContent, const std::string &sessionId) {
    try {
        // Determine session ID
        std::string actualSessionId = sessionId.empty() ? generateSessionId() : sessionId;

        // Store as default session if none specified
        if (sessionId.empty()) {
            defaultSessionId_ = actualSessionId;
        }

        // Create StateMachine with automatic dependency injection
        auto result = StateMachineFactory::createWithSCXML(scxmlContent);
        if (!result.has_value()) {
            sessionErrors_[actualSessionId] = "Failed to create state machine: " + result.error;
            LOG_ERROR("SCXMLEngine: Failed to load SCXML content: {}", result.error);
            return false;
        }

        stateMachine_ = std::move(result.value);
        if (!stateMachine_) {
            sessionErrors_[actualSessionId] = "State machine creation returned null";
            LOG_ERROR("SCXMLEngine: State machine creation returned null");
            return false;
        }

        LOG_INFO("SCXMLEngine: SCXML content loaded successfully with session: {}", actualSessionId);
        return true;

    } catch (const std::exception &e) {
        std::string actualSessionId = sessionId.empty() ? defaultSessionId_ : sessionId;
        sessionErrors_[actualSessionId] = std::string("Load failed: ") + e.what();
        LOG_ERROR("SCXMLEngine: Exception during SCXML load: {}", e.what());
        return false;
    }
}

bool SCXMLEngineImpl::loadSCXMLFromFile(const std::string &scxmlFile, const std::string &sessionId) {
    try {
        // Read file content
        std::ifstream file(scxmlFile);
        if (!file.is_open()) {
            std::string actualSessionId = sessionId.empty() ? defaultSessionId_ : sessionId;
            sessionErrors_[actualSessionId] = "Cannot open SCXML file: " + scxmlFile;
            LOG_ERROR("SCXMLEngine: Cannot open SCXML file: {}", scxmlFile);
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return loadSCXMLFromString(content, sessionId);

    } catch (const std::exception &e) {
        std::string actualSessionId = sessionId.empty() ? defaultSessionId_ : sessionId;
        sessionErrors_[actualSessionId] = std::string("File load failed: ") + e.what();
        LOG_ERROR("SCXMLEngine: Exception during file load: {}", e.what());
        return false;
    }
}

bool SCXMLEngineImpl::startStateMachine(const std::string &sessionId) {
    std::string actualSessionId = sessionId.empty() ? defaultSessionId_ : sessionId;

    if (!stateMachine_) {
        sessionErrors_[actualSessionId] = "No state machine loaded";
        LOG_ERROR("SCXMLEngine: Cannot start - no state machine loaded");
        return false;
    }

    try {
        bool result = stateMachine_->start();
        if (!result) {
            sessionErrors_[actualSessionId] = "Failed to start state machine";
            LOG_ERROR("SCXMLEngine: Failed to start state machine");
            return false;
        }

        LOG_INFO("SCXMLEngine: State machine started successfully for session: {}", actualSessionId);
        return true;

    } catch (const std::exception &e) {
        sessionErrors_[actualSessionId] = std::string("Start failed: ") + e.what();
        LOG_ERROR("SCXMLEngine: Exception during start: {}", e.what());
        return false;
    }
}

void SCXMLEngineImpl::stopStateMachine(const std::string &sessionId) {
    if (stateMachine_) {
        try {
            stateMachine_->stop();
            std::string actualSessionId = sessionId.empty() ? defaultSessionId_ : sessionId;
            LOG_INFO("SCXMLEngine: State machine stopped for session: {}", actualSessionId);
        } catch (const std::exception &e) {
            LOG_WARN("SCXMLEngine: Exception during stop: {}", e.what());
        }
    }
}

bool SCXMLEngineImpl::sendEventSync(const std::string &eventName, const std::string &sessionId,
                                    const std::string &eventData) {
    std::string actualSessionId = sessionId.empty() ? defaultSessionId_ : sessionId;

    if (!stateMachine_) {
        sessionErrors_[actualSessionId] = "No state machine available";
        return false;
    }

    if (!stateMachine_->isRunning()) {
        sessionErrors_[actualSessionId] = "State machine is not running";
        return false;
    }

    try {
        auto result = stateMachine_->processEvent(eventName, eventData);
        if (!result.success) {
            sessionErrors_[actualSessionId] = "Event processing failed: " + result.errorMessage;
            LOG_WARN("SCXMLEngine: Event '{}' failed: {}", eventName, result.errorMessage);
            return false;
        }
        return true;

    } catch (const std::exception &e) {
        sessionErrors_[actualSessionId] = std::string("Event processing exception: ") + e.what();
        LOG_ERROR("SCXMLEngine: Event '{}' exception: {}", eventName, e.what());
        return false;
    }
}

bool SCXMLEngineImpl::isStateMachineRunning([[maybe_unused]] const std::string &sessionId) const {
    return stateMachine_ && stateMachine_->isRunning();
}

std::string SCXMLEngineImpl::getCurrentStateSync([[maybe_unused]] const std::string &sessionId) const {
    if (!stateMachine_) {
        return "";
    }
    return stateMachine_->getCurrentState();
}

bool SCXMLEngineImpl::isInStateSync(const std::string &stateId, [[maybe_unused]] const std::string &sessionId) const {
    if (!stateMachine_) {
        return false;
    }
    return stateMachine_->isStateActive(stateId);
}

std::vector<std::string> SCXMLEngineImpl::getActiveStatesSync([[maybe_unused]] const std::string &sessionId) const {
    if (!stateMachine_) {
        return {};
    }
    return stateMachine_->getActiveStates();
}

bool SCXMLEngineImpl::setVariableSync(const std::string &name, const std::string &value, const std::string &sessionId) {
    std::string actualSessionId = sessionId.empty() ? defaultSessionId_ : sessionId;

    if (!stateMachine_) {
        sessionErrors_[actualSessionId] = "No state machine available";
        return false;
    }

    try {
        // Use StateMachine's session ID
        const std::string &smSessionId = stateMachine_->getSessionId();
        auto &jsEngine = JSEngine::instance();
        auto future = jsEngine.setVariable(smSessionId, name, ScriptValue(value));
        auto result = future.get();

        if (!JSEngine::isSuccess(result)) {
            sessionErrors_[actualSessionId] = "Failed to set variable: " + result.getErrorMessage();
            LOG_WARN("SCXMLEngine: Failed to set variable '{}': {}", name, result.getErrorMessage());
            return false;
        }
        return true;

    } catch (const std::exception &e) {
        sessionErrors_[actualSessionId] = std::string("Variable setting exception: ") + e.what();
        LOG_ERROR("SCXMLEngine: Variable '{}' exception: {}", name, e.what());
        return false;
    }
}

std::string SCXMLEngineImpl::getVariableSync(const std::string &name,
                                             [[maybe_unused]] const std::string &sessionId) const {
    if (!stateMachine_) {
        return "";
    }

    try {
        // Use StateMachine's session ID
        const std::string &smSessionId = stateMachine_->getSessionId();
        auto &jsEngine = JSEngine::instance();
        auto future = jsEngine.getVariable(smSessionId, name);
        auto result = future.get();

        if (!JSEngine::isSuccess(result)) {
            return "";
        }
        return JSEngine::resultToString(result);

    } catch (const std::exception &e) {
        LOG_WARN("SCXMLEngine: Failed to get variable '{}': {}", name, e.what());
        return "";
    }
}

std::string SCXMLEngineImpl::getLastStateMachineError(const std::string &sessionId) const {
    std::string actualSessionId = sessionId.empty() ? defaultSessionId_ : sessionId;
    auto it = sessionErrors_.find(actualSessionId);
    return it != sessionErrors_.end() ? it->second : "";
}

}  // namespace RSM
