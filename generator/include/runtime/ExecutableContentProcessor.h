#pragma once

#include "common/Result.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// Forward declarations
namespace SCXML {

namespace Model {
class IActionNode;
}
// Forward declarations
namespace Runtime {
class RuntimeContext;
}

namespace Core {
class IActionNode;
}

namespace Events {
class Event;
}

/**
 * @brief SCXML Executable Content Processor
 *
 * This class implements the execution engine for SCXML executable content
 * according to the W3C SCXML specification. It handles:
 * - Basic actions: raise, log, assign, send, cancel
 * - Control flow: if/elseif/else, foreach
 * - Script execution
 * - Expression evaluation integration
 */
// Forward declarations for implementation classes
class ExpressionEvaluatorImpl;
class ScriptEngineImpl;

class ExecutableContentProcessor {
public:
    /**
     * @brief Execution result for executable content
     */
    struct ExecutionResult {
        bool success;                           // Whether execution succeeded
        std::string errorMessage;               // Error message if execution failed
        std::vector<std::string> raisedEvents;  // Events raised during execution
        std::vector<std::string> logMessages;   // Log messages generated
        bool shouldTerminate;                   // Whether execution should terminate

        ExecutionResult() : success(false), shouldTerminate(false) {}
    };

    /**
     * @brief Value type for expression results and data model
     */
    using Value = std::variant<std::monostate, bool, int64_t, double, std::string>;

    /**
     * @brief Data model context
     */
    using DataModel = std::unordered_map<std::string, Value>;

    /**
     * @brief Construct a new Executable Content Processor
     */
    ExecutableContentProcessor();

    /**
     * @brief Destructor
     */
    ~ExecutableContentProcessor();

    /**
     * @brief Execute a list of action nodes
     * @param actions List of action nodes to execute
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeActions(const std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &actions,
                                   SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute a single action node
     * @param action Action node to execute
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeAction(std::shared_ptr<SCXML::Model::IActionNode> action,
                                  SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute raise action
     * @param eventName Name of event to raise
     * @param eventData Optional event data
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeRaise(const std::string &eventName, const std::string &eventData,
                                 SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute if/elseif/else conditional action
     * @param ifAction Conditional action to execute
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeIf(std::shared_ptr<class IfActionNode> ifAction, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute log action
     * @param expression Expression to evaluate and log
     * @param label Optional log label
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeLog(const std::string &expression, const std::string &label,
                               SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute assign action
     * @param location Data model location to assign to
     * @param expression Expression to evaluate and assign
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeAssign(const std::string &location, const std::string &expression,
                                  SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute send action
     * @param eventName Event name to send
     * @param target Target for the event
     * @param delay Optional delay
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeSend(const std::string &eventName, const std::string &target, const std::string &delay,
                                SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute cancel action
     * @param sendId ID of send to cancel
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeCancel(const std::string &sendId, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute if/elseif/else conditional
     * @param action Conditional action node
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeConditional(std::shared_ptr<SCXML::Model::IActionNode> action,
                                       SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute foreach loop
     * @param action Foreach action node
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeForeach(std::shared_ptr<SCXML::Model::IActionNode> action,
                                   SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute script
     * @param scriptContent Script content to execute
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeScript(const std::string &scriptContent, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Evaluate expression in current context
     * @param expression Expression to evaluate
     * @param context Runtime context
     * @return Evaluated value
     */
    Value evaluateExpression(const std::string &expression, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if expression evaluates to true
     * @param condition Condition expression
     * @param context Runtime context
     * @return Whether condition is true
     */
    bool evaluateCondition(const std::string &condition, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get data model value
     * @param location Data model location
     * @param context Runtime context
     * @return Data model value
     */
    Value getDataModelValue(const std::string &location, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Set data model value
     * @param location Data model location
     * @param value Value to set
     * @param context Runtime context
     * @return Whether assignment succeeded
     */
    bool setDataModelValue(const std::string &location, const Value &value, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Convert value to string representation
     * @param value Value to convert
     * @return String representation
     */
    std::string valueToString(const Value &value) const;

    /**
     * @brief Convert value to boolean
     * @param value Value to convert
     * @return Boolean representation
     */
    bool valueToBool(const Value &value) const;

    /**
     * @brief Parse array expression for foreach
     * @param arrayExpr Array expression
     * @param context Runtime context
     * @return Vector of values
     */
    std::vector<Value> parseArrayExpression(const std::string &arrayExpr, SCXML::Runtime::RuntimeContext &context);

protected:
    /**
     * @brief Execute child actions of a parent action
     * @param parentAction Parent action containing child actions
     * @param context Runtime context
     * @return Execution result
     */
    ExecutionResult executeChildActions(std::shared_ptr<SCXML::Model::IActionNode> parentAction,
                                        SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Merge execution results
     * @param result1 First result
     * @param result2 Second result
     * @return Merged result
     */
    ExecutionResult mergeResults(const ExecutionResult &result1, const ExecutionResult &result2);

    /**
     * @brief Create error result
     * @param errorMessage Error message
     * @return Error result
     */
    ExecutionResult createErrorResult(const std::string &errorMessage);

    /**
     * @brief Create success result
     * @return Success result
     */
    ExecutionResult createSuccessResult();

private:
    // Expression evaluator for conditions and assignments
    std::unique_ptr<class ExpressionEvaluatorImpl> expressionEvaluator_;

    // Script execution engine
    std::unique_ptr<class ScriptEngineImpl> scriptEngine_;

    // Action type handlers
    std::unordered_map<std::string, std::function<ExecutionResult(std::shared_ptr<SCXML::Model::IActionNode>,
                                                                  SCXML::Runtime::RuntimeContext &)>>
        actionHandlers_;

    /**
     * @brief Initialize action handlers
     */
    void initializeActionHandlers();

    /**
     * @brief Get action attribute value
     * @param action Action node
     * @param attributeName Attribute name
     * @param defaultValue Default value if attribute not found
     * @return Attribute value
     */
    std::string getActionAttribute(std::shared_ptr<SCXML::Model::IActionNode> action, const std::string &attributeName,
                                   const std::string &defaultValue = "") const;

    /**
     * @brief Log execution error
     * @param message Error message
     * @param actionId Action ID (optional)
     */
    void logError(const std::string &message, const std::string &actionId = "") const;

    /**
     * @brief Log execution info
     * @param message Info message
     * @param actionId Action ID (optional)
     */
    void logInfo(const std::string &message, const std::string &actionId = "") const;

    /**
     * @brief Load script content from file
     * @param srcPath Path to script file
     * @param context Runtime context for path resolution
     * @return Script content or empty string if failed
     */
    std::string loadScriptFromFile(const std::string &srcPath, SCXML::Runtime::RuntimeContext &context) const;
};

}  // namespace SCXML