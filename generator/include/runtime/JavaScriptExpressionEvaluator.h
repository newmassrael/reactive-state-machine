#pragma once

#include "runtime/ExpressionEvaluator.h"
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// Forward declarations
#include "runtime/DataModelEngine.h"

namespace SCXML {
// Forward declarations
namespace Runtime {
class RuntimeContext;
}

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

/**
 * @brief Enhanced JavaScript-compatible Expression Evaluator for SCXML
 *
 * This class provides comprehensive JavaScript expression evaluation capability
 * for SCXML guard conditions and data model expressions. It supports:
 * - Boolean expressions with logical operators (&&, ||, !)
 * - Comparison operators (==, !=, <, >, <=, >=, ===, !==)
 * - Arithmetic operators (+, -, *, /, %, **)
 * - String operations and concatenation
 * - Data model variable access
 * - Event data access (_event.data.*)
 * - Function calls (built-in functions)
 * - Conditional operator (ternary: condition ? true : false)
 */
class JavaScriptExpressionEvaluator : public SCXML::Runtime::ExpressionEvaluator {
public:
    /**
     * @brief Value type for JavaScript variables
     */
    using JSValue = std::variant<bool,           // Boolean values
                                 double,         // Numeric values
                                 std::string,    // String values
                                 std::nullptr_t  // null/undefined
                                 >;

    /**
     * @brief JavaScript evaluation context with enhanced features
     */
    struct JSEvaluationContext : public SCXML::Runtime::ExpressionEvaluator::EventContext {
        SCXML::Runtime::RuntimeContext *runtimeContext = nullptr;  // Runtime context for data model access
        SCXML::Events::EventPtr currentEvent;                      // Current triggering event
        std::unordered_map<std::string, JSValue> jsVariables;      // JavaScript variables
        std::string sourceStateId;                                 // Source state for transitions
        std::string targetStateId;                                 // Target state for transitions

        JSEvaluationContext() = default;

        JSEvaluationContext(const SCXML::Runtime::ExpressionEvaluator::EventContext &base)
            : SCXML::Runtime::ExpressionEvaluator::EventContext(base) {}
    };

    /**
     * @brief Construct JavaScript Expression Evaluator
     */
    JavaScriptExpressionEvaluator();

    /**
     * @brief Destructor
     */
    ~JavaScriptExpressionEvaluator() = default;

    // ========== ExpressionEvaluator Interface ==========

    /**
     * @brief Evaluate expression as boolean
     * @param expression JavaScript expression string
     * @param context Evaluation context
     * @return Boolean evaluation result
     */
    BooleanResult evaluateBoolean(const std::string &expression, const EventContext &context);

    /**
     * @brief Evaluate expression as string
     * @param expression JavaScript expression string
     * @param context Evaluation context
     * @return String evaluation result
     */
    StringResult evaluateString(const std::string &expression, const EventContext &context);

    /**
     * @brief Evaluate expression as number
     * @param expression JavaScript expression string
     * @param context Evaluation context
     * @return Number evaluation result
     */
    EvaluationResult evaluateNumber(const std::string &expression, const DataModel &, const EventContext *context);

    /**
     * @brief Check if expression is syntactically valid
     * @param expression Expression to validate
     * @return True if valid syntax
     */
    bool isValidExpression(const std::string &expression) const;

    // ========== JavaScript-specific Methods ==========

    /**
     * @brief Evaluate expression with JavaScript context
     * @param expression JavaScript expression
     * @param context JS evaluation context
     * @return JSValue result
     */
    JSValue evaluateJSExpression(const std::string &expression, const JSEvaluationContext &context);

    /**
     * @brief Set JavaScript variable
     * @param name Variable name
     * @param value Variable value
     */
    void setJSVariable(const std::string &name, const JSValue &value);

    /**
     * @brief Get JavaScript variable
     * @param name Variable name
     * @return Variable value or null if not found
     */
    JSValue getJSVariable(const std::string &name) const;

    /**
     * @brief Clear all JavaScript variables
     */
    void clearJSVariables();

    /**
     * @brief Convert JSValue to boolean following JavaScript rules
     * @param value Value to convert
     * @return Boolean conversion
     */
    static bool jsValueToBoolean(const JSValue &value);

    /**
     * @brief Convert JSValue to string following JavaScript rules
     * @param value Value to convert
     * @return String conversion
     */
    static std::string jsValueToString(const JSValue &value);

    /**
     * @brief Convert JSValue to number following JavaScript rules
     * @param value Value to convert
     * @return Number conversion (NaN represented as quiet_NaN)
     */
    static double jsValueToNumber(const JSValue &value);

protected:
    // ========== Expression Parsing Methods ==========

    /**
     * @brief Parse and evaluate conditional (ternary) expression
     * @param expression Expression string
     * @param context JS context
     * @return Evaluation result
     */
    JSValue evaluateConditional(const std::string &expression, const JSEvaluationContext &context);

    /**
     * @brief Parse and evaluate logical OR expression
     * @param expression Expression string
     * @param context JS context
     * @return Evaluation result
     */
    JSValue evaluateLogicalOr(const std::string &expression, const JSEvaluationContext &context);

    /**
     * @brief Parse and evaluate logical AND expression
     * @param expression Expression string
     * @param context JS context
     * @return Evaluation result
     */
    JSValue evaluateLogicalAnd(const std::string &expression, const JSEvaluationContext &context);

    /**
     * @brief Parse and evaluate equality expression
     * @param expression Expression string
     * @param context JS context
     * @return Evaluation result
     */
    JSValue evaluateEquality(const std::string &expression, const JSEvaluationContext &context);

    /**
     * @brief Parse and evaluate relational expression
     * @param expression Expression string
     * @param context JS context
     * @return Evaluation result
     */
    JSValue evaluateRelational(const std::string &expression, const JSEvaluationContext &context);

    /**
     * @brief Parse and evaluate additive expression
     * @param expression Expression string
     * @param context JS context
     * @return Evaluation result
     */
    JSValue evaluateAdditive(const std::string &expression, const JSEvaluationContext &context);

    /**
     * @brief Parse and evaluate multiplicative expression
     * @param expression Expression string
     * @param context JS context
     * @return Evaluation result
     */
    JSValue evaluateMultiplicative(const std::string &expression, const JSEvaluationContext &context);

    /**
     * @brief Parse and evaluate unary expression
     * @param expression Expression string
     * @param context JS context
     * @return Evaluation result
     */
    JSValue evaluateUnary(const std::string &expression, const JSEvaluationContext &context);

    /**
     * @brief Parse and evaluate primary expression (literals, variables, functions)
     * @param expression Expression string
     * @param context JS context
     * @return Evaluation result
     */
    JSValue evaluatePrimary(const std::string &expression, const JSEvaluationContext &context);

    // ========== Data Model Access Methods ==========

    /**
     * @brief Access data model variable
     * @param variableName Variable name (can be dot-notation)
     * @param context JS context
     * @return Variable value or null if not found
     */
    JSValue accessDataModelVariable(const std::string &variableName, const JSEvaluationContext &context);

    /**
     * @brief Access event data using _event notation
     * @param eventPath Event data path (e.g., "_event.data.value")
     * @param context JS context
     * @return Event data value or null if not found
     */
    JSValue accessEventData(const std::string &eventPath, const JSEvaluationContext &context);

    /**
     * @brief Check if identifier is an event data reference
     * @param identifier Identifier to check
     * @return True if starts with _event
     */
    static bool isEventDataReference(const std::string &identifier);

    // ========== Built-in Function Support ==========

    /**
     * @brief Call built-in JavaScript function
     * @param functionName Function name
     * @param args Function arguments
     * @param context JS context
     * @return Function result
     */
    JSValue callBuiltinFunction(const std::string &functionName, const std::vector<JSValue> &args,
                                const JSEvaluationContext &context);

    /**
     * @brief Register built-in functions
     */
    void initializeBuiltinFunctions();

    // ========== Utility Methods ==========

    /**
     * @brief Trim whitespace from expression
     * @param expression Expression to trim
     * @return Trimmed expression
     */
    static std::string trimExpression(const std::string &expression);

    /**
     * @brief Split expression by operator while respecting parentheses
     * @param expression Expression to split
     * @param operators Operators to split on (in precedence order)
     * @return Vector of expression parts and operators
     */
    static std::vector<std::string> splitExpression(const std::string &expression,
                                                    const std::vector<std::string> &operators);

    /**
     * @brief Find matching parenthesis
     * @param expression Expression string
     * @param startPos Position of opening parenthesis
     * @return Position of matching closing parenthesis
     */
    static size_t findMatchingParenthesis(const std::string &expression, size_t startPos);

    /**
     * @brief Parse string literal
     * @param literal String literal with quotes
     * @return Parsed string value
     */
    static std::string parseStringLiteral(const std::string &literal);

    /**
     * @brief Parse number literal
     * @param literal Number literal string
     * @return Parsed number value
     */
    static double parseNumberLiteral(const std::string &literal);

    /**
     * @brief Convert DataValue to JSValue (using void* to avoid circular dependencies)
     * @param dataValue DataValue as void pointer
     * @param dataEngine Data model engine pointer
     * @return Converted JSValue
     */
    JSValue convertDataValueToJSValue(const SCXML::DataModelEngine::DataValue &dataValue,
                                      std::shared_ptr<SCXML::DataModelEngine> dataEngine);

private:
    // JavaScript variables storage
    std::unordered_map<std::string, JSValue> globalJSVariables_;

    // Built-in functions registry
    std::unordered_map<std::string, std::function<JSValue(const std::vector<JSValue> &, const JSEvaluationContext &)>>
        builtinFunctions_;

    // Regular expressions for parsing
    static const std::regex numberRegex_;
    static const std::regex stringRegex_;
    static const std::regex identifierRegex_;
    static const std::regex functionCallRegex_;
};
}  // namespace SCXML
