#pragma once

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace SCXML {
namespace Runtime {

/**
 * @brief SCXML Expression Evaluator - Evaluates expressions in SCXML conditions and assignments
 *
 * This class implements a lightweight expression evaluator for SCXML expressions.
 * It supports:
 * - Data model variable access (data.variable, variable)
 * - Comparison operators (==, !=, <, <=, >, >=)
 * - Logical operators (&&, ||, !)
 * - Arithmetic operators (+, -, *, /, %)
 * - String literals ('string', "string")
 * - Numeric literals (123, 45.67)
 * - Boolean literals (true, false)
 * - Event data access (event.data)
 *
 * W3C SCXML compliance:
 * - Supports null datamodel expressions
 * - Handles undefined variables gracefully
 * - Type coercion following JavaScript semantics (for ECMAScript datamodel)
 */
class ExpressionEvaluator {
public:
    /**
     * @brief Expression evaluation result
     */
    using Value = std::variant<std::monostate, bool, int64_t, double, std::string>;

    /**
     * @brief Data model type - maps variable names to values
     */
    using DataModel = std::unordered_map<std::string, std::string>;

    /**
     * @brief Event data context for expression evaluation
     */
    struct EventContext {
        std::string eventName;
        std::string eventData;
        std::unordered_map<std::string, std::string> eventFields;
        std::unordered_map<std::string, std::string> variables;  // 호환성을 위해 추가
    };

    /**
     * @brief Boolean evaluation result
     */
    struct BooleanResult {
        bool success = false;
        bool value = false;
        std::string error;
    };

    /**
     * @brief String evaluation result
     */
    struct StringResult {
        bool success = false;
        std::string value;
        std::string error;
    };

    /**
     * @brief Number evaluation result
     */
    struct NumberResult {
        bool success = false;
        double value = 0.0;
        std::string error;
    };

    /**
     * @brief Expression evaluation result
     */
    struct EvaluationResult {
        bool success = false;
        Value value;
        std::string error;

        // Convenience methods
        bool asBool() const;
        int64_t asInt() const;
        double asDouble() const;
        std::string asString() const;
        bool isNull() const;
    };

    /**
     * @brief Constructor
     */
    ExpressionEvaluator();

    /**
     * @brief Destructor
     */
    ~ExpressionEvaluator();

    // ========== Expression Evaluation ==========

    /**
     * @brief Evaluate boolean expression (for conditions)
     * @param expression Expression to evaluate
     * @param dataModel Current data model state
     * @param eventContext Event context (optional)
     * @return Evaluation result
     */
    EvaluationResult evaluateBoolean(const std::string &expression, const DataModel &dataModel,
                                     const EventContext *eventContext = nullptr);

    /**
     * @brief Evaluate string expression (for assignments, logging)
     * @param expression Expression to evaluate
     * @param dataModel Current data model state
     * @param eventContext Event context (optional)
     * @return Evaluation result
     */
    EvaluationResult evaluateString(const std::string &expression, const DataModel &dataModel,
                                    const EventContext *eventContext = nullptr);

    /**
     * @brief Evaluate numeric expression
     * @param expression Expression to evaluate
     * @param dataModel Current data model state
     * @param eventContext Event context (optional)
     * @return Evaluation result
     */
    EvaluationResult evaluateNumeric(const std::string &expression, const DataModel &dataModel,
                                     const EventContext *eventContext = nullptr);

    /**
     * @brief Evaluate generic expression (auto-detect return type)
     * @param expression Expression to evaluate
     * @param dataModel Current data model state
     * @param eventContext Event context (optional)
     * @return Evaluation result
     */
    EvaluationResult evaluate(const std::string &expression, const DataModel &dataModel,
                              const EventContext *eventContext = nullptr);

    // ========== Convenience Methods for Generated Code ==========

    /**
     * @brief Quick boolean evaluation for guard conditions
     * @param expression Guard condition expression
     * @param dataModel Data model
     * @param eventContext Event context (optional)
     * @return true if condition is met, false otherwise
     */
    bool evaluateCondition(const std::string &expression, const DataModel &dataModel,
                           const EventContext *eventContext = nullptr);

    /**
     * @brief Quick string evaluation for assignments
     * @param expression Assignment expression
     * @param dataModel Data model
     * @param eventContext Event context (optional)
     * @return Evaluated string value
     */
    std::string evaluateAssignment(const std::string &expression, const DataModel &dataModel,
                                   const EventContext *eventContext = nullptr);

    // ========== Configuration ==========

    /**
     * @brief Set datamodel type for expression semantics
     * @param datamodel Datamodel type ("null", "ecmascript", "xpath")
     */
    void setDatamodel(const std::string &datamodel);

    /**
     * @brief Get current datamodel type
     * @return Datamodel type
     */
    const std::string &getDatamodel() const {
        return datamodelType_;
    }

    /**
     * @brief Enable/disable strict mode
     * In strict mode, undefined variables cause errors
     * In non-strict mode, undefined variables return null/false
     * @param strict True for strict mode
     */
    void setStrictMode(bool strict);

    /**
     * @brief Check if strict mode is enabled
     * @return True if strict mode is enabled
     */
    bool isStrictMode() const {
        return strictMode_;
    }

    // ========== Custom Functions ==========

    /**
     * @brief Custom function type
     */
    using CustomFunction = std::function<Value(const std::vector<Value> &)>;

    /**
     * @brief Register custom function
     * @param name Function name
     * @param function Function implementation
     */
    void registerFunction(const std::string &name, CustomFunction function);

    /**
     * @brief Unregister custom function
     * @param name Function name
     */
    void unregisterFunction(const std::string &name);

    // ========== Utility Methods ==========

    /**
     * @brief Check if expression is valid syntax
     * @param expression Expression to validate
     * @return True if syntax is valid
     */
    bool isValidExpression(const std::string &expression);

    /**
     * @brief Get all variables referenced in expression
     * @param expression Expression to analyze
     * @return Set of variable names
     */
    std::vector<std::string> getReferencedVariables(const std::string &expression);

    /**
     * @brief Precompile expression for faster evaluation
     * @param expression Expression to precompile
     * @return Precompiled expression handle (for future use)
     */
    size_t precompileExpression(const std::string &expression);

private:
    // ========== Expression Parsing ==========

    /**
     * @brief Token types for expression parsing
     */
    enum class TokenType {
        IDENTIFIER,      // variable names
        STRING_LITERAL,  // 'string' or "string"
        NUMBER,          // 123, 45.67
        BOOLEAN,         // true, false
        OPERATOR,        // +, -, *, /, ==, !=, <, <=, >, >=, &&, ||, !
        PARENTHESIS,     // (, )
        DOT,             // .
        COMMA,           // ,
        END_OF_INPUT
    };

    /**
     * @brief Token structure
     */
    struct Token {
        TokenType type;
        std::string value;
        size_t position;

        Token(TokenType t, std::string v, size_t pos) : type(t), value(std::move(v)), position(pos) {}
    };

    /**
     * @brief Expression AST node types
     */
    enum class NodeType {
        LITERAL,       // string, number, boolean literals
        VARIABLE,      // variable access
        BINARY_OP,     // binary operations
        UNARY_OP,      // unary operations
        FUNCTION_CALL  // function calls
    };

    /**
     * @brief Expression AST node
     */
    struct ExpressionNode {
        NodeType type;
        std::string value;
        std::vector<std::unique_ptr<ExpressionNode>> children;

        ExpressionNode(NodeType t, std::string v = "") : type(t), value(std::move(v)) {}
    };

    // ========== Internal State ==========
    std::string datamodelType_ = "null";
    bool strictMode_ = false;
    std::unordered_map<std::string, CustomFunction> customFunctions_;

    // Precompiled expressions cache
    std::unordered_map<std::string, std::unique_ptr<ExpressionNode>> compiledExpressions_;
    size_t nextExpressionId_ = 1;

    // ========== Internal Methods ==========

    /**
     * @brief Tokenize expression string
     * @param expression Expression to tokenize
     * @return List of tokens
     */
    std::vector<Token> tokenize(const std::string &expression);

    /**
     * @brief Parse tokens into AST
     * @param tokens Token list
     * @return Root AST node
     */
    std::unique_ptr<ExpressionNode> parse(const std::vector<Token> &tokens);

    /**
     * @brief Parse expression (recursive descent parser)
     * @param tokens Token list
     * @param position Current position (modified)
     * @return AST node
     */
    std::unique_ptr<ExpressionNode> parseExpression(const std::vector<Token> &tokens, size_t &position);
    std::unique_ptr<ExpressionNode> parseLogicalOr(const std::vector<Token> &tokens, size_t &position);
    std::unique_ptr<ExpressionNode> parseLogicalAnd(const std::vector<Token> &tokens, size_t &position);
    std::unique_ptr<ExpressionNode> parseEquality(const std::vector<Token> &tokens, size_t &position);
    std::unique_ptr<ExpressionNode> parseComparison(const std::vector<Token> &tokens, size_t &position);
    std::unique_ptr<ExpressionNode> parseArithmetic(const std::vector<Token> &tokens, size_t &position);
    std::unique_ptr<ExpressionNode> parseTerm(const std::vector<Token> &tokens, size_t &position);
    std::unique_ptr<ExpressionNode> parseUnary(const std::vector<Token> &tokens, size_t &position);
    std::unique_ptr<ExpressionNode> parsePrimary(const std::vector<Token> &tokens, size_t &position);

    /**
     * @brief Evaluate AST node
     * @param node AST node to evaluate
     * @param dataModel Data model context
     * @param eventContext Event context
     * @return Evaluation result
     */
    Value evaluateNode(const ExpressionNode *node, const DataModel &dataModel, const EventContext *eventContext);

    /**
     * @brief Get variable value from data model
     * @param name Variable name (can be dotted path)
     * @param dataModel Data model
     * @param eventContext Event context
     * @return Variable value
     */
    Value getVariable(const std::string &name, const DataModel &dataModel, const EventContext *eventContext);

    /**
     * @brief Perform binary operation
     * @param op Operator string
     * @param left Left operand
     * @param right Right operand
     * @return Operation result
     */
    Value performBinaryOperation(const std::string &op, const Value &left, const Value &right);

    /**
     * @brief Perform unary operation
     * @param op Operator string
     * @param operand Operand
     * @return Operation result
     */
    Value performUnaryOperation(const std::string &op, const Value &operand);

    /**
     * @brief Convert value to boolean (JavaScript semantics)
     * @param value Value to convert
     * @return Boolean result
     */
    bool valueToBoolean(const Value &value);

    /**
     * @brief Convert value to string
     * @param value Value to convert
     * @return String result
     */
    std::string valueToString(const Value &value);

    /**
     * @brief Convert value to number
     * @param value Value to convert
     * @return Numeric result
     */
    double valueToNumber(const Value &value);

    /**
     * @brief Check if character is valid identifier start
     * @param c Character to check
     * @return True if valid
     */
    bool isIdentifierStart(char c);

    /**
     * @brief Check if character is valid identifier continuation
     * @param c Character to check
     * @return True if valid
     */
    bool isIdentifierContinuation(char c);

    /**
     * @brief Check if string is a reserved keyword
     * @param str String to check
     * @return True if reserved
     */
    bool isReservedKeyword(const std::string &str);

    /**
     * @brief Unescape string literal
     * @param str Escaped string
     * @return Unescaped string
     */
    std::string unescapeString(const std::string &str);
};

/**
 * @brief Convenience function to create expression evaluator
 * @param datamodel Datamodel type
 * @return Unique pointer to evaluator
 */
std::unique_ptr<ExpressionEvaluator> createExpressionEvaluator(const std::string &datamodel = "null");

/**
 * @brief Global expression evaluator instance for generated code
 * @return Shared expression evaluator
 */
ExpressionEvaluator &getGlobalEvaluator();

}  // namespace Runtime
}  // namespace SCXML