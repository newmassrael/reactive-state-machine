#define SCXML_ENGINE_EXPORTS
#include "SCXMLEngineImpl.h"
#include "common/Logger.h"
#include <sstream>
#include <iostream>

namespace RSM {

// === ExecutionResult Implementation ===


::std::string ExecutionResult::getValueAsString() const {
    return ::std::visit([](const auto& v) -> ::std::string {
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
    }, value);
}


// === Event Implementation ===


Event::Event(const ::std::string& name, const ::std::string& type)
    : name_(name), type_(type) {
}


// === SCXMLEngineImpl Implementation ===


SCXMLEngineImpl::SCXMLEngineImpl() = default;


SCXMLEngineImpl::~SCXMLEngineImpl() {
    if (initialized_) {
        shutdown();
    }
}


bool SCXMLEngineImpl::initialize() {
    Logger::debug("SCXMLEngineImpl: Starting initialization...");
    if (initialized_) {
        Logger::debug("SCXMLEngineImpl: Already initialized");
        return true;
    }


    bool success = JSEngine::instance().initialize();
    Logger::debug("SCXMLEngineImpl: JSEngine initialization result: " + std::string(success ? "SUCCESS" : "FAILED"));
    if (success) {
        initialized_ = true;
    }
    return success;
}


void SCXMLEngineImpl::shutdown() {
    if (initialized_) {
        JSEngine::instance().shutdown();
        initialized_ = false;
    }
}


::std::string SCXMLEngineImpl::getEngineInfo() const {
    return JSEngine::instance().getEngineInfo() + " (SCXML C++ API v1.0)";
}


bool SCXMLEngineImpl::createSession(const ::std::string& sessionId,
                                   const ::std::string& parentSessionId) {
    return JSEngine::instance().createSession(sessionId, parentSessionId);
}


bool SCXMLEngineImpl::destroySession(const ::std::string& sessionId) {
    return JSEngine::instance().destroySession(sessionId);
}


bool SCXMLEngineImpl::hasSession(const ::std::string& sessionId) const {
    return JSEngine::instance().hasSession(sessionId);
}


::std::vector<SessionInfo> SCXMLEngineImpl::getActiveSessions() const {
    auto sessionIds = JSEngine::instance().getActiveSessions();
    ::std::vector<SessionInfo> result;
    result.reserve(sessionIds.size());


    for (const auto& id : sessionIds) {
        SessionInfo info;
        info.sessionId = id;
        info.isActive = true;
        result.push_back(::std::move(info));
    }


    return result;
}


::std::future<ExecutionResult> SCXMLEngineImpl::executeScript(const ::std::string& sessionId,
                                                           const ::std::string& script) {
    auto jsFuture = JSEngine::instance().executeScript(sessionId, script);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}


::std::future<ExecutionResult> SCXMLEngineImpl::evaluateExpression(const ::std::string& sessionId,
                                                                 const ::std::string& expression) {
    auto jsFuture = JSEngine::instance().evaluateExpression(sessionId, expression);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}


::std::future<ExecutionResult> SCXMLEngineImpl::setVariable(const ::std::string& sessionId,
                                                         const ::std::string& name,
                                                         const ScriptValue& value) {
    // Convert public ScriptValue to internal ScriptValue (same type)
    auto jsFuture = JSEngine::instance().setVariable(sessionId, name, value);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}


::std::future<ExecutionResult> SCXMLEngineImpl::getVariable(const ::std::string& sessionId,
                                                         const ::std::string& name) {
    auto jsFuture = JSEngine::instance().getVariable(sessionId, name);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}


::std::future<ExecutionResult> SCXMLEngineImpl::setCurrentEvent(const ::std::string& sessionId,
                                                             ::std::shared_ptr<Event> event) {
    auto internalEvent = event;
    auto jsFuture = JSEngine::instance().setCurrentEvent(sessionId, internalEvent);
    return ::std::async(::std::launch::deferred, [this, jsFuture = ::std::move(jsFuture)]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}


::std::future<ExecutionResult> SCXMLEngineImpl::setupSystemVariables(const ::std::string& sessionId,
                                                                   const ::std::string& sessionName,
                                                                   const ::std::vector<::std::string>& ioProcessors) {
    auto jsFuture = JSEngine::instance().setupSystemVariables(sessionId, sessionName, ioProcessors);
    return ::std::async(::std::launch::deferred, [jsFuture = ::std::move(jsFuture), this]() mutable {
        auto jsResult = jsFuture.get();
        return convertResult(jsResult);
    });
}


size_t SCXMLEngineImpl::getMemoryUsage() const {
    return JSEngine::instance().getMemoryUsage();
}


void SCXMLEngineImpl::collectGarbage() {
    JSEngine::instance().collectGarbage();
}


ExecutionResult SCXMLEngineImpl::convertResult(const JSResult& jsResult) const {
    ExecutionResult result;
    result.success = jsResult.success;
    result.value = jsResult.value;  // Same type, direct copy
    result.errorMessage = jsResult.errorMessage;
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



}  // namespace RSM
