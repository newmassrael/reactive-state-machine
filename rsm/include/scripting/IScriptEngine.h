#pragma once

#include "JSResult.h"
#include "runtime/SCXMLTypes.h"
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

/**
 * @brief Abstract interface for script execution engines
 *
 * This interface provides abstraction for different JavaScript engines,
 * enabling easy testing with mocks and future extension to other engines.
 * Follows Interface Segregation Principle (ISP).
 */
class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;

    /**
     * @brief Initialize the script engine
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown the script engine
     */
    virtual void shutdown() = 0;

    /**
     * @brief Execute JavaScript script
     * @param script JavaScript code to execute
     * @return Future with execution result
     */
    virtual std::future<JSResult> executeScript(const std::string &script) = 0;

    /**
     * @brief Evaluate JavaScript expression
     * @param expression JavaScript expression to evaluate
     * @return Future with evaluation result
     */
    virtual std::future<JSResult> evaluateExpression(const std::string &expression) = 0;

    /**
     * @brief Set a variable value
     * @param name Variable name
     * @param value Variable value
     * @return Future indicating success/failure
     */
    virtual std::future<JSResult> setVariable(const std::string &name, const ScriptValue &value) = 0;

    /**
     * @brief Get a variable value
     * @param name Variable name
     * @return Future with variable value or error
     */
    virtual std::future<JSResult> getVariable(const std::string &name) = 0;

    /**
     * @brief Get engine information
     * @return Engine name and version
     */
    virtual std::string getEngineInfo() const = 0;

    /**
     * @brief Get current memory usage
     * @return Memory usage in bytes
     */
    virtual size_t getMemoryUsage() const = 0;

    /**
     * @brief Trigger garbage collection
     */
    virtual void collectGarbage() = 0;
};

/**
 * @brief Session-based script engine interface
 *
 * Extends IScriptEngine with session management capabilities.
 */
class ISessionBasedScriptEngine : public IScriptEngine {
public:
    /**
     * @brief Create a new session
     * @param sessionId Unique session identifier
     * @param parentSessionId Optional parent session
     * @return true if session created successfully
     */
    virtual bool createSession(const std::string &sessionId, const std::string &parentSessionId = "") = 0;

    /**
     * @brief Destroy a session
     * @param sessionId Session to destroy
     * @return true if session destroyed successfully
     */
    virtual bool destroySession(const std::string &sessionId) = 0;

    /**
     * @brief Check if session exists
     * @param sessionId Session to check
     * @return true if session exists
     */
    virtual bool hasSession(const std::string &sessionId) = 0;

    /**
     * @brief Get all active sessions
     * @return Vector of session IDs
     */
    virtual std::vector<std::string> getActiveSessions() const = 0;

    // Session-specific operations
    virtual std::future<JSResult> executeScript(const std::string &sessionId, const std::string &script) = 0;
    virtual std::future<JSResult> evaluateExpression(const std::string &sessionId, const std::string &expression) = 0;
    virtual std::future<JSResult> setVariable(const std::string &sessionId, const std::string &name,
                                              const ScriptValue &value) = 0;
    virtual std::future<JSResult> getVariable(const std::string &sessionId, const std::string &name) = 0;
};

/**
 * @brief Mock implementation for testing
 */
class MockScriptEngine : public ISessionBasedScriptEngine {
private:
    bool initialized_ = false;
    std::map<std::string, bool> sessions_;
    std::map<std::string, std::map<std::string, ScriptValue>> sessionVariables_;
    std::map<std::string, JSResult> predefinedResults_;

public:
    // IScriptEngine implementation
    bool initialize() override {
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        initialized_ = false;
        sessions_.clear();
        sessionVariables_.clear();
    }

    std::future<JSResult> executeScript(const std::string &script) override {
        return executeScript("default", script);
    }

    std::future<JSResult> evaluateExpression(const std::string &expression) override {
        return evaluateExpression("default", expression);
    }

    std::future<JSResult> setVariable(const std::string &name, const ScriptValue &value) override {
        return setVariable("default", name, value);
    }

    std::future<JSResult> getVariable(const std::string &name) override {
        return getVariable("default", name);
    }

    std::string getEngineInfo() const override {
        return "MockScriptEngine v1.0 for Testing";
    }

    size_t getMemoryUsage() const override {
        return 1024;  // Mock memory usage
    }

    void collectGarbage() override {
        // Mock garbage collection
    }

    // ISessionBasedScriptEngine implementation
    bool createSession(const std::string &sessionId, const std::string &parentSessionId = "") override {
        (void)parentSessionId;  // Unused parameter - inheritance not implemented in mock
        if (!initialized_) {
            return false;
        }
        sessions_[sessionId] = true;
        sessionVariables_[sessionId] = {};
        return true;
    }

    bool destroySession(const std::string &sessionId) override {
        sessions_.erase(sessionId);
        sessionVariables_.erase(sessionId);
        return true;
    }

    bool hasSession(const std::string &sessionId) override {
        return sessions_.find(sessionId) != sessions_.end();
    }

    std::vector<std::string> getActiveSessions() const override {
        std::vector<std::string> result;
        for (const auto &[sessionId, _] : sessions_) {
            result.push_back(sessionId);
        }
        return result;
    }

    std::future<JSResult> executeScript(const std::string &sessionId, const std::string &script) override {
        std::promise<JSResult> promise;
        auto future = promise.get_future();

        if (!hasSession(sessionId)) {
            promise.set_value(JSResult::createError("Session not found: " + sessionId));
        } else {
            auto it = predefinedResults_.find(script);
            if (it != predefinedResults_.end()) {
                promise.set_value(it->second);
            } else {
                promise.set_value(JSResult::createSuccess());
            }
        }

        return future;
    }

    std::future<JSResult> evaluateExpression(const std::string &sessionId, const std::string &expression) override {
        std::promise<JSResult> promise;
        auto future = promise.get_future();

        if (!hasSession(sessionId)) {
            promise.set_value(JSResult::createError("Session not found: " + sessionId));
        } else {
            auto it = predefinedResults_.find(expression);
            if (it != predefinedResults_.end()) {
                promise.set_value(it->second);
            } else {
                // Default to true for conditions
                promise.set_value(JSResult::createSuccess(ScriptValue{true}));
            }
        }

        return future;
    }

    std::future<JSResult> setVariable(const std::string &sessionId, const std::string &name,
                                      const ScriptValue &value) override {
        std::promise<JSResult> promise;
        auto future = promise.get_future();

        if (!hasSession(sessionId)) {
            promise.set_value(JSResult::createError("Session not found: " + sessionId));
        } else {
            sessionVariables_[sessionId][name] = value;
            promise.set_value(JSResult::createSuccess());
        }

        return future;
    }

    std::future<JSResult> getVariable(const std::string &sessionId, const std::string &name) override {
        std::promise<JSResult> promise;
        auto future = promise.get_future();

        if (!hasSession(sessionId)) {
            promise.set_value(JSResult::createError("Session not found: " + sessionId));
        } else {
            auto sessionIt = sessionVariables_.find(sessionId);
            if (sessionIt != sessionVariables_.end()) {
                auto varIt = sessionIt->second.find(name);
                if (varIt != sessionIt->second.end()) {
                    promise.set_value(JSResult::createSuccess(varIt->second));
                } else {
                    promise.set_value(JSResult::createError("Variable not found: " + name));
                }
            } else {
                promise.set_value(JSResult::createError("Session variables not found"));
            }
        }

        return future;
    }

    // Test utilities
    void setPredefinedResult(const std::string &scriptOrExpression, const JSResult &result) {
        predefinedResults_[scriptOrExpression] = result;
    }

    void clearPredefinedResults() {
        predefinedResults_.clear();
    }
};

}  // namespace RSM