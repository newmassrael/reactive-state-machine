#include "runtime/QuickJSEngine.h"
#include "runtime/QuickJSEngine.h"
#include "runtime/RuntimeContext.h"
#include "events/Event.h"
#include "common/Logger.h"
#include <stdexcept>
#include <cstring>

namespace SCXML {

QuickJSEngine::QuickJSEngine() 
    : runtime_(nullptr), context_(nullptr) {
    SCXML::Common::Logger::debug("QuickJSEngine::Constructor - Creating QuickJS engine");
}

QuickJSEngine::~QuickJSEngine() {
    shutdown();
    SCXML::Common::Logger::debug("QuickJSEngine::Destructor - QuickJS engine destroyed");
}

bool QuickJSEngine::initialize() {
    SCXML::Common::Logger::debug("QuickJSEngine::initialize - Initializing QuickJS engine");
    
    try {
        // Create QuickJS runtime
        runtime_ = JS_NewRuntime();
        if (!runtime_) {
            SCXML::Common::Logger::error("QuickJSEngine::initialize - Failed to create JS runtime");
            return false;
        }
        
        // Create QuickJS context
        context_ = JS_NewContext(runtime_);
        if (!context_) {
            SCXML::Common::Logger::error("QuickJSEngine::initialize - Failed to create JS context");
            JS_FreeRuntime(runtime_);
            runtime_ = nullptr;
            return false;
        }
        
        // Set this instance as context opaque for native function callbacks
        JS_SetContextOpaque(context_, this);
        
        // Setup SCXML built-in functions and objects
        setupSCXMLBuiltins();
        setupConsoleObject();
        setupMathObject();
        setupJSONObject();
        
        SCXML::Common::Logger::debug("QuickJSEngine::initialize - QuickJS engine initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("QuickJSEngine::initialize - Exception: " + std::string(e.what()));
        shutdown();
        return false;
    }
}

void QuickJSEngine::shutdown() {
    if (context_) {
        // Clear our internal containers first
        nativeFunctions_.clear();
        currentEvent_.reset();
        
        // Get global object and remove our properties
        JSValue global = JS_GetGlobalObject(context_);
        
        JSAtom inAtom = JS_NewAtom(context_, "In");
        JS_DeleteProperty(context_, global, inAtom, 0);
        JS_FreeAtom(context_, inAtom);
        
        JSAtom consoleAtom = JS_NewAtom(context_, "console");
        JS_DeleteProperty(context_, global, consoleAtom, 0);
        JS_FreeAtom(context_, consoleAtom);
        
        JS_FreeValue(context_, global);
        
        // Force garbage collection
        if (runtime_) {
            JS_RunGC(runtime_);
        }
        
        JS_FreeContext(context_);
        context_ = nullptr;
    }
    
    if (runtime_) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
}

IECMAScriptEngine::ECMAResult QuickJSEngine::evaluateExpression(
    const std::string &expression, ::SCXML::Runtime::RuntimeContext &context [[maybe_unused]]) {
    
    SCXML::Common::Logger::debug("QuickJSEngine::evaluateExpression - Evaluating: " + expression);
    
    if (!context_ || !runtime_) {
        return ECMAResult::createError("QuickJS engine not initialized");
    }
    
    try {
        // Update event context if needed
        setupEventObject();
        
        // Evaluate the expression
        JSValue result = JS_Eval(context_, expression.c_str(), expression.length(), 
                                "<eval>", JS_EVAL_TYPE_GLOBAL);
        
        // Check for exceptions
        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(context_);
            const char* str = JS_ToCString(context_, exception);
            std::string errorMsg = str ? str : "Unknown JavaScript error";
            JS_FreeCString(context_, str);
            JS_FreeValue(context_, exception);
            JS_FreeValue(context_, result);
            
            SCXML::Common::Logger::error("QuickJSEngine::evaluateExpression - JavaScript error: " + errorMsg);
            return ECMAResult::createError(errorMsg);
        }
        
        // Convert result to ECMAValue
        ECMAValue ecmaValue = jsValueToECMAValue(result);
        JS_FreeValue(context_, result);
        
        SCXML::Common::Logger::debug("QuickJSEngine::evaluateExpression - Evaluation successful");
        return ECMAResult::createSuccess(ecmaValue);
        
    } catch (const std::exception& e) {
        std::string errorMsg = "QuickJSEngine::evaluateExpression - Exception: " + std::string(e.what());
        SCXML::Common::Logger::error(errorMsg);
        return ECMAResult::createError(errorMsg);
    }
}

IECMAScriptEngine::ECMAResult QuickJSEngine::executeScript(
    const std::string &script, ::SCXML::Runtime::RuntimeContext &context) {
    
    SCXML::Common::Logger::debug("QuickJSEngine::executeScript - Executing script");
    
    return evaluateExpression(script, context);
}

bool QuickJSEngine::setVariable(const std::string &name, const ECMAValue &value) {
    SCXML::Common::Logger::debug("QuickJSEngine::setVariable - Setting variable: " + name);
    
    if (!context_) {
        SCXML::Common::Logger::error("QuickJSEngine::setVariable - Engine not initialized");
        return false;
    }
    
    try {
        JSValue jsValue;
        
        // Special handling for strings that might be JSON
        if (std::holds_alternative<std::string>(value)) {
            const std::string& strValue = std::get<std::string>(value);
            
            // Try to parse as JSON if it looks like JSON
            if (!strValue.empty() && (strValue[0] == '{' || strValue[0] == '[')) {
                JSValue parsed = JS_ParseJSON(context_, strValue.c_str(), strValue.length(), ("<var-" + name + ">").c_str());
                if (!JS_IsException(parsed)) {
                    // Successfully parsed as JSON
                    jsValue = parsed;
                } else {
                    // Clear exception and use as regular string
                    JSValue exception = JS_GetException(context_);
                    JS_FreeValue(context_, exception);
                    jsValue = JS_NewString(context_, strValue.c_str());
                }
            } else {
                // Regular string
                jsValue = JS_NewString(context_, strValue.c_str());
            }
        } else {
            // Use normal ECMAValue to JSValue conversion
            jsValue = ecmaValueToJSValue(value);
        }
        
        JSValue global = JS_GetGlobalObject(context_);
        int result = JS_SetPropertyStr(context_, global, name.c_str(), jsValue);
        JS_FreeValue(context_, global);
        
        if (result < 0) {
            SCXML::Common::Logger::error("QuickJSEngine::setVariable - Failed to set variable: " + name);
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("QuickJSEngine::setVariable - Exception: " + std::string(e.what()));
        return false;
    }
}

IECMAScriptEngine::ECMAResult QuickJSEngine::getVariable(const std::string &name) {
    SCXML::Common::Logger::debug("QuickJSEngine::getVariable - Getting variable: " + name);
    
    if (!context_) {
        return ECMAResult::createError("QuickJS engine not initialized");
    }
    
    try {
        JSValue global = JS_GetGlobalObject(context_);
        JSValue jsValue = JS_GetPropertyStr(context_, global, name.c_str());
        JS_FreeValue(context_, global);
        
        if (JS_IsException(jsValue)) {
            JSValue exception = JS_GetException(context_);
            const char* str = JS_ToCString(context_, exception);
            std::string errorMsg = str ? str : "Unknown error getting variable";
            JS_FreeCString(context_, str);
            JS_FreeValue(context_, exception);
            JS_FreeValue(context_, jsValue);
            return ECMAResult::createError(errorMsg);
        }
        
        if (JS_IsUndefined(jsValue)) {
            JS_FreeValue(context_, jsValue);
            return ECMAResult::createError("Variable not found: " + name);
        }
        
        ECMAValue ecmaValue = jsValueToECMAValue(jsValue);
        JS_FreeValue(context_, jsValue);
        
        return ECMAResult::createSuccess(ecmaValue);
        
    } catch (const std::exception& e) {
        std::string errorMsg = "QuickJSEngine::getVariable - Exception: " + std::string(e.what());
        return ECMAResult::createError(errorMsg);
    }
}

void QuickJSEngine::setCurrentEvent(const std::shared_ptr<::SCXML::Events::Event> &event) {
    SCXML::Common::Logger::debug("QuickJSEngine::setCurrentEvent - Setting current event");
    currentEvent_ = event;
    setupEventObject();
}

void QuickJSEngine::setStateCheckFunction(const StateCheckFunction &func) {
    stateCheckFunction_ = func;
}

void QuickJSEngine::setupSCXMLSystemVariables(const std::string &sessionId, 
                                             const std::string &sessionName,
                                             const std::vector<std::string> &ioProcessors) {
    SCXML::Common::Logger::debug("QuickJSEngine::setupSCXMLSystemVariables - Setting up system variables");
    
    if (!context_) {
        return;
    }
    
    try {
        // Set _sessionid
        setVariable("_sessionid", ECMAValue(sessionId));
        
        // Set _name
        setVariable("_name", ECMAValue(sessionName));
        
        // Set _ioprocessors (as array of strings)
        JSValue ioArray = JS_NewArray(context_);
        for (size_t i = 0; i < ioProcessors.size(); ++i) {
            JSValue str = JS_NewString(context_, ioProcessors[i].c_str());
            JS_SetPropertyUint32(context_, ioArray, static_cast<uint32_t>(i), str);
        }
        JSValue global = JS_GetGlobalObject(context_);
        JS_SetPropertyStr(context_, global, "_ioprocessors", ioArray);
        JS_FreeValue(context_, global);
        
        SCXML::Common::Logger::debug("QuickJSEngine::setupSCXMLSystemVariables - System variables set up successfully");
        
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("QuickJSEngine::setupSCXMLSystemVariables - Exception: " + std::string(e.what()));
    }
}

bool QuickJSEngine::registerNativeFunction(const std::string &name, const NativeFunction &func) {
    SCXML::Common::Logger::debug("QuickJSEngine::registerNativeFunction - Registering function: " + name);
    
    if (!context_) {
        return false;
    }
    
    try {
        // Store the native function
        nativeFunctions_[name] = func;
        
        // Create simple JS function wrapper (no dynamic allocation)
        JSValue jsFunc = JS_NewCFunction(context_, nativeFunctionWrapper, name.c_str(), 0);
        
        // Set as global property
        JSValue global = JS_GetGlobalObject(context_);
        int result = JS_SetPropertyStr(context_, global, name.c_str(), jsFunc);
        JS_FreeValue(context_, global);
        
        return result >= 0;
        
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("QuickJSEngine::registerNativeFunction - Exception: " + std::string(e.what()));
        return false;
    }
}

std::string QuickJSEngine::getEngineVersion() const {
    return "QuickJS 2025-04-26";
}

size_t QuickJSEngine::getMemoryUsage() const {
    if (!runtime_) {
        return 0;
    }
    
    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime_, &usage);
    return static_cast<size_t>(usage.memory_used_size);
}

void QuickJSEngine::collectGarbage() {
    if (runtime_) {
        JS_RunGC(runtime_);
        SCXML::Common::Logger::debug("QuickJSEngine::collectGarbage - Garbage collection completed");
    }
}

// Private helper methods

JSValue QuickJSEngine::ecmaValueToJSValue(const ECMAValue &value) {
    return std::visit([this](const auto& v) -> JSValue {
        using T = std::decay_t<decltype(v)>;
        
        if constexpr (std::is_same_v<T, std::monostate>) {
            return JS_UNDEFINED;
        } else if constexpr (std::is_same_v<T, bool>) {
            return JS_NewBool(context_, v);
        } else if constexpr (std::is_same_v<T, double>) {
            return JS_NewFloat64(context_, v);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return JS_NewString(context_, v.c_str());
        } else {
            return JS_UNDEFINED;
        }
    }, value);
}

IECMAScriptEngine::ECMAValue QuickJSEngine::jsValueToECMAValue(JSValue value) {
    if (JS_IsBool(value)) {
        return ECMAValue(JS_ToBool(context_, value) != 0);
    } else if (JS_IsNumber(value)) {
        double d;
        JS_ToFloat64(context_, &d, value);
        return ECMAValue(d);
    } else if (JS_IsString(value)) {
        const char* str = JS_ToCString(context_, value);
        std::string result = str ? str : "";
        JS_FreeCString(context_, str);
        return ECMAValue(result);
    } else if (JS_IsNull(value)) {
        return ECMAValue(std::monostate{});
    } else if (JS_IsUndefined(value)) {
        return ECMAValue(std::monostate{});
    } else if (JS_IsObject(value)) {
        // Try to convert object/array to JSON string for better representation
        JSValue jsonString = JS_JSONStringify(context_, value, JS_UNDEFINED, JS_UNDEFINED);
        if (!JS_IsException(jsonString) && JS_IsString(jsonString)) {
            const char* str = JS_ToCString(context_, jsonString);
            std::string result = str ? str : "{}";
            JS_FreeCString(context_, str);
            JS_FreeValue(context_, jsonString);
            return ECMAValue(result);
        } else {
            // Fallback to toString
            JS_FreeValue(context_, jsonString);
            const char* str = JS_ToCString(context_, value);
            std::string result = str ? str : "[object]";
            JS_FreeCString(context_, str);
            return ECMAValue(result);
        }
    }
    
    // For any other types, convert to string representation
    const char* str = JS_ToCString(context_, value);
    std::string result = str ? str : "[unknown]";
    JS_FreeCString(context_, str);
    return ECMAValue(result);
}

void QuickJSEngine::setupSCXMLBuiltins() {
    // Register In() function directly with specialized wrapper
    JSValue inFunc = JS_NewCFunction(context_, inFunctionWrapper, "In", 1);
    JSValue global = JS_GetGlobalObject(context_);
    JS_SetPropertyStr(context_, global, "In", inFunc);
    JS_FreeValue(context_, global);
}

void QuickJSEngine::setupConsoleObject() {
    // Add console.log implementation via JavaScript
    const char* consoleLogCode = R"(
        if (typeof console === 'undefined') {
            console = {};
        }
        console.log = function(...args) {
            // Simple implementation - could be enhanced
            return undefined;
        };
    )";
    
    // Properly free the result of JS_Eval
    JSValue result = JS_Eval(context_, consoleLogCode, strlen(consoleLogCode), "<console>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(context_, result);
}

void QuickJSEngine::setupMathObject() {
    // Math object is built into QuickJS, no additional setup needed
}

void QuickJSEngine::setupJSONObject() {
    // JSON object is built into QuickJS, no additional setup needed
}

void QuickJSEngine::setupEventObject() {
    if (!currentEvent_) {
        return;
    }
    
    try {
        // Create _event object
        JSValue eventObj = JS_NewObject(context_);
        
        // Set event name
        JSValue nameValue = JS_NewString(context_, currentEvent_->getName().c_str());
        JS_SetPropertyStr(context_, eventObj, "name", nameValue);
        
        // Set event data properly
        JSValue dataValue;
        if (currentEvent_->hasData()) {
            std::string dataStr = currentEvent_->getDataAsString();
            if (!dataStr.empty()) {
                // Try to parse as JSON first
                JSValue parsed = JS_ParseJSON(context_, dataStr.c_str(), dataStr.length(), "<event-data>");
                if (!JS_IsException(parsed)) {
                    // Successfully parsed as JSON object
                    dataValue = parsed;
                } else {
                    // Clear the exception and use as string
                    JSValue exception = JS_GetException(context_);
                    JS_FreeValue(context_, exception);
                    dataValue = JS_NewString(context_, dataStr.c_str());
                }
            } else {
                dataValue = JS_UNDEFINED;
            }
        } else {
            dataValue = JS_UNDEFINED;
        }
        JS_SetPropertyStr(context_, eventObj, "data", dataValue);
        
        // Set event type
        JSValue typeValue = JS_NewString(context_, 
            currentEvent_->isExternal() ? "external" : 
            currentEvent_->isInternal() ? "internal" : "platform");
        JS_SetPropertyStr(context_, eventObj, "type", typeValue);
        
        // Set as global _event
        JSValue global = JS_GetGlobalObject(context_);
        JS_SetPropertyStr(context_, global, "_event", eventObj);
        JS_FreeValue(context_, global);
        
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("QuickJSEngine::setupEventObject - Exception: " + std::string(e.what()));
    }
}

// Static wrapper for In() function  
JSValue QuickJSEngine::inFunctionWrapper(JSContext *ctx, JSValueConst this_val [[maybe_unused]],
                                        int argc, JSValueConst *argv) {
    QuickJSEngine* engine = static_cast<QuickJSEngine*>(JS_GetContextOpaque(ctx));
    if (!engine || argc == 0) {
        return JS_NewBool(ctx, false);
    }
    
    // Convert first argument to string
    const char* stateName = JS_ToCString(ctx, argv[0]);
    if (!stateName) {
        return JS_NewBool(ctx, false);
    }
    
    std::string stateNameStr(stateName);
    JS_FreeCString(ctx, stateName);
    
    // Call the state check function
    bool result = false;
    if (engine->stateCheckFunction_) {
        result = engine->stateCheckFunction_(stateNameStr);
    }
    
    return JS_NewBool(ctx, result);
}

// Static wrapper for console function
JSValue QuickJSEngine::consoleFunctionWrapper(JSContext *ctx [[maybe_unused]], JSValueConst this_val [[maybe_unused]],
                                             int argc [[maybe_unused]], JSValueConst *argv [[maybe_unused]]) {
    // Simple console implementation
    return JS_UNDEFINED;
}

// Generic static wrapper for native functions (fallback)
JSValue QuickJSEngine::nativeFunctionWrapper(JSContext *ctx [[maybe_unused]], JSValueConst this_val [[maybe_unused]],
                                            int argc [[maybe_unused]], JSValueConst *argv [[maybe_unused]]) {
    return JS_UNDEFINED;
}

} // namespace SCXML