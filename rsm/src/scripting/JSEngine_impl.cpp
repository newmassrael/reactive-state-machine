#include "scripting/JSEngine.h"
#include "quickjs.h"
#include <iostream>
using namespace RSM;


namespace RSM {


// === Internal JavaScript Execution Methods ===


JSResult JSEngine::executeScriptInternal(const std::string& sessionId, const std::string& script) {
    std::cout << "JSEngine: executeScriptInternal - sessionId: " << sessionId << ", script: " << script << std::endl;
    SessionContext* session = getSession(sessionId);
    if (!session || !session->jsContext) {
        std::cout << "JSEngine: Session not found or no context: " << sessionId << std::endl;
        return JSResult::createError("Session not found: " + sessionId);
    }


    JSContext* ctx = session->jsContext;
    std::cout << "JSEngine: Evaluating script with QuickJS..." << std::endl;
    ::JSValue result = JS_Eval(ctx, script.c_str(), script.length(), "<script>", JS_EVAL_TYPE_GLOBAL);


    std::cout << "JSEngine: JS_Eval completed, checking result..." << std::endl;
    std::cout << "JSEngine: JS_IsException(result): " << (JS_IsException(result) ? "true" : "false") << std::endl;
    std::cout << "JSEngine: JS_IsUndefined(result): " << (JS_IsUndefined(result) ? "true" : "false") << std::endl;
    std::cout << "JSEngine: JS_IsNull(result): " << (JS_IsNull(result) ? "true" : "false") << std::endl;
    std::cout << "JSEngine: JS_IsNumber(result): " << (JS_IsNumber(result) ? "true" : "false") << std::endl;


    if (JS_IsException(result)) {
        std::cout << "JSEngine: QuickJS exception occurred" << std::endl;
        JSResult error = createErrorFromException(ctx);
        JS_FreeValue(ctx, result);
        std::cout << "JSEngine: Error: " << error.errorMessage << std::endl;
        return error;
    }


    std::cout << "JSEngine: Script executed successfully, converting result..." << std::endl;
    ScriptValue jsResult = quickJSToJSValue(ctx, result);
    JS_FreeValue(ctx, result);
    std::cout << "JSEngine: Result converted, returning success" << std::endl;
    return JSResult::createSuccess(jsResult);
}


JSResult JSEngine::evaluateExpressionInternal(const std::string& sessionId, const std::string& expression) {
    SessionContext* session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }


    JSContext* ctx = session->jsContext;
    ::JSValue result = JS_Eval(ctx, expression.c_str(), expression.length(), "<expression>", JS_EVAL_TYPE_GLOBAL);


    if (JS_IsException(result)) {
        JSResult error = createErrorFromException(ctx);
        JS_FreeValue(ctx, result);
        return error;
    }


    ScriptValue jsResult = quickJSToJSValue(ctx, result);
    JS_FreeValue(ctx, result);
    return JSResult::createSuccess(jsResult);
}


JSResult JSEngine::setVariableInternal(const std::string& sessionId, const std::string& name, const ScriptValue& value) {
    SessionContext* session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }


    JSContext* ctx = session->jsContext;
    ::JSValue global = JS_GetGlobalObject(ctx);
    ::JSValue qjsValue = jsValueToQuickJS(ctx, value);


    JS_SetPropertyStr(ctx, global, name.c_str(), qjsValue);
    JS_FreeValue(ctx, global);


    return JSResult::createSuccess();
}


JSResult JSEngine::getVariableInternal(const std::string& sessionId, const std::string& name) {
    SessionContext* session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }


    JSContext* ctx = session->jsContext;
    ::JSValue global = JS_GetGlobalObject(ctx);
    ::JSValue qjsValue = JS_GetPropertyStr(ctx, global, name.c_str());


    if (JS_IsException(qjsValue)) {
        JS_FreeValue(ctx, global);
        return createErrorFromException(ctx);
    }


    // Check if the property actually exists (not just undefined)
    if (JS_IsUndefined(qjsValue)) {
        // Use JS_HasProperty to distinguish between "not set" and "set to undefined"
        JSAtom atom = JS_NewAtom(ctx, name.c_str());
        int hasProperty = JS_HasProperty(ctx, global, atom);
        JS_FreeAtom(ctx, atom);  // Free the atom to prevent memory leak
        if (hasProperty <= 0) {
            // Property doesn't exist - this is an error
            JS_FreeValue(ctx, qjsValue);
            JS_FreeValue(ctx, global);
            return JSResult::createError("Variable not found: " + name);
        }
        // Property exists but is undefined - this is valid, continue with existing qjsValue
    }


    ScriptValue result = quickJSToJSValue(ctx, qjsValue);
    JS_FreeValue(ctx, qjsValue);
    JS_FreeValue(ctx, global);


    return JSResult::createSuccess(result);
}


JSResult JSEngine::setCurrentEventInternal(const std::string& sessionId, const std::shared_ptr<Event>& event) {
    SessionContext* session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }


    JSContext* ctx = session->jsContext;
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


JSResult JSEngine::setupSystemVariablesInternal(const std::string& sessionId,
                                               const std::string& sessionName,
                                               const std::vector<std::string>& ioProcessors) {
    SessionContext* session = getSession(sessionId);
    if (!session || !session->jsContext) {
        return JSResult::createError("Session not found: " + sessionId);
    }


    JSContext* ctx = session->jsContext;
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


ScriptValue JSEngine::quickJSToJSValue(JSContext* ctx, JSValue qjsValue) {
    if (JS_IsBool(qjsValue)) {
        return JS_ToBool(ctx, qjsValue) ? true : false;
    } else if (JS_IsNumber(qjsValue)) {
        // JavaScript numbers are always double (IEEE 754)
        double d;
        JS_ToFloat64(ctx, &d, qjsValue);
        return d;
    } else if (JS_IsString(qjsValue)) {
        const char* str = JS_ToCString(ctx, qjsValue);
        std::string result(str ? str : "");
        if (str) {
            JS_FreeCString(ctx, str);
        }
        return result;
    }


    return std::monostate{};  // undefined/null
}


JSValue JSEngine::jsValueToQuickJS(JSContext* ctx, const ScriptValue& value) {
    return std::visit([ctx](const auto& v) -> JSValue {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            return JS_NewBool(ctx, v);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return JS_NewInt64(ctx, v);
        } else if constexpr (std::is_same_v<T, double>) {
            return JS_NewFloat64(ctx, v);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return JS_NewString(ctx, v.c_str());
        } else {
            return JS_UNDEFINED;
        }
    }, value);
}


// === Error Handling ===


JSResult JSEngine::createErrorFromException(JSContext* ctx) {
    ::JSValue exception = JS_GetException(ctx);
    std::cout << "JSEngine: Getting exception details..." << std::endl;
    if (JS_IsNull(exception)) {
        std::cout << "JSEngine: Exception is null" << std::endl;
        return JSResult::createError("JavaScript error: Exception is null");
    }
    const char* errorStr = JS_ToCString(ctx, exception);
    std::string errorMessage;
    if (errorStr) {
        errorMessage = std::string("JavaScript error: ") + errorStr;
        std::cout << "JSEngine: Exception message: " << errorStr << std::endl;
        JS_FreeCString(ctx, errorStr);
    } else {
        errorMessage = "Unknown JavaScript error - could not get error string";
        std::cout << "JSEngine: Could not get error string from exception" << std::endl;
    }
    // Try to get stack trace
    ::JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
    if (!JS_IsUndefined(stack)) {
        const char* stackStr = JS_ToCString(ctx, stack);
        if (stackStr) {
            std::cout << "JSEngine: Stack trace: " << stackStr << std::endl;
            errorMessage += "\nStack: " + std::string(stackStr);
            JS_FreeCString(ctx, stackStr);
        }
    }
    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, exception);
    std::cout << "JSEngine: Final error message: " << errorMessage << std::endl;
    return JSResult::createError(errorMessage);
}


}  // namespace RSM
