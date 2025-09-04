#pragma once

#include "common/Result.h"
#include "runtime/DataModelEngine.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
namespace Runtime {
class RuntimeContext;

/**
 * @brief Enhanced expression evaluator with comprehensive SCXML expression support
 *
 * Provides advanced expression evaluation capabilities including:
 * - Complex conditional expressions
 * - Event data access (_event.*)
 * - System variables (_sessionid, _name, _ioprocessors, _x)
 * - Mathematical operations
 * - String manipulation
 * - Type conversion and validation
 */

class EnhancedExpressionEvaluator {
public:
    enum class ExpressionType {
        BOOLEAN,          ///< Boolean condition
        ARITHMETIC,       ///< Mathematical expression
        STRING,           ///< String expression
        LOCATION,         ///< Data model location
        EVENT_DATA,       ///< Event data access
        SYSTEM_VARIABLE,  ///< System variable
        FUNCTION_CALL,    ///< Function invocation
        COMPLEX           ///< Complex multi-type expression
    };

    struct ExpressionContext {
        std::map<std::string, std::string> eventData;   ///< Current event data
        std::string sessionId;                          ///< Session identifier
        std::string stateMachineName;                   ///< State machine name
        std::vector<std::string> ioProcessors;          ///< Available I/O processors
        std::map<std::string, std::string> systemVars;  ///< System variables
    };

    /**
     * @brief Constructor
     */
    EnhancedExpressionEvaluator();

    /**
     * @brief Destructor
     */
    ~EnhancedExpressionEvaluator() = default;

    /**
     * @brief Evaluate a boolean condition expression
     * @param context Runtime context
     * @param expression Boolean expression
     * @param exprContext Expression evaluation context
     * @return Result containing boolean value
     */
    SCXML::Common::Result<bool> evaluateBooleanExpression(SCXML::Runtime::RuntimeContext &context,
                                                          const std::string &expression,
                                                          const ExpressionContext &exprContext = {});

    /**
     * @brief Evaluate an arithmetic expression
     * @param context Runtime context
     * @param expression Arithmetic expression
     * @param exprContext Expression evaluation context
     * @return Result containing numeric value
     */
    SCXML::Common::Result<double> evaluateArithmeticExpression(SCXML::Runtime::RuntimeContext &context,
                                                               const std::string &expression,
                                                               const ExpressionContext &exprContext = {});

    /**
     * @brief Evaluate a string expression
     * @param context Runtime context
     * @param expression String expression
     * @param exprContext Expression evaluation context
     * @return Result containing string value
     */
    SCXML::Common::Result<std::string> evaluateStringExpression(SCXML::Runtime::RuntimeContext &context,
                                                                const std::string &expression,
                                                                const ExpressionContext &exprContext = {});

    /**
     * @brief Evaluate any expression and return as string
     * @param context Runtime context
     * @param expression Expression to evaluate
     * @param exprContext Expression evaluation context
     * @return Result containing string representation
     */
    SCXML::Common::Result<std::string> evaluateExpression(SCXML::Runtime::RuntimeContext &context,
                                                          const std::string &expression,
                                                          const ExpressionContext &exprContext = {});

    /**
     * @brief Determine the type of an expression
     * @param expression Expression to analyze
     * @return Detected expression type
     */
    ExpressionType detectExpressionType(const std::string &expression);

    /**
     * @brief Validate expression syntax
     * @param expression Expression to validate
     * @return Validation errors (empty if valid)
     */
    std::vector<std::string> validateExpression(const std::string &expression);

    /**
     * @brief Set up expression context from current runtime state
     * @param context Runtime context
     * @return Expression context for evaluation
     */
    ExpressionContext createExpressionContext(SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Check if expression references event data
     * @param expression Expression to check
     * @return true if expression uses _event
     */
    bool usesEventData(const std::string &expression);

    /**
     * @brief Check if expression references system variables
     * @param expression Expression to check
     * @return true if expression uses system variables
     */
    bool usesSystemVariables(const std::string &expression);

    /**
     * @brief Preprocess expression to handle SCXML-specific syntax
     * @param expression Raw expression
     * @param exprContext Expression context
     * @return Preprocessed expression ready for evaluation
     */
    std::string preprocessExpression(const std::string &expression, const ExpressionContext &exprContext);

private:
    /**
     * @brief Evaluate event data access expression (_event.*)
     * @param expression Event data expression
     * @param exprContext Expression context
     * @return Result containing event data value
     */
    SCXML::Common::Result<std::string> evaluateEventDataExpression(const std::string &expression,
                                                                   const ExpressionContext &exprContext);

    /**
     * @brief Evaluate system variable access
     * @param expression System variable expression
     * @param exprContext Expression context
     * @return Result containing system variable value
     */
    SCXML::Common::Result<std::string> evaluateSystemVariable(const std::string &expression,
                                                              const ExpressionContext &exprContext);

    /**
     * @brief Replace event data placeholders in expression
     * @param expression Expression with _event references
     * @param exprContext Expression context
     * @return Expression with substituted values
     */
    std::string substituteEventData(const std::string &expression, const ExpressionContext &exprContext);

    /**
     * @brief Replace system variable placeholders
     * @param expression Expression with system variable references
     * @param exprContext Expression context
     * @return Expression with substituted values
     */
    std::string substituteSystemVariables(const std::string &expression, const ExpressionContext &exprContext);

    /**
     * @brief Parse and validate function calls in expressions
     * @param expression Expression potentially containing function calls
     * @return Validation results
     */
    std::vector<std::string> validateFunctionCalls(const std::string &expression);

    /**
     * @brief Check if expression contains balanced parentheses/brackets
     * @param expression Expression to check
     * @return true if balanced
     */
    bool hasBalancedDelimiters(const std::string &expression);

    /**
     * @brief Convert value to boolean according to SCXML rules
     * @param value String value
     * @return Boolean conversion result
     */
    bool convertToBoolean(const std::string &value);

    /**
     * @brief Escape string for safe JavaScript evaluation
     * @param str String to escape
     * @return Escaped string
     */
    std::string escapeString(const std::string &str);
};

}  // namespace Runtime
}  // namespace SCXML