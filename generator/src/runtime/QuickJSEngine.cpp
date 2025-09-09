#include "runtime/QuickJSEngine.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "runtime/JSValueManager.h"
#include "runtime/RuntimeContext.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace SCXML {

QuickJSEngine::QuickJSEngine() : runtime_(nullptr), context_(nullptr) {}

QuickJSEngine::~QuickJSEngine() {
    shutdown();
}

bool QuickJSEngine::initialize() {
    try {
        // Create QuickJS runtime
        runtime_ = JS_NewRuntime();
        if (!runtime_) {
            SCXML::Common::Logger::error("QuickJSEngine::initialize - Failed to create JS runtime");
            return false;
        }

        // Create QuickJS context with standard objects
        context_ = JS_NewContext(runtime_);
        if (!context_) {
            SCXML::Common::Logger::error("QuickJSEngine::initialize - Failed to create JS context");
            JS_FreeRuntime(runtime_);
            runtime_ = nullptr;
            return false;
        }

        // Debug: Log context address for tracking
        SCXML::Common::Logger::error("QuickJSEngine::initialize - Created new QuickJS context at address: " +
                                     std::to_string(reinterpret_cast<uintptr_t>(context_)));

        // Set this instance as context opaque for native function callbacks
        JS_SetContextOpaque(context_, this);

        // QuickJS is working with basic operators - the problem might be elsewhere

        // Setup SCXML built-in functions and objects
        setupSCXMLBuiltins();
        setupConsoleObject();
        setupMathObject();
        setupJSONObject();

        return true;

    } catch (const std::exception &e) {
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

        // Clear all tracked JSValues before context destruction
        JSValueTracker::clearAll(context_);

        // Simple cleanup - just run GC a few times
        if (runtime_) {
            // Execute pending jobs
            while (JS_IsJobPending(runtime_)) {
                JSContext *ctx = context_;
                if (JS_ExecutePendingJob(runtime_, &ctx) < 0) {
                    break;
                }
            }

            // Run garbage collection
            for (int i = 0; i < 3; i++) {
                JS_RunGC(runtime_);
            }
        }

        JS_FreeContext(context_);
        context_ = nullptr;
    }

    if (runtime_) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
}

IECMAScriptEngine::ECMAResult QuickJSEngine::evaluateExpression(const std::string &expression,
                                                                ::SCXML::Runtime::RuntimeContext &context
                                                                [[maybe_unused]]) {
    if (!context_) {
        return ECMAResult::createError("Engine not initialized");
    }

    try {
        // Safety check for empty or invalid expressions
        if (expression.empty()) {
            SCXML::Common::Logger::debug("QuickJSEngine::evaluateExpression - Empty expression, returning undefined");
            return ECMAResult::createSuccess(ECMAValue(std::monostate{}));
        }

        // Check for problematic expressions that could cause segfaults
        // Handle case where expression is exactly two single quotes (empty string literal) - return empty string
        if (expression == "''") {
            SCXML::Common::Logger::error("QuickJSEngine::evaluateExpression - Converting '' to empty string");
            return ECMAResult::createSuccess(ECMAValue(std::string("")));
        }

        // Debug: Log expression details to understand the pattern
        if (expression.find("'") != std::string::npos) {
            SCXML::Common::Logger::error("QuickJSEngine::evaluateExpression - Expression with quotes detected: '" +
                                         expression + "', length=" + std::to_string(expression.length()));
            for (size_t i = 0; i < expression.length(); ++i) {
                SCXML::Common::Logger::error("QuickJSEngine::evaluateExpression - Char[" + std::to_string(i) +
                                             "] = " + std::to_string(static_cast<int>(expression[i])) + " ('" +
                                             std::string(1, expression[i]) + "')");
            }
        }

        if (false || expression == "\"\"\"\"" || expression.find("''''") != std::string::npos) {
            SCXML::Common::Logger::error("QuickJSEngine::evaluateExpression - Detected problematic quote pattern: '" +
                                         expression + "'");
            return ECMAResult::createError("Invalid quote pattern in expression");
        }

        // Debug logging for expression
        SCXML::Common::Logger::debug("QuickJSEngine::evaluateExpression - About to evaluate: '" + expression + "'");
        SCXML::Common::Logger::error("QuickJSEngine::evaluateExpression - Using QuickJS context at address: " +
                                     std::to_string(reinterpret_cast<uintptr_t>(context_)));

        // Debug: Check if variables exist before evaluation (only for problematic expressions)
        if (expression.find("counter") != std::string::npos || expression.find("threshold") != std::string::npos) {
            JSValueWrapper global(context_, JS_GetGlobalObject(context_), "debug_global");
            JSValueWrapper counterVal(context_, JS_GetPropertyStr(context_, global.get(), "counter"), "debug_counter");
            JSValueWrapper thresholdVal(context_, JS_GetPropertyStr(context_, global.get(), "threshold"),
                                        "debug_threshold");

            bool counterExists = !JS_IsUndefined(counterVal.get());
            bool thresholdExists = !JS_IsUndefined(thresholdVal.get());

            // Get variable values and types for debugging
            std::string counterInfo = "undefined";
            std::string thresholdInfo = "undefined";

            if (counterExists) {
                if (JS_IsNumber(counterVal.get())) {
                    double counterDouble;
                    JS_ToFloat64(context_, &counterDouble, counterVal.get());
                    counterInfo = "number(" + std::to_string(counterDouble) + ")";
                } else if (JS_IsString(counterVal.get())) {
                    const char *str = JS_ToCString(context_, counterVal.get());
                    counterInfo = "string('" + std::string(str ? str : "null") + "')";
                    JS_FreeCString(context_, str);
                } else if (JS_IsBool(counterVal.get())) {
                    counterInfo =
                        "boolean(" + std::string(JS_ToBool(context_, counterVal.get()) ? "true" : "false") + ")";
                } else {
                    counterInfo = "other_type";
                }
            }

            if (thresholdExists) {
                if (JS_IsNumber(thresholdVal.get())) {
                    double thresholdDouble;
                    JS_ToFloat64(context_, &thresholdDouble, thresholdVal.get());
                    thresholdInfo = "number(" + std::to_string(thresholdDouble) + ")";
                } else if (JS_IsString(thresholdVal.get())) {
                    const char *str = JS_ToCString(context_, thresholdVal.get());
                    thresholdInfo = "string('" + std::string(str ? str : "null") + "')";
                    JS_FreeCString(context_, str);
                } else if (JS_IsBool(thresholdVal.get())) {
                    thresholdInfo =
                        "boolean(" + std::string(JS_ToBool(context_, thresholdVal.get()) ? "true" : "false") + ")";
                }
            }

            SCXML::Common::Logger::error("QuickJSEngine::evaluateExpression - Variables: counter=" + counterInfo +
                                         ", threshold=" + thresholdInfo);

            // Debug completed - context separation was not the issue
            SCXML::Common::Logger::debug("QuickJSEngine::evaluateExpression - Variable debugging complete");
        }

        // Simple evaluation without complex recursion tracking or GC calls
        JSValueWrapper result(context_,
                              JS_Eval(context_, expression.c_str(), expression.length(), "<eval>", JS_EVAL_TYPE_GLOBAL),
                              "eval_result");

        // Check for exceptions
        if (JS_IsException(result.get())) {
            JSValueWrapper exception(context_, JS_GetException(context_), "eval_exception");
            const char *exceptionStr = JS_ToCString(context_, exception.get());
            std::string errorMsg = exceptionStr ? std::string(exceptionStr) : "JavaScript evaluation failed";
            JS_FreeCString(context_, exceptionStr);

            // Add context information
            errorMsg += " (Expression: '" + expression + "')";

            SCXML::Common::Logger::error("QuickJSEngine::evaluateExpression - " + errorMsg);
            return ECMAResult::createError(errorMsg);
        }

        // Convert result to ECMAValue - ensure JSValueWrapper stays alive during conversion
        JSValue rawValue = result.get();
        ECMAValue ecmaValue = jsValueToECMAValue(rawValue);

        return ECMAResult::createSuccess(ecmaValue);

    } catch (const std::exception &e) {
        std::string errorMsg = "QuickJSEngine::evaluateExpression - Exception: " + std::string(e.what());
        SCXML::Common::Logger::error(errorMsg);
        return ECMAResult::createError(errorMsg);
    }
}

IECMAScriptEngine::ECMAResult QuickJSEngine::executeScript(const std::string &script,
                                                           ::SCXML::Runtime::RuntimeContext &context) {
    return evaluateExpression(script, context);
}

bool QuickJSEngine::setVariable(const std::string &name, const ECMAValue &value) {
    // Debug: Log context address for setVariable
    SCXML::Common::Logger::error("QuickJSEngine::setVariable - Using QuickJS context at address: " +
                                 std::to_string(reinterpret_cast<uintptr_t>(context_)) + " for variable: " + name);

    // Add debug logging for all variables to help debug DataModel issues
    if (name == "counter" || name == "threshold" || name == "events") {
        if (std::holds_alternative<std::string>(value)) {
            SCXML::Common::Logger::debug("QuickJSEngine::setVariable - Setting " + name + " = '" +
                                         std::get<std::string>(value) + "' (string)");
        } else if (std::holds_alternative<int64_t>(value)) {
            SCXML::Common::Logger::debug("QuickJSEngine::setVariable - Setting " + name + " = " +
                                         std::to_string(std::get<int64_t>(value)) + " (int64)");
        } else if (std::holds_alternative<double>(value)) {
            SCXML::Common::Logger::debug("QuickJSEngine::setVariable - Setting " + name + " = " +
                                         std::to_string(std::get<double>(value)) + " (double)");
        } else {
            SCXML::Common::Logger::debug("QuickJSEngine::setVariable - Setting " + name + " to other type");
        }
    }
    if (!context_) {
        return false;
    }

    try {
        JSValueWrapper jsValue(context_, JS_UNDEFINED, "setvar_" + name);

        if (std::holds_alternative<std::string>(value)) {
            std::string strValue = std::get<std::string>(value);

            // Trim the string value
            std::string trimmed = strValue;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
            trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

            // Try to parse as number first (important for DataModel variables like counter, threshold)
            if (!trimmed.empty() && (std::isdigit(trimmed[0]) || trimmed[0] == '-' || trimmed[0] == '+')) {
                try {
                    // Try integer first
                    if (trimmed.find('.') == std::string::npos) {
                        int64_t intValue = std::stoll(trimmed);
                        jsValue = JSValueWrapper(context_, JS_NewInt64(context_, intValue), "setvar_int_" + name);
                    } else {
                        // Try double
                        double doubleValue = std::stod(trimmed);
                        jsValue =
                            JSValueWrapper(context_, JS_NewFloat64(context_, doubleValue), "setvar_double_" + name);
                    }
                } catch (const std::exception &e) {
                    // Not a number, treat as string
                    jsValue = JSValueWrapper(context_, JS_NewString(context_, strValue.c_str()), "setvar_str_" + name);
                }
            }
            // Check if string looks like JSON (object or array)
            else if (!trimmed.empty() && ((trimmed.front() == '{' && trimmed.back() == '}') ||
                                          (trimmed.front() == '[' && trimmed.back() == ']'))) {
                // Try to parse as JSON
                JSValueWrapper parsed(context_,
                                      JS_ParseJSON(context_, trimmed.c_str(), trimmed.length(), "<setVariable>"),
                                      "setvar_json_" + name);
                if (!JS_IsException(parsed.get())) {
                    jsValue = std::move(parsed);  // Use the parsed JSON object/array
                } else {
                    // If JSON parsing fails, clear the exception and treat as string
                    JSValueWrapper exception(context_, JS_GetException(context_), "json_parse_exception");
                    jsValue = JSValueWrapper(context_, JS_NewString(context_, strValue.c_str()),
                                             "setvar_str_fallback_" + name);
                }
            } else {
                // Regular string
                jsValue = JSValueWrapper(context_, JS_NewString(context_, strValue.c_str()), "setvar_string_" + name);
            }
        } else if (std::holds_alternative<bool>(value)) {
            jsValue = JSValueWrapper(context_, JS_NewBool(context_, std::get<bool>(value)), "setvar_bool_" + name);
        } else if (std::holds_alternative<int64_t>(value)) {
            jsValue = JSValueWrapper(context_, JS_NewInt64(context_, std::get<int64_t>(value)), "setvar_int64_" + name);
        } else if (std::holds_alternative<double>(value)) {
            jsValue =
                JSValueWrapper(context_, JS_NewFloat64(context_, std::get<double>(value)), "setvar_double_" + name);
        } else {
            // null case
            jsValue = JSValueWrapper(context_, JS_NULL, "setvar_null_" + name);
        }

        JSValueWrapper global(context_, JS_GetGlobalObject(context_), "setvar_global");
        int result = JS_SetPropertyStr(context_, global.get(), name.c_str(), JS_DupValue(context_, jsValue.get()));

        if (result < 0) {
            SCXML::Common::Logger::error("QuickJSEngine::setVariable - Failed to set variable: " + name);
            return false;
        }

        return true;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("QuickJSEngine::setVariable - Exception: " + std::string(e.what()));
        return false;
    }
}

IECMAScriptEngine::ECMAResult QuickJSEngine::getVariable(const std::string &name) {
    // Add debug logging for events variable
    if (name == "events") {
        SCXML::Common::Logger::debug("QuickJSEngine::getVariable - Requesting events variable");
    }
    if (!context_) {
        return ECMAResult::createError("Engine not initialized");
    }

    try {
        JSValueWrapper global(context_, JS_GetGlobalObject(context_), "getvar_global");
        JSValueWrapper jsValue(context_, JS_GetPropertyStr(context_, global.get(), name.c_str()), "getvar_" + name);

        if (JS_IsException(jsValue.get())) {
            JSValueWrapper exception(context_, JS_GetException(context_), "getvar_exception");
            const char *str = JS_ToCString(context_, exception.get());
            std::string errorMsg = str ? str : "Unknown error getting variable";
            JS_FreeCString(context_, str);
            return ECMAResult::createError(errorMsg);
        }

        if (JS_IsUndefined(jsValue.get())) {
            return ECMAResult::createError("Variable not found: " + name);
        }

        ECMAValue ecmaValue = jsValueToECMAValue(jsValue.get());

        // Add debug logging for events variable result
        if (name == "events") {
            if (std::holds_alternative<std::string>(ecmaValue)) {
                SCXML::Common::Logger::debug("QuickJSEngine::getVariable - Retrieved events = '" +
                                             std::get<std::string>(ecmaValue) + "'");
            } else {
                SCXML::Common::Logger::debug("QuickJSEngine::getVariable - Retrieved events as non-string value");
            }
        }

        return ECMAResult::createSuccess(ecmaValue);

    } catch (const std::exception &e) {
        std::string errorMsg = "QuickJSEngine::getVariable - Exception: " + std::string(e.what());
        return ECMAResult::createError(errorMsg);
    }
}

void QuickJSEngine::setCurrentEvent(const std::shared_ptr<::SCXML::Events::Event> &event) {
    currentEvent_ = event;
    setupEventObject();
}

void QuickJSEngine::setStateCheckFunction(const StateCheckFunction &func) {
    stateCheckFunction_ = func;
}

void QuickJSEngine::setupSCXMLSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                              const std::vector<std::string> &ioProcessors) {
    if (!context_) {
        return;
    }

    try {
        // Set _sessionid
        setVariable("_sessionid", ECMAValue(sessionId));

        // Set _name
        setVariable("_name", ECMAValue(sessionName));

        // Set _ioprocessors (as array of strings)
        JSValueWrapper ioArray(context_, JS_NewArray(context_), "ioprocessors_array");
        for (size_t i = 0; i < ioProcessors.size(); ++i) {
            JSValueWrapper str(context_, JS_NewString(context_, ioProcessors[i].c_str()),
                               "ioprocessor_" + std::to_string(i));
            JS_SetPropertyUint32(context_, ioArray.get(), static_cast<uint32_t>(i), JS_DupValue(context_, str.get()));
        }
        JSValueWrapper global(context_, JS_GetGlobalObject(context_), "ioprocessors_global");
        JS_SetPropertyStr(context_, global.get(), "_ioprocessors", JS_DupValue(context_, ioArray.get()));

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("QuickJSEngine::setupSCXMLSystemVariables - Exception: " + std::string(e.what()));
    }
}

bool QuickJSEngine::registerNativeFunction(const std::string &name, const NativeFunction &func) {
    (void)name;  // Suppress unused parameter warning
    (void)func;  // Suppress unused parameter warning
    // TODO: Implement native function registration
    return false;
}

// Private helper methods

JSValue QuickJSEngine::ecmaValueToJSValue(const ECMAValue &value) {
    return std::visit(
        [this](const auto &v) -> JSValue {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                return JS_UNDEFINED;
            } else if constexpr (std::is_same_v<T, bool>) {
                return JS_NewBool(context_, v);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return JS_NewInt64(context_, v);
            } else if constexpr (std::is_same_v<T, double>) {
                return JS_NewFloat64(context_, v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return JS_NewString(context_, v.c_str());
            } else {
                return JS_UNDEFINED;
            }
        },
        value);
}

IECMAScriptEngine::ECMAValue QuickJSEngine::jsValueToECMAValue(JSValue value) {
    // Check if the value is valid before processing
    if (JS_IsException(value)) {
        return ECMAValue(std::string("[exception]"));
    }

    if (JS_IsBool(value)) {
        return ECMAValue(JS_ToBool(context_, value) != 0);
    } else if (JS_IsNumber(value)) {
        double d;
        JS_ToFloat64(context_, &d, value);

        // Always return double for consistency with other tests and engines
        return ECMAValue(d);
    } else if (JS_IsString(value)) {
        // Get string length first to allocate properly
        size_t len;
        const char *str = JS_ToCStringLen(context_, &len, value);
        if (!str) {
            return ECMAValue(std::string(""));
        }
        // Create string with explicit length to avoid strlen issues
        std::string result(str, len);
        JS_FreeCString(context_, str);
        return ECMAValue(std::move(result));
    } else if (JS_IsNull(value)) {
        return ECMAValue(std::monostate{});
    } else if (JS_IsUndefined(value)) {
        return ECMAValue(std::monostate{});
    } else if (JS_IsObject(value)) {
        // Try to convert object/array to JSON string for better representation
        JSValueWrapper jsonString(context_, JS_JSONStringify(context_, value, JS_UNDEFINED, JS_UNDEFINED),
                                  "json_stringify");
        if (!JS_IsException(jsonString.get()) && JS_IsString(jsonString.get())) {
            size_t len;
            const char *str = JS_ToCStringLen(context_, &len, jsonString.get());
            if (!str) {
                return ECMAValue(std::string("{}"));
            }
            std::string result(str, len);
            JS_FreeCString(context_, str);
            return ECMAValue(result);
        } else {
            // Fallback to toString
            size_t len;
            const char *str = JS_ToCStringLen(context_, &len, value);
            if (!str) {
                return ECMAValue(std::string("[object]"));
            }
            std::string result(str, len);
            JS_FreeCString(context_, str);
            return ECMAValue(result);
        }
    }

    // For any other types, convert to string representation
    size_t len;
    const char *str = JS_ToCStringLen(context_, &len, value);
    if (!str) {
        return ECMAValue(std::string("[unknown]"));
    }
    std::string result(str, len);
    JS_FreeCString(context_, str);
    return ECMAValue(result);
}

void QuickJSEngine::setupSCXMLBuiltins() {
    // Register In() function directly with specialized wrapper
    JSValueWrapper inFunc(context_, JS_NewCFunction(context_, inFunctionWrapper, "In", 1), "in_function");
    JSValueWrapper global(context_, JS_GetGlobalObject(context_), "builtins_global");
    JS_SetPropertyStr(context_, global.get(), "In", JS_DupValue(context_, inFunc.get()));
}

void QuickJSEngine::setupConsoleObject() {
    // Add console.log implementation via JavaScript
    const char *consoleLogCode = R"(
        if (typeof console === 'undefined') {
            console = {};
        }
        console.log = function(...args) {
            // Simple implementation - could be enhanced
            return undefined;
        };
    )";

    // Properly free the result of JS_Eval with RAII
    JSValueWrapper result(context_,
                          JS_Eval(context_, consoleLogCode, strlen(consoleLogCode), "<console>", JS_EVAL_TYPE_GLOBAL),
                          "console_setup");
}

void QuickJSEngine::setupMathObject() {
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
                abs: function(x) { return x < 0 ? -x : x; }
            };
        }
    )";

    JSValueWrapper result(context_, JS_Eval(context_, mathCode, strlen(mathCode), "<math>", JS_EVAL_TYPE_GLOBAL),
                          "math_setup");
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
        JSValueWrapper eventObj(context_, JS_NewObject(context_), "event_object");

        // Set event name
        JSValueWrapper nameValue(context_, JS_NewString(context_, currentEvent_->getName().c_str()), "event_name");
        JS_SetPropertyStr(context_, eventObj.get(), "name", JS_DupValue(context_, nameValue.get()));

        // Set event data properly
        JSValueWrapper dataValue(context_, JS_UNDEFINED, "event_data");
        if (currentEvent_->hasData()) {
            std::string dataStr = currentEvent_->getDataAsString();
            if (!dataStr.empty()) {
                // Try to parse as JSON first
                JSValueWrapper parsed(context_,
                                      JS_ParseJSON(context_, dataStr.c_str(), dataStr.length(), "<event-data>"),
                                      "event_data_parsed");
                if (!JS_IsException(parsed.get())) {
                    // Successfully parsed as JSON object
                    dataValue = std::move(parsed);
                } else {
                    // Clear the exception and use as string
                    JSValueWrapper exception(context_, JS_GetException(context_), "event_data_exception");
                    dataValue = JSValueWrapper(context_, JS_NewString(context_, dataStr.c_str()), "event_data_string");
                }
            } else {
                dataValue = JSValueWrapper(context_, JS_UNDEFINED, "event_data_empty");
            }
        } else {
            dataValue = JSValueWrapper(context_, JS_UNDEFINED, "event_data_none");
        }
        JS_SetPropertyStr(context_, eventObj.get(), "data", JS_DupValue(context_, dataValue.get()));

        // Set event type
        JSValueWrapper typeValue(context_,
                                 JS_NewString(context_, currentEvent_->isExternal()   ? "external"
                                                        : currentEvent_->isInternal() ? "internal"
                                                                                      : "platform"),
                                 "event_type");
        JS_SetPropertyStr(context_, eventObj.get(), "type", JS_DupValue(context_, typeValue.get()));

        // Set as global _event
        JSValueWrapper global(context_, JS_GetGlobalObject(context_), "event_global");
        JS_SetPropertyStr(context_, global.get(), "_event", JS_DupValue(context_, eventObj.get()));

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("QuickJSEngine::setupEventObject - Exception: " + std::string(e.what()));
    }
}

// Static wrapper for In() function
JSValue QuickJSEngine::inFunctionWrapper(JSContext *ctx, JSValueConst this_val [[maybe_unused]], int argc,
                                         JSValueConst *argv) {
    QuickJSEngine *engine = static_cast<QuickJSEngine *>(JS_GetContextOpaque(ctx));
    if (!engine || argc == 0) {
        return JS_NewBool(ctx, false);
    }

    // Convert first argument to string
    const char *stateName = JS_ToCString(ctx, argv[0]);
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

std::string QuickJSEngine::getEngineVersion() const {
    return "2021-03-27";  // QuickJS version
}

size_t QuickJSEngine::getMemoryUsage() const {
    if (!runtime_) {
        return 0;
    }

    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime_, &usage);
    return static_cast<size_t>(usage.malloc_size) + static_cast<size_t>(usage.malloc_count) * sizeof(void *);
}

void QuickJSEngine::collectGarbage() {
    if (runtime_) {
        JS_RunGC(runtime_);
    }
}

}  // namespace SCXML