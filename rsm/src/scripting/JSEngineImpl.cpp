#include "common/Logger.h"
#include "quickjs.h"
#include "scripting/JSEngine.h"
#include <climits>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace RSM {

// === Internal JavaScript Execution Methods ===

JSResult JSEngine::executeScriptInternal(const std::string &sessionId, const std::string &script) {
    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }

    JSContext *ctx = session->jsContext;

    // Execute script with QuickJS global evaluation
    LOG_DEBUG("JSEngine: Executing script with QuickJS...");

    ::JSValue result = JS_Eval(ctx, script.c_str(), script.length(), "<script>", JS_EVAL_TYPE_GLOBAL);

    LOG_DEBUG("JSEngine: JS_Eval completed, checking result...");

    if (JS_IsException(result)) {
        LOG_DEBUG("JSEngine: Exception occurred in script execution");
        JSResult error = createErrorFromException(ctx);
        JS_FreeValue(ctx, result);
        return error;
    }

    LOG_DEBUG("JSEngine: Script execution successful, converting result...");
    ScriptValue jsResult = quickJSToJSValue(ctx, result);
    JS_FreeValue(ctx, result);
    LOG_DEBUG("JSEngine: Result conversion completed, returning success");
    return JSResult::createSuccess(jsResult);
}

JSResult JSEngine::evaluateExpressionInternal(const std::string &sessionId, const std::string &expression) {
    SPDLOG_DEBUG("JSEngine::evaluateExpressionInternal - Evaluating expression '{}' in session '{}'", expression,
                 sessionId);

    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        SPDLOG_ERROR("JSEngine::evaluateExpressionInternal - Session not found: {}", sessionId);
        return JSResult::createError("Session not found: " + sessionId);
    }

    SPDLOG_DEBUG("JSEngine::evaluateExpressionInternal - Session found, context valid");

    JSContext *ctx = session->jsContext;

    // First try to evaluate as-is
    ::JSValue result = JS_Eval(ctx, expression.c_str(), expression.length(), "<expression>", JS_EVAL_TYPE_GLOBAL);

    // If it failed and the expression starts with '{', try wrapping in parentheses for object literals
    if (JS_IsException(result) && !expression.empty() && expression[0] == '{') {
        SPDLOG_DEBUG("JSEngine::evaluateExpressionInternal - First evaluation failed, trying wrapped expression for "
                     "object literal");
        JS_FreeValue(ctx, result);  // Free the exception

        std::string wrappedExpression = "(" + expression + ")";
        result =
            JS_Eval(ctx, wrappedExpression.c_str(), wrappedExpression.length(), "<expression>", JS_EVAL_TYPE_GLOBAL);
    }

    if (JS_IsException(result)) {
        SPDLOG_ERROR("JSEngine::evaluateExpressionInternal - Final JS_Eval failed for expression '{}'", expression);

        // 루트 원인 분석: _event.data.aParam 실패 시 _event 객체 상태 확인
        if (expression.find("_event.data") != std::string::npos) {
            LOG_ERROR("JSEngine: _event.data 접근 실패 - 디버깅 정보:");

            // _event 객체 존재 확인
            ::JSValue eventCheck = JS_Eval(ctx, "_event", 6, "<debug>", JS_EVAL_TYPE_GLOBAL);
            if (JS_IsException(eventCheck)) {
                LOG_ERROR("JSEngine: _event 객체가 존재하지 않음");
                JS_FreeValue(ctx, eventCheck);
            } else if (JS_IsUndefined(eventCheck)) {
                LOG_ERROR("JSEngine: _event이 undefined임");
                JS_FreeValue(ctx, eventCheck);
            } else {
                LOG_DEBUG("JSEngine: _event 객체 존재함");

                // _event.data 확인
                ::JSValue dataCheck = JS_Eval(ctx, "_event.data", 11, "<debug>", JS_EVAL_TYPE_GLOBAL);
                if (JS_IsException(dataCheck)) {
                    LOG_ERROR("JSEngine: _event.data 접근 실패");
                    JS_FreeValue(ctx, dataCheck);
                } else if (JS_IsUndefined(dataCheck)) {
                    LOG_ERROR("JSEngine: _event.data가 undefined임");
                    JS_FreeValue(ctx, dataCheck);
                } else {
                    LOG_DEBUG("JSEngine: _event.data exists");
                }
                JS_FreeValue(ctx, dataCheck);
                JS_FreeValue(ctx, eventCheck);
            }
        }

        JSResult error = createErrorFromException(ctx);
        JS_FreeValue(ctx, result);
        return error;
    }

    SPDLOG_DEBUG("JSEngine::evaluateExpressionInternal - JS_Eval succeeded for expression '{}'", expression);

    ScriptValue jsResult = quickJSToJSValue(ctx, result);
    JS_FreeValue(ctx, result);

    // Debug logging for ScriptValue conversion
    std::string debug_type = "unknown";
    std::string debug_value = "unknown";
    if (std::holds_alternative<ScriptUndefined>(jsResult)) {
        debug_type = "ScriptUndefined";
        debug_value = "undefined";
    } else if (std::holds_alternative<ScriptNull>(jsResult)) {
        debug_type = "ScriptNull";
        debug_value = "null";
    } else if (std::holds_alternative<bool>(jsResult)) {
        debug_type = "bool";
        debug_value = std::get<bool>(jsResult) ? "true" : "false";
    } else if (std::holds_alternative<int64_t>(jsResult)) {
        debug_type = "int64_t";
        debug_value = std::to_string(std::get<int64_t>(jsResult));
    } else if (std::holds_alternative<double>(jsResult)) {
        debug_type = "double";
        debug_value = std::to_string(std::get<double>(jsResult));
    } else if (std::holds_alternative<std::string>(jsResult)) {
        debug_type = "string";
        debug_value = "\"" + std::get<std::string>(jsResult) + "\"";
    }

    LOG_TRACE("JSEngine::evaluateExpressionInternal - Expression='{}', type={}, value={}", expression, debug_type,
              debug_value);

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
    SPDLOG_DEBUG("JSEngine::setVariableInternal - Setting variable '{}' in session '{}'", name, sessionId);

    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        SPDLOG_ERROR("JSEngine::setVariableInternal - Session not found: {}", sessionId);
        return JSResult::createError("Session not found: " + sessionId);
    }

    JSContext *ctx = session->jsContext;
    ::JSValue global = JS_GetGlobalObject(ctx);

    // Log the value using variant visit pattern
    std::string valueStr = std::visit(
        [](const auto &v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return "STRING: '" + v + "'";
            } else if constexpr (std::is_same_v<T, bool>) {
                return "BOOLEAN: " + std::string(v ? "true" : "false");
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return "NUMBER(int64): " + std::to_string(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return "NUMBER(double): " + std::to_string(v);
            } else if constexpr (std::is_same_v<T, std::shared_ptr<ScriptArray>>) {
                return "ARRAY: [" + std::to_string(v->elements.size()) + " elements]";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<ScriptObject>>) {
                return "OBJECT: [" + std::to_string(v->properties.size()) + " properties]";
            } else if constexpr (std::is_same_v<T, ScriptNull>) {
                return "NULL";
            } else if constexpr (std::is_same_v<T, ScriptUndefined>) {
                return "UNDEFINED";
            } else {
                return "UNKNOWN_TYPE";
            }
        },
        value);

    SPDLOG_DEBUG("JSEngine::setVariableInternal - Variable '{}' value: {}", name, valueStr);

    ::JSValue qjsValue = jsValueToQuickJS(ctx, value);

    // Check if conversion was successful
    if (JS_IsException(qjsValue)) {
        SPDLOG_ERROR("JSEngine::setVariableInternal - Failed to convert ScriptValue to QuickJS value for variable '{}'",
                     name);
        JS_FreeValue(ctx, global);
        return createErrorFromException(ctx);
    }

    // Set the property
    int result = JS_SetPropertyStr(ctx, global, name.c_str(), qjsValue);
    if (result < 0) {
        SPDLOG_ERROR("JSEngine::setVariableInternal - Failed to set property '{}' in global object", name);
        JS_FreeValue(ctx, global);
        return JSResult::createError("Failed to set variable: " + name);
    }

    JS_FreeValue(ctx, global);

    // Track pre-initialized variable for datamodel initialization optimization
    session->preInitializedVars.insert(name);

    SPDLOG_DEBUG("JSEngine::setVariableInternal - Successfully set variable '{}' in session '{}'", name, sessionId);
    return JSResult::createSuccess();
}

JSResult JSEngine::getVariableInternal(const std::string &sessionId, const std::string &name) {
    SPDLOG_DEBUG("JSEngine::getVariableInternal - Getting variable '{}' from session '{}'", name, sessionId);

    SessionContext *session = getSession(sessionId);
    if (!session || !session->jsContext) {
        SPDLOG_ERROR("JSEngine::getVariableInternal - Session not found: {}", sessionId);
        return JSResult::createError("Session not found: " + sessionId);
    }

    SPDLOG_DEBUG("JSEngine::getVariableInternal - Session found, context valid");

    JSContext *ctx = session->jsContext;
    ::JSValue global = JS_GetGlobalObject(ctx);

    // First check if the property exists before getting it
    JSAtom atom = JS_NewAtom(ctx, name.c_str());
    int hasProperty = JS_HasProperty(ctx, global, atom);
    SPDLOG_DEBUG("JSEngine::getVariableInternal - JS_HasProperty('{}') returned: {}", name, hasProperty);
    JS_FreeAtom(ctx, atom);
    (void)hasProperty;  // Suppress unused variable warning

    ::JSValue qjsValue = JS_GetPropertyStr(ctx, global, name.c_str());

    if (JS_IsException(qjsValue)) {
        SPDLOG_ERROR("JSEngine::getVariableInternal - JS_GetPropertyStr failed for variable '{}'", name);
        JS_FreeValue(ctx, global);
        return createErrorFromException(ctx);
    }

    // Check if the property actually exists (not just undefined)
    if (JS_IsUndefined(qjsValue)) {
        SPDLOG_DEBUG("JSEngine::getVariableInternal - Variable '{}' is undefined, checking if property exists", name);
        // Use JS_HasProperty to distinguish between "not set" and "set to
        // undefined"
        JSAtom atom2 = JS_NewAtom(ctx, name.c_str());
        int hasProperty2 = JS_HasProperty(ctx, global, atom2);
        JS_FreeAtom(ctx, atom2);  // Free the atom to prevent memory leak
        SPDLOG_DEBUG("JSEngine::getVariableInternal - Second JS_HasProperty('{}') returned: {}", name, hasProperty2);
        if (hasProperty2 <= 0) {
            // Property doesn't exist - this is not an error, caller will handle
            SPDLOG_DEBUG("JSEngine::getVariableInternal - Variable '{}' does not exist in global context", name);
            JS_FreeValue(ctx, qjsValue);
            JS_FreeValue(ctx, global);
            return JSResult::createError("Variable not found: " + name);
        }
        // Property exists but is undefined - this is valid, continue with existing
        // qjsValue
        SPDLOG_DEBUG("JSEngine::getVariableInternal - Variable '{}' exists but is set to undefined", name);
    } else {
        SPDLOG_DEBUG("JSEngine::getVariableInternal - Variable '{}' found with value", name);
    }

    ScriptValue result = quickJSToJSValue(ctx, qjsValue);
    JS_FreeValue(ctx, qjsValue);
    JS_FreeValue(ctx, global);

    SPDLOG_DEBUG("JSEngine::getVariableInternal - Successfully retrieved variable '{}'", name);
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

    // Update internal __eventData object (bypasses read-only _event protection)
    ::JSValue eventDataProperty = JS_GetPropertyStr(ctx, global, "__eventData");
    if (!JS_IsObject(eventDataProperty)) {
        JS_FreeValue(ctx, eventDataProperty);
        JS_FreeValue(ctx, eventObj);
        JS_FreeValue(ctx, global);
        return JSResult::createError("__eventData object not found");
    }

    if (event) {
        // Set event properties on the internal data object
        JS_SetPropertyStr(ctx, eventDataProperty, "name", JS_NewString(ctx, event->getName().c_str()));
        JS_SetPropertyStr(ctx, eventDataProperty, "type", JS_NewString(ctx, event->getType().c_str()));
        JS_SetPropertyStr(ctx, eventDataProperty, "sendid", JS_NewString(ctx, event->getSendId().c_str()));
        JS_SetPropertyStr(ctx, eventDataProperty, "origin", JS_NewString(ctx, event->getOrigin().c_str()));
        JS_SetPropertyStr(ctx, eventDataProperty, "origintype", JS_NewString(ctx, event->getOriginType().c_str()));
        JS_SetPropertyStr(ctx, eventDataProperty, "invokeid", JS_NewString(ctx, event->getInvokeId().c_str()));

        // Parse and set event data as JSON or fallback to undefined
        if (event->hasData()) {
            std::string dataStr = event->getDataAsString();
            LOG_DEBUG("JSEngine: Setting event data from string: '{}'", dataStr);
            ::JSValue dataValue = JS_ParseJSON(ctx, dataStr.c_str(), dataStr.length(), "<event-data>");
            if (!JS_IsException(dataValue)) {
                JS_SetPropertyStr(ctx, eventDataProperty, "data", dataValue);
                LOG_DEBUG("JSEngine: Successfully parsed and set event data JSON");
            } else {
                // Log JSON parsing error
                ::JSValue exception = JS_GetException(ctx);
                const char *errorStr = JS_ToCString(ctx, exception);
                LOG_ERROR("JSEngine: Failed to parse event data as JSON: '{}', error: {}", dataStr,
                          errorStr ? errorStr : "unknown");
                if (errorStr) {
                    JS_FreeCString(ctx, errorStr);
                }
                JS_FreeValue(ctx, exception);
                JS_SetPropertyStr(ctx, eventDataProperty, "data", JS_UNDEFINED);
            }
        } else {
            LOG_DEBUG("JSEngine: Event has no data, setting _event.data to undefined");
            JS_SetPropertyStr(ctx, eventDataProperty, "data", JS_UNDEFINED);
        }
    } else {
        // Reset all event properties to empty/undefined values
        const char *props[] = {"name", "type", "sendid", "origin", "origintype", "invokeid"};
        for (int i = 0; i < 6; i++) {
            JS_SetPropertyStr(ctx, eventDataProperty, props[i], JS_NewString(ctx, ""));
        }
        JS_SetPropertyStr(ctx, eventDataProperty, "data", JS_UNDEFINED);
    }

    JS_FreeValue(ctx, eventDataProperty);
    JS_FreeValue(ctx, eventObj);
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
    // SCXML W3C Compliance: Handle null and undefined distinctly
    if (JS_IsUndefined(qjsValue)) {
        return ScriptUndefined{};
    } else if (JS_IsNull(qjsValue)) {
        return ScriptNull{};
    } else if (JS_IsBool(qjsValue)) {
        return JS_ToBool(ctx, qjsValue) ? true : false;
    } else if (JS_IsNumber(qjsValue)) {
        // JavaScript numbers are always double (IEEE 754)
        double d;
        JS_ToFloat64(ctx, &d, qjsValue);

        LOG_TRACE("JSEngine::quickJSToJSValue - JS_IsNumber=true, extracted double={}", d);

        // SCXML W3C compliance: Return as int64_t if it's a whole number within range
        if (d == floor(d) && d >= LLONG_MIN && d <= LLONG_MAX) {
            int64_t int_result = static_cast<int64_t>(d);
            LOG_TRACE("JSEngine::quickJSToJSValue - Converting to int64_t={}", int_result);
            return int_result;
        }
        LOG_TRACE("JSEngine::quickJSToJSValue - Returning as double={}", d);
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

    // Default fallback for unknown types
    return ScriptUndefined{};
}

JSValue JSEngine::jsValueToQuickJS(JSContext *ctx, const ScriptValue &value) {
    return std::visit(
        [this, ctx](const auto &v) -> JSValue {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, ScriptUndefined>) {
                return JS_UNDEFINED;
            } else if constexpr (std::is_same_v<T, ScriptNull>) {
                return JS_NULL;
            } else if constexpr (std::is_same_v<T, bool>) {
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
    if (JS_IsNull(exception)) {
        return JSResult::createError("JavaScript error: Exception is null");
    }
    const char *errorStr = JS_ToCString(ctx, exception);
    std::string errorMessage;
    if (errorStr) {
        errorMessage = std::string("JavaScript error: ") + errorStr;
        JS_FreeCString(ctx, errorStr);
    } else {
        errorMessage = "Unknown JavaScript error - could not get error string";
    }
    // Try to get stack trace
    ::JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
    if (!JS_IsUndefined(stack)) {
        const char *stackStr = JS_ToCString(ctx, stack);
        if (stackStr) {
            errorMessage += "\nStack: " + std::string(stackStr);
            JS_FreeCString(ctx, stackStr);
        }
    }
    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, exception);
    return JSResult::createError(errorMessage);
}
}  // namespace RSM
