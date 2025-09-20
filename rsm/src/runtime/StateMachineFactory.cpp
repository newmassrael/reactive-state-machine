#include "runtime/StateMachineFactory.h"
#include "common/Logger.h"
#include "scripting/IScriptEngine.h"
#include "scripting/JSEngine.h"
#include <random>

namespace RSM {

// === StateMachineFactory Implementation ===

StateMachineFactory::CreationResult StateMachineFactory::createProduction() {
    auto scriptEngine = std::make_shared<JSEngineAdapter>();
    return createInternal(scriptEngine, "", true);
}

StateMachineFactory::CreationResult StateMachineFactory::createForTesting() {
    auto mockEngine = std::make_shared<MockScriptEngine>();
    return createInternal(mockEngine, "", true);
}

StateMachineFactory::CreationResult
StateMachineFactory::createWithScriptEngine(std::shared_ptr<ISessionBasedScriptEngine> scriptEngine) {
    if (!scriptEngine) {
        return CreationResult("Script engine cannot be null");
    }
    return createInternal(scriptEngine, "", true);
}

StateMachineFactory::CreationResult StateMachineFactory::createWithSCXML(const std::string &scxmlContent,
                                                                         bool useProductionEngine) {
    if (scxmlContent.empty()) {
        return CreationResult("SCXML content cannot be empty");
    }

    std::shared_ptr<ISessionBasedScriptEngine> scriptEngine;
    if (useProductionEngine) {
        scriptEngine = std::make_shared<JSEngineAdapter>();
    } else {
        scriptEngine = std::make_shared<MockScriptEngine>();
    }

    return createInternal(scriptEngine, scxmlContent, true);
}

StateMachineFactory::CreationResult
StateMachineFactory::createInternal(std::shared_ptr<ISessionBasedScriptEngine> scriptEngine,
                                    const std::string &scxmlContent, bool autoInitialize) {
    if (!scriptEngine) {
        return CreationResult("Script engine is required");
    }

    try {
        // Create StateMachine with dependency injection
        auto stateMachine = std::make_unique<StateMachine>();

        // Load SCXML if provided
        if (!scxmlContent.empty()) {
            if (!stateMachine->loadSCXMLFromString(scxmlContent)) {
                return CreationResult("Failed to load SCXML content");
            }
        }

        // Initialize if requested
        if (autoInitialize) {
            if (!stateMachine->start()) {
                return CreationResult("Failed to start StateMachine");
            }
        }

        Logger::debug("StateMachineFactory: Successfully created StateMachine instance");
        return CreationResult(std::move(stateMachine));

    } catch (const std::exception &e) {
        return CreationResult("StateMachine creation failed: " + std::string(e.what()));
    }
}

// === Builder Implementation ===

StateMachineFactory::CreationResult StateMachineFactory::Builder::build() {
    // Use mock engine if no engine specified
    if (!scriptEngine_) {
        scriptEngine_ = std::make_shared<MockScriptEngine>();
    }

    return StateMachineFactory::createInternal(scriptEngine_, scxmlContent_, autoInitialize_);
}

// === JSEngineAdapter Implementation ===

JSEngineAdapter::JSEngineAdapter() {
    // Generate default session ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    defaultSessionId_ = "adapter_" + std::to_string(dis(gen));
}

JSEngineAdapter::~JSEngineAdapter() {
    if (initialized_) {
        shutdown();
    }
}

bool JSEngineAdapter::initialize() {
    if (initialized_) {
        return true;
    }

    // JSEngine은 생성자에서 자동 초기화됨 (RAII)
    JSEngine::instance();  // RAII 보장
    Logger::debug("JSEngineAdapter: JSEngine automatically initialized via RAII");

    // Create default session
    if (!JSEngine::instance().createSession(defaultSessionId_)) {
        Logger::error("JSEngineAdapter: Failed to create default session");
        return false;
    }

    initialized_ = true;
    Logger::debug("JSEngineAdapter: Successfully initialized");
    return true;
}

void JSEngineAdapter::shutdown() {
    if (!initialized_) {
        return;
    }

    // Destroy default session
    JSEngine::instance().destroySession(defaultSessionId_);

    // Note: We don't shutdown the JSEngine instance as it's a singleton
    // and might be used by other components

    initialized_ = false;
    Logger::debug("JSEngineAdapter: Shutdown completed");
}

std::future<JSResult> JSEngineAdapter::executeScript(const std::string &script) {
    return executeScript(defaultSessionId_, script);
}

std::future<JSResult> JSEngineAdapter::evaluateExpression(const std::string &expression) {
    return evaluateExpression(defaultSessionId_, expression);
}

std::future<JSResult> JSEngineAdapter::setVariable(const std::string &name, const ScriptValue &value) {
    return setVariable(defaultSessionId_, name, value);
}

std::future<JSResult> JSEngineAdapter::getVariable(const std::string &name) {
    return getVariable(defaultSessionId_, name);
}

std::string JSEngineAdapter::getEngineInfo() const {
    return JSEngine::instance().getEngineInfo() + " (via Adapter)";
}

size_t JSEngineAdapter::getMemoryUsage() const {
    return JSEngine::instance().getMemoryUsage();
}

void JSEngineAdapter::collectGarbage() {
    JSEngine::instance().collectGarbage();
}

bool JSEngineAdapter::createSession(const std::string &sessionId, const std::string &parentSessionId) {
    if (!initialized_) {
        Logger::error("JSEngineAdapter: Not initialized");
        return false;
    }
    return JSEngine::instance().createSession(sessionId, parentSessionId);
}

bool JSEngineAdapter::destroySession(const std::string &sessionId) {
    if (!initialized_) {
        return false;
    }
    return JSEngine::instance().destroySession(sessionId);
}

bool JSEngineAdapter::hasSession(const std::string &sessionId) {
    if (!initialized_) {
        return false;
    }
    return JSEngine::instance().hasSession(sessionId);
}

std::vector<std::string> JSEngineAdapter::getActiveSessions() const {
    if (!initialized_) {
        return {};
    }
    return JSEngine::instance().getActiveSessions();
}

std::future<JSResult> JSEngineAdapter::executeScript(const std::string &sessionId, const std::string &script) {
    if (!initialized_) {
        std::promise<JSResult> promise;
        promise.set_value(JSResult::createError("Adapter not initialized"));
        return promise.get_future();
    }
    return JSEngine::instance().executeScript(sessionId, script);
}

std::future<JSResult> JSEngineAdapter::evaluateExpression(const std::string &sessionId, const std::string &expression) {
    if (!initialized_) {
        std::promise<JSResult> promise;
        promise.set_value(JSResult::createError("Adapter not initialized"));
        return promise.get_future();
    }
    return JSEngine::instance().evaluateExpression(sessionId, expression);
}

std::future<JSResult> JSEngineAdapter::setVariable(const std::string &sessionId, const std::string &name,
                                                   const ScriptValue &value) {
    if (!initialized_) {
        std::promise<JSResult> promise;
        promise.set_value(JSResult::createError("Adapter not initialized"));
        return promise.get_future();
    }
    return JSEngine::instance().setVariable(sessionId, name, value);
}

std::future<JSResult> JSEngineAdapter::getVariable(const std::string &sessionId, const std::string &name) {
    if (!initialized_) {
        std::promise<JSResult> promise;
        promise.set_value(JSResult::createError("Adapter not initialized"));
        return promise.get_future();
    }
    return JSEngine::instance().getVariable(sessionId, name);
}

}  // namespace RSM