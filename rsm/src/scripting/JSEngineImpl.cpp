#include "common/Logger.h"
#include "quickjs.h"
#include "scripting/JSEngine.h"
#include <iostream>

namespace RSM {

// === Internal JavaScript Execution Methods ===

JSResult JSEngine::executeScriptInternal(const std::string &sessionId, const std::string &script) {
    Logger::debug("JSEngine: executeScriptInternal - sessionId: " + sessionId + ", script: " + script);
    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        Logger::debug("JSEngine: Session not found or no context: " + sessionId);
        return JSResult::createError("Session not found: " + sessionId);
    }

    JSContext *ctx = session->jsContext;
    Logger::debug("JSEngine: Evaluating script with QuickJS...");
    ::JSValue result = JS_Eval(ctx, script.c_str(), script.length(), "<script>", JS_EVAL_TYPE_GLOBAL);

    Logger::debug("JSEngine: JS_Eval completed, checking result...");
    Logger::debug("JSEngine: JS_IsException(result): " + std::string(JS_IsException(result) ? "true" : "false"));
    Logger::debug("JSEngine: JS_IsUndefined(result): " + std::string(JS_IsUndefined(result) ? "true" : "false"));
    Logger::debug("JSEngine: JS_IsNull(result): " + std::string(JS_IsNull(result) ? "true" : "false"));
    Logger::debug("JSEngine: JS_IsNumber(result): " + std::string(JS_IsNumber(result) ? "true" : "false"));

    if (JS_IsException(result)) {
        Logger::debug("JSEngine: QuickJS exception occurred");
        JSResult error = createErrorFromException(ctx);
        JS_FreeValue(ctx, result);
        Logger::debug("JSEngine: Error: " + error.errorMessage);
        return error;
    }

    Logger::debug("JSEngine: Script executed successfully, converting result...");
    ScriptValue jsResult = quickJSToJSValue(ctx, result);
    JS_FreeValue(ctx, result);
    Logger::debug("JSEngine: Result converted, returning success");
    return JSResult::createSuccess(jsResult);
}

JSResult JSEngine::evaluateExpressionInternal(const std::string &sessionId, const std::string &expression) {
    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }

    JSContext *ctx = session->jsContext;
    
    // First try to evaluate as-is
    ::JSValue result = JS_Eval(ctx, expression.c_str(), expression.length(), "<expression>", JS_EVAL_TYPE_GLOBAL);

    // If it failed and the expression starts with '{', try wrapping in parentheses for object literals
    if (JS_IsException(result) && !expression.empty() && expression[0] == '{') {
        JS_FreeValue(ctx, result);  // Free the exception
        
        std::string wrappedExpression = "(" + expression + ")";
        result = JS_Eval(ctx, wrappedExpression.c_str(), wrappedExpression.length(), "<expression>", JS_EVAL_TYPE_GLOBAL);
    }

    if (JS_IsException(result)) {
        JSResult error = createErrorFromException(ctx);
        JS_FreeValue(ctx, result);
        return error;
    }

    ScriptValue jsResult = quickJSToJSValue(ctx, result);
    JS_FreeValue(ctx, result);
    return JSResult::createSuccess(jsResult);
}

JSResult JSEngine::validateExpressionInternal(const std::string &sessionId, const std::string &expression) {
    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }

    JSContext *ctx = session->jsContext;

    // Try compiling as JavaScript expression to check for syntax errors
    ::JSValue result = JS_Eval(ctx, ("(function(){return (" + expression + ");})").c_str(), expression.length() + 23,
                               "<validation>", JS_EVAL_FLAG_COMPILE_ONLY);

    if (JS_IsException(result)) {
        JSResult error = createErrorFromException(ctx);
        JS_FreeValue(ctx, result);
        return error;
    }

    JS_FreeValue(ctx, result);
    return JSResult::createSuccess();
}

JSResult JSEngine::setVariableInternal(const std::string &sessionId, const std::string &name,
                                       const ScriptValue &value) {
    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }

    JSContext *ctx = session->jsContext;
    ::JSValue global = JS_GetGlobalObject(ctx);
    ::JSValue qjsValue = jsValueToQuickJS(ctx, value);

    JS_SetPropertyStr(ctx, global, name.c_str(), qjsValue);
    JS_FreeValue(ctx, global);

    return JSResult::createSuccess();
}

JSResult JSEngine::getVariableInternal(const std::string &sessionId, const std::string &name) {
    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }

    JSContext *ctx = session->jsContext;
    ::JSValue global = JS_GetGlobalObject(ctx);
    ::JSValue qjsValue = JS_GetPropertyStr(ctx, global, name.c_str());

    if (JS_IsException(qjsValue)) {
        JS_FreeValue(ctx, global);
        return createErrorFromException(ctx);
    }

    // Check if the property actually exists (not just undefined)
    if (JS_IsUndefined(qjsValue)) {
        // Use JS_HasProperty to distinguish between "not set" and "set to
        // undefined"
        JSAtom atom = JS_NewAtom(ctx, name.c_str());
        int hasProperty = JS_HasProperty(ctx, global, atom);
        JS_FreeAtom(ctx, atom);  // Free the atom to prevent memory leak
        if (hasProperty <= 0) {
            // Property doesn't exist - this is an error
            JS_FreeValue(ctx, qjsValue);
            JS_FreeValue(ctx, global);
            return JSResult::createError("Variable not found: " + name);
        }
        // Property exists but is undefined - this is valid, continue with existing
        // qjsValue
    }

    ScriptValue result = quickJSToJSValue(ctx, qjsValue);
    JS_FreeValue(ctx, qjsValue);
    JS_FreeValue(ctx, global);

    return JSResult::createSuccess(result);
}

JSResult JSEngine::setCurrentEventInternal(const std::string &sessionId, const std::shared_ptr<Event> &event) {
    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }

    JSContext *ctx = session->jsContext;
    ::JSValue global = JS_GetGlobalObject(ctx);
    ::JSValue eventObj = JS_NewObject(ctx);

    if (event) {
        // Set event properties
        JS_SetPropertyStr(ctx, eventObj, "name", JS_NewString(ctx, event->getName().c_str()));
        JS_SetPropertyStr(ctx, eventObj, "type", JS_NewString(ctx, event->getType().c_str()));
        JS_SetPropertyStr(ctx, eventObj, "sendid", JS_NewString(ctx, event->getSendId().c_str()));
        JS_SetPropertyStr(ctx, eventObj, "origin", JS_NewString(ctx, event->getOrigin().c_str()));
        JS_SetPropertyStr(ctx, eventObj, "origintype", JS_NewString(ctx, event->getOriginType().c_str()));
        JS_SetPropertyStr(ctx, eventObj, "invokeid", JS_NewString(ctx, event->getInvokeId().c_str()));

        // Set event data
        if (event->hasData()) {
            std::string dataStr = event->getDataAsString();
            ::JSValue dataValue = JS_ParseJSON(ctx, dataStr.c_str(), dataStr.length(), "<event-data>");
            if (!JS_IsException(dataValue)) {
                JS_SetPropertyStr(ctx, eventObj, "data", dataValue);
            } else {
                JS_SetPropertyStr(ctx, eventObj, "data", JS_UNDEFINED);
            }
        } else {
            JS_SetPropertyStr(ctx, eventObj, "data", JS_UNDEFINED);
        }

        // Store event in session
        session->currentEvent = event;
    } else {
        // Clear event
        JS_SetPropertyStr(ctx, eventObj, "name", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, eventObj, "type", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, eventObj, "sendid", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, eventObj, "origin", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, eventObj, "origintype", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, eventObj, "invokeid", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, eventObj, "data", JS_UNDEFINED);

        session->currentEvent.reset();
    }

    JS_SetPropertyStr(ctx, global, "_event", eventObj);
    JS_FreeValue(ctx, global);

    return JSResult::createSuccess();
}

JSResult JSEngine::setupSystemVariablesInternal(const std::string &sessionId, const std::string &sessionName,
                                                const std::vector<std::string> &ioProcessors) {
    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }

    JSContext *ctx = session->jsContext;
    ::JSValue global = JS_GetGlobalObject(ctx);

    // Set _sessionid
    JS_SetPropertyStr(ctx, global, "_sessionid", JS_NewString(ctx, sessionId.c_str()));

    // Set _name
    JS_SetPropertyStr(ctx, global, "_name", JS_NewString(ctx, sessionName.c_str()));

    // Set _ioprocessors
    ::JSValue ioProcessorsArray = JS_NewArray(ctx);
    for (size_t i = 0; i < ioProcessors.size(); ++i) {
        JS_SetPropertyUint32(ctx, ioProcessorsArray, static_cast<uint32_t>(i),
                             JS_NewString(ctx, ioProcessors[i].c_str()));
    }
    JS_SetPropertyStr(ctx, global, "_ioprocessors", ioProcessorsArray);

    JS_FreeValue(ctx, global);

    // Store in session
    session->sessionName = sessionName;
    session->ioProcessors = ioProcessors;

    return JSResult::createSuccess();
}

// === Type Conversion ===

ScriptValue JSEngine::quickJSToJSValue(JSContext *ctx, JSValue qjsValue) {
    if (JS_IsBool(qjsValue)) {
        return JS_ToBool(ctx, qjsValue) ? true : false;
    } else if (JS_IsNumber(qjsValue)) {
        // JavaScript numbers are always double (IEEE 754)
        double d;
        JS_ToFloat64(ctx, &d, qjsValue);
        return d;
    } else if (JS_IsString(qjsValue)) {
        const char *str = JS_ToCString(ctx, qjsValue);
        std::string result(str ? str : "");
        if (str) {
            JS_FreeCString(ctx, str);
        }
        return result;
    } else if (JS_IsArray(qjsValue)) {
        auto scriptArray = std::make_shared<ScriptArray>();
        JSValue lengthVal = JS_GetPropertyStr(ctx, qjsValue, "length");
        int64_t length = 0;
        JS_ToInt64(ctx, &length, lengthVal);
        JS_FreeValue(ctx, lengthVal);
        
        scriptArray->elements.reserve(static_cast<size_t>(length));
        for (int64_t i = 0; i < length; ++i) {
            JSValue element = JS_GetPropertyUint32(ctx, qjsValue, static_cast<uint32_t>(i));
            scriptArray->elements.push_back(quickJSToJSValue(ctx, element));
            JS_FreeValue(ctx, element);
        }
        return scriptArray;
    } else if (JS_IsObject(qjsValue) && !JS_IsFunction(ctx, qjsValue)) {
        auto scriptObject = std::make_shared<ScriptObject>();
        JSPropertyEnum *props = nullptr;
        uint32_t propCount = 0;
        
        if (JS_GetOwnPropertyNames(ctx, &props, &propCount, qjsValue, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
            for (uint32_t i = 0; i < propCount; ++i) {
                const char *key = JS_AtomToCString(ctx, props[i].atom);
                if (key) {
                    JSValue propValue = JS_GetProperty(ctx, qjsValue, props[i].atom);
                    scriptObject->properties[key] = quickJSToJSValue(ctx, propValue);
                    JS_FreeValue(ctx, propValue);
                    JS_FreeCString(ctx, key);
                }
                JS_FreeAtom(ctx, props[i].atom);
            }
            js_free(ctx, props);
        }
        return scriptObject;
    }

    return std::monostate{};  // undefined/null
}

JSValue JSEngine::jsValueToQuickJS(JSContext *ctx, const ScriptValue &value) {
    return std::visit(
        [this, ctx](const auto &v) -> JSValue {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return JS_NewBool(ctx, v);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return JS_NewInt64(ctx, v);
            } else if constexpr (std::is_same_v<T, double>) {
                return JS_NewFloat64(ctx, v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return JS_NewString(ctx, v.c_str());
            } else if constexpr (std::is_same_v<T, std::shared_ptr<ScriptArray>>) {
                JSValue jsArray = JS_NewArray(ctx);
                for (size_t i = 0; i < v->elements.size(); ++i) {
                    JSValue element = jsValueToQuickJS(ctx, v->elements[i]);
                    JS_SetPropertyUint32(ctx, jsArray, static_cast<uint32_t>(i), element);
                }
                return jsArray;
            } else if constexpr (std::is_same_v<T, std::shared_ptr<ScriptObject>>) {
                JSValue jsObject = JS_NewObject(ctx);
                for (const auto &[key, val] : v->properties) {
                    JSValue propValue = jsValueToQuickJS(ctx, val);
                    JS_SetPropertyStr(ctx, jsObject, key.c_str(), propValue);
                }
                return jsObject;
            } else {
                return JS_UNDEFINED;
            }
        },
        value);
}

// === Error Handling ===

JSResult JSEngine::createErrorFromException(JSContext *ctx) {
    ::JSValue exception = JS_GetException(ctx);
    Logger::debug("JSEngine: Getting exception details...");
    if (JS_IsNull(exception)) {
        Logger::debug("JSEngine: Exception is null");
        return JSResult::createError("JavaScript error: Exception is null");
    }
    const char *errorStr = JS_ToCString(ctx, exception);
    std::string errorMessage;
    if (errorStr) {
        errorMessage = std::string("JavaScript error: ") + errorStr;
        Logger::debug("JSEngine: Exception message: " + std::string(errorStr));
        JS_FreeCString(ctx, errorStr);
    } else {
        errorMessage = "Unknown JavaScript error - could not get error string";
        Logger::debug("JSEngine: Could not get error string from exception");
    }
    // Try to get stack trace
    ::JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
    if (!JS_IsUndefined(stack)) {
        const char *stackStr = JS_ToCString(ctx, stack);
        if (stackStr) {
            Logger::debug("JSEngine: Stack trace: " + std::string(stackStr));
            errorMessage += "\nStack: " + std::string(stackStr);
            JS_FreeCString(ctx, stackStr);
        }
    }
    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, exception);
    Logger::debug("JSEngine: Final error message: " + errorMessage);
    return JSResult::createError(errorMessage);
}

}  // namespace RSM
