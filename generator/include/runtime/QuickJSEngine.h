#pragma once

#include "runtime/IECMAScriptEngine.h"

// Suppress QuickJS warnings (3rd party library)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"  
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "quickjs.h"
#pragma GCC diagnostic pop

#include <memory>
#include <unordered_map>
#include <string>

namespace SCXML {

/**
 * @brief Complete JavaScript engine using QuickJS
 * 
 * This provides full ECMAScript support for SCXML state machines,
 * replacing the limited fallback JavaScriptEvaluatorAdapter.
 */
class QuickJSEngine : public IECMAScriptEngine {
public:
    QuickJSEngine();
    virtual ~QuickJSEngine();

    // IECMAScriptEngine interface
    bool initialize() override;
    void shutdown() override;
    
    ECMAResult evaluateExpression(const std::string &expression, 
                                 ::SCXML::Runtime::RuntimeContext &context) override;
    
    ECMAResult executeScript(const std::string &script, 
                           ::SCXML::Runtime::RuntimeContext &context) override;
    
    bool setVariable(const std::string &name, const ECMAValue &value) override;
    ECMAResult getVariable(const std::string &name) override;
    
    // SCXML-specific features
    void setCurrentEvent(const std::shared_ptr<::SCXML::Events::Event> &event) override;
    void setStateCheckFunction(const StateCheckFunction &func) override;
    void setupSCXMLSystemVariables(const std::string &sessionId, 
                                  const std::string &sessionName,
                                  const std::vector<std::string> &ioProcessors) override;
    
    bool registerNativeFunction(const std::string &name, const NativeFunction &func) override;
    
    // Engine information
    std::string getEngineName() const override { return "QuickJS"; }
    std::string getEngineVersion() const override;
    size_t getMemoryUsage() const override;
    void collectGarbage() override;

private:
    JSRuntime* runtime_;
    JSContext* context_;
    
    StateCheckFunction stateCheckFunction_;
    std::shared_ptr<::SCXML::Events::Event> currentEvent_;
    std::unordered_map<std::string, NativeFunction> nativeFunctions_;
    
    // QuickJS helper methods
    JSValue ecmaValueToJSValue(const ECMAValue &value);
    ECMAValue jsValueToECMAValue(JSValue value);
    
    void setupSCXMLBuiltins();
    void setupConsoleObject();
    void setupMathObject();
    void setupJSONObject();
    void setupEventObject();
    
    // Error handling
    ECMAResult createErrorFromException();
    void throwRuntimeError(const std::string &message);
    
    // Native function wrappers
    static JSValue inFunctionWrapper(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv);
    static JSValue consoleFunctionWrapper(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv);
    static JSValue nativeFunctionWrapper(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv);
};

} // namespace SCXML