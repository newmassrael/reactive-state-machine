#pragma once

#include "../SCXMLTypes.h"
#include "JSResult.h"
#include <future>
#include <map>
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

}  // namespace RSM