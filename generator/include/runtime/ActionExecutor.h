#pragma once

#include <memory>
#include <string>

#include "runtime/RuntimeContext.h"

namespace SCXML {

// Forward declarations
namespace Core {
class ActionNode;
}

namespace Runtime {

/**
 * @brief Abstract base class for Action Executors
 *
 * This class implements the Strategy pattern to separate action execution logic
 * from action data representation. Each concrete executor handles the execution
 * of a specific action type.
 */
class ActionExecutor {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~ActionExecutor() = default;

    /**
     * @brief Execute an action using the provided runtime context
     * @param actionNode The action node containing configuration data
     * @param context Runtime context for execution
     * @return true if action executed successfully
     */
    virtual bool execute(const Core::ActionNode& actionNode, RuntimeContext& context) = 0;

    /**
     * @brief Get the action type that this executor handles
     * @return Action type string (e.g., "assign", "log", "send")
     */
    virtual std::string getActionType() const = 0;

    /**
     * @brief Validate action configuration before execution
     * @param actionNode The action node to validate
     * @return Vector of validation error messages (empty if valid)
     */
    virtual std::vector<std::string> validate(const Core::ActionNode& actionNode) const {
        (void)actionNode; // Suppress unused parameter warning
        return {}; // Default: no validation errors
    }

protected:
    /**
     * @brief Helper to safely cast ActionNode to specific type
     * @tparam T Target action node type
     * @param actionNode Base action node to cast
     * @return Pointer to cast result, nullptr if cast fails
     */
    template<typename T>
    const T* safeCast(const Core::ActionNode& actionNode) const {
        return dynamic_cast<const T*>(&actionNode);
    }

    /**
     * @brief Log execution error with context information
     * @param actionType Type of action that failed
     * @param errorMessage Error description
     * @param context Runtime context for logging
     */
    void logExecutionError(const std::string& actionType, 
                          const std::string& errorMessage, 
                          RuntimeContext& context) const;
};

/**
 * @brief Factory interface for creating Action Executors
 */
class ActionExecutorFactory {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~ActionExecutorFactory() = default;

    /**
     * @brief Create executor for the specified action type
     * @param actionType Action type string (e.g., "assign", "log", "send")
     * @return Shared pointer to executor, nullptr if type not supported
     */
    virtual std::shared_ptr<ActionExecutor> createExecutor(const std::string& actionType) = 0;

    /**
     * @brief Check if action type is supported
     * @param actionType Action type to check
     * @return true if executor exists for this type
     */
    virtual bool supportsActionType(const std::string& actionType) const = 0;

    /**
     * @brief Get list of all supported action types
     * @return Vector of supported action type strings
     */
    virtual std::vector<std::string> getSupportedActionTypes() const = 0;
};

/**
 * @brief Default implementation of ActionExecutorFactory
 */
class DefaultActionExecutorFactory : public ActionExecutorFactory {
public:
    /**
     * @brief Constructor - registers all built-in executors
     */
    DefaultActionExecutorFactory();

    /**
     * @brief Create executor for the specified action type
     */
    std::shared_ptr<ActionExecutor> createExecutor(const std::string& actionType) override;

    /**
     * @brief Check if action type is supported
     */
    bool supportsActionType(const std::string& actionType) const override;

    /**
     * @brief Get list of all supported action types
     */
    std::vector<std::string> getSupportedActionTypes() const override;

    /**
     * @brief Register a custom executor
     * @param actionType Action type string
     * @param creator Function that creates executor instances
     */
    void registerExecutor(const std::string& actionType, 
                         std::function<std::shared_ptr<ActionExecutor>()> creator);

private:
    std::unordered_map<std::string, std::function<std::shared_ptr<ActionExecutor>()>> executorCreators_;
    
    void registerBuiltInExecutors();
};

} // namespace Runtime
} // namespace SCXML