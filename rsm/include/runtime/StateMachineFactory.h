#pragma once

#include "StateMachine.h"
#include "scripting/IScriptEngine.h"
#include <memory>
#include <string>
#include <variant>

namespace RSM {

/**
 * @brief Factory for creating StateMachine instances with proper dependency injection
 *
 * This factory follows the SOLID principles:
 * - SRP: Only responsible for creating StateMachine instances
 * - OCP: Open for extension with new creation methods
 * - DIP: Depends on abstractions (IScriptEngine) not concretions
 */
class StateMachineFactory {
public:
    /**
     * @brief Result type for factory operations
     */
    struct CreationResult {
        std::unique_ptr<StateMachine> value;
        std::string error;
        bool success;

        CreationResult(std::unique_ptr<StateMachine> sm) : value(std::move(sm)), success(true) {}

        CreationResult(const std::string &err) : error(err), success(false) {}

        bool has_value() const {
            return success;
        }

        explicit operator bool() const {
            return success;
        }
    };

    /**
     * @brief Create StateMachine for production use
     * @return StateMachine instance or error message
     */
    static CreationResult createProduction();

    /**
     * @brief Create StateMachine for testing with mocks
     * @return StateMachine instance with mock dependencies
     */
    static CreationResult createForTesting();

    /**
     * @brief Create StateMachine with custom script engine
     * @param scriptEngine Custom script engine implementation
     * @return StateMachine instance or error message
     */
    static CreationResult createWithScriptEngine(std::shared_ptr<ISessionBasedScriptEngine> scriptEngine);

    /**
     * @brief Create StateMachine with SCXML content
     * @param scxmlContent SCXML document content
     * @param useProductionEngine Use production or mock engine
     * @return Fully configured StateMachine or error message
     */
    static CreationResult createWithSCXML(const std::string &scxmlContent, bool useProductionEngine = true);

    /**
     * @brief Builder pattern for complex configurations
     */
    class Builder {
    private:
        std::shared_ptr<ISessionBasedScriptEngine> scriptEngine_;
        std::string scxmlContent_;
        bool autoInitialize_ = true;

    public:
        Builder &withScriptEngine(std::shared_ptr<ISessionBasedScriptEngine> engine) {
            scriptEngine_ = engine;
            return *this;
        }

        Builder &withSCXML(const std::string &content) {
            scxmlContent_ = content;
            return *this;
        }

        Builder &withAutoInitialize(bool autoInit) {
            autoInitialize_ = autoInit;
            return *this;
        }

        /**
         * @brief Build the StateMachine with specified configuration
         * @return StateMachine instance or error message
         */
        CreationResult build();
    };

    /**
     * @brief Get a builder instance
     * @return Builder for fluent configuration
     */
    static Builder builder() {
        return Builder{};
    }

private:
    /**
     * @brief Create StateMachine with dependencies - internal method
     * @param scriptEngine Script engine to use
     * @param scxmlContent Optional SCXML content
     * @param autoInitialize Whether to initialize automatically
     * @return StateMachine instance or error message
     */
    static CreationResult createInternal(std::shared_ptr<ISessionBasedScriptEngine> scriptEngine,
                                         const std::string &scxmlContent = "", bool autoInitialize = true);
};

/**
 * @brief Adapter to make existing JSEngine work with new interface
 *
 * This adapter allows gradual migration from old JSEngine to new interface.
 * Can be removed once full migration is complete.
 */
class JSEngineAdapter : public ISessionBasedScriptEngine {
private:
    std::string defaultSessionId_;
    bool initialized_ = false;

public:
    JSEngineAdapter();
    ~JSEngineAdapter() override;

    // IScriptEngine implementation
    bool initialize() override;
    void shutdown() override;
    std::future<JSResult> executeScript(const std::string &script) override;
    std::future<JSResult> evaluateExpression(const std::string &expression) override;
    std::future<JSResult> setVariable(const std::string &name, const ScriptValue &value) override;
    std::future<JSResult> getVariable(const std::string &name) override;
    std::string getEngineInfo() const override;
    size_t getMemoryUsage() const override;
    void collectGarbage() override;

    // ISessionBasedScriptEngine implementation
    bool createSession(const std::string &sessionId, const std::string &parentSessionId = "") override;
    bool destroySession(const std::string &sessionId) override;
    bool hasSession(const std::string &sessionId) override;
    std::vector<std::string> getActiveSessions() const override;
    std::future<JSResult> executeScript(const std::string &sessionId, const std::string &script) override;
    std::future<JSResult> evaluateExpression(const std::string &sessionId, const std::string &expression) override;
    std::future<JSResult> setVariable(const std::string &sessionId, const std::string &name,
                                      const ScriptValue &value) override;
    std::future<JSResult> getVariable(const std::string &sessionId, const std::string &name) override;
};

}  // namespace RSM