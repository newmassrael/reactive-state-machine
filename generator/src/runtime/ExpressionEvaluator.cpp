#include "runtime/ExpressionEvaluator.h"
#include "common/Logger.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace SCXML {
namespace Runtime {

// ========== EvaluationResult Implementation ==========

bool ExpressionEvaluator::EvaluationResult::asBool() const {
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value);
    }
    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value) != 0;
    }
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value) != 0.0;
    }
    if (std::holds_alternative<std::string>(value)) {
        const std::string &str = std::get<std::string>(value);
        return !str.empty() && str != "false" && str != "0";
    }
    return false;  // null/monostate is false
}

int64_t ExpressionEvaluator::EvaluationResult::asInt() const {
    if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value);
    }
    if (std::holds_alternative<double>(value)) {
        return static_cast<int64_t>(std::get<double>(value));
    }
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? 1 : 0;
    }
    if (std::holds_alternative<std::string>(value)) {
        try {
            return std::stoll(std::get<std::string>(value));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

double ExpressionEvaluator::EvaluationResult::asDouble() const {
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    }
    if (std::holds_alternative<int64_t>(value)) {
        return static_cast<double>(std::get<int64_t>(value));
    }
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? 1.0 : 0.0;
    }
    if (std::holds_alternative<std::string>(value)) {
        try {
            return std::stod(std::get<std::string>(value));
        } catch (...) {
            return 0.0;
        }
    }
    return 0.0;
}

std::string ExpressionEvaluator::EvaluationResult::asString() const {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    }
    if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    }
    return "null";
}

bool ExpressionEvaluator::EvaluationResult::isNull() const {
    return std::holds_alternative<std::monostate>(value);
}

// ========== ExpressionEvaluator Implementation ==========

ExpressionEvaluator::ExpressionEvaluator() {
    // Register built-in functions
    registerFunction("In", [this](const std::vector<Value> &args) -> Value {
        // SCXML In() predicate - check if state is active
        if (args.size() != 1) {
            return false;
        }
        // This would need integration with state machine context
        // For now, return false
        return false;
    });

    SCXML::Common::Logger::info("ExpressionEvaluator initialized with datamodel: " + datamodelType_);
}

ExpressionEvaluator::~ExpressionEvaluator() = default;

// ========== Public Evaluation Methods ==========

ExpressionEvaluator::EvaluationResult ExpressionEvaluator::evaluateBoolean(const std::string &expression,
                                                                           const DataModel &dataModel,
                                                                           const EventContext *eventContext) {
    EvaluationResult result = evaluate(expression, dataModel, eventContext);
    if (result.success) {
        // Convert result to boolean
        bool boolValue = valueToBoolean(result.value);
        result.value = boolValue;
    }
    return result;
}

ExpressionEvaluator::EvaluationResult ExpressionEvaluator::evaluateString(const std::string &expression,
                                                                          const DataModel &dataModel,
                                                                          const EventContext *eventContext) {
    EvaluationResult result = evaluate(expression, dataModel, eventContext);
    if (result.success) {
        // Convert result to string
        std::string stringValue = valueToString(result.value);
        result.value = stringValue;
    }
    return result;
}

ExpressionEvaluator::EvaluationResult ExpressionEvaluator::evaluateNumeric(const std::string &expression,
                                                                           const DataModel &dataModel,
                                                                           const EventContext *eventContext) {
    EvaluationResult result = evaluate(expression, dataModel, eventContext);
    if (result.success) {
        // Convert result to number
        double numValue = valueToNumber(result.value);
        result.value = numValue;
    }
    return result;
}

ExpressionEvaluator::EvaluationResult ExpressionEvaluator::evaluate(const std::string &expression,
                                                                    const DataModel &dataModel,
                                                                    const EventContext *eventContext) {
    EvaluationResult result;

    if (expression.empty()) {
        result.success = true;
        result.value = true;  // Empty expressions are true in SCXML
        return result;
    }

    try {
        // Check for precompiled expression
        auto compiledIt = compiledExpressions_.find(expression);
        std::unique_ptr<ExpressionNode> ast;

        if (compiledIt != compiledExpressions_.end()) {
            // Use precompiled AST (we'll make a copy for thread safety)
            // For now, just reparse
        }

        // Tokenize and parse expression
        auto tokens = tokenize(expression);
        if (tokens.empty()) {
            result.error = "Empty expression";
            return result;
        }

        ast = parse(tokens);
        if (!ast) {
            result.error = "Failed to parse expression: " + expression;
            return result;
        }

        // Evaluate AST
        result.value = evaluateNode(ast.get(), dataModel, eventContext);
        result.success = true;

    } catch (const std::exception &e) {
        result.error = "Expression evaluation error: " + std::string(e.what());
        SCXML::Common::Logger::warning("ExpressionEvaluator::evaluate() - " + result.error + " (expression: " + expression + ")");
    }

    return result;
}

// ========== Convenience Methods ==========

bool ExpressionEvaluator::evaluateCondition(const std::string &expression, const DataModel &dataModel,
                                            const EventContext *eventContext) {
    auto result = evaluateBoolean(expression, dataModel, eventContext);
    return result.success ? result.asBool() : false;
}

std::string ExpressionEvaluator::evaluateAssignment(const std::string &expression, const DataModel &dataModel,
                                                    const EventContext *eventContext) {
    auto result = evaluateString(expression, dataModel, eventContext);
    return result.success ? result.asString() : "";
}

// ========== Configuration ==========

void ExpressionEvaluator::setDatamodel(const std::string &datamodel) {
    datamodelType_ = datamodel;
    SCXML::Common::Logger::info("ExpressionEvaluator datamodel set to: " + datamodel);
}

void ExpressionEvaluator::setStrictMode(bool strict) {
    strictMode_ = strict;
    SCXML::Common::Logger::info("ExpressionEvaluator strict mode " + std::string(strict ? "enabled" : "disabled"));
}

// ========== Custom Functions ==========

void ExpressionEvaluator::registerFunction(const std::string &name, CustomFunction function) {
    customFunctions_[name] = std::move(function);
}

void ExpressionEvaluator::unregisterFunction(const std::string &name) {
    customFunctions_.erase(name);
}

// ========== Utility Methods ==========

bool ExpressionEvaluator::isValidExpression(const std::string &expression) {
    try {
        auto tokens = tokenize(expression);
        auto ast = parse(tokens);
        return ast != nullptr;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> ExpressionEvaluator::getReferencedVariables(const std::string &expression) {
    std::vector<std::string> variables;

    try {
        auto tokens = tokenize(expression);
        for (const auto &token : tokens) {
            if (token.type == TokenType::IDENTIFIER && !isReservedKeyword(token.value)) {
                variables.push_back(token.value);
            }
        }

        // Remove duplicates
        std::sort(variables.begin(), variables.end());
        variables.erase(std::unique(variables.begin(), variables.end()), variables.end());

    } catch (...) {
        // Return empty vector on error
    }

    return variables;
}

size_t ExpressionEvaluator::precompileExpression(const std::string &expression) {
    try {
        auto tokens = tokenize(expression);
        auto ast = parse(tokens);

        if (ast) {
            size_t id = nextExpressionId_++;
            compiledExpressions_[expression] = std::move(ast);
            return id;
        }
    } catch (...) {
        // Ignore precompilation errors
    }

    return 0;  // Invalid ID
}

// ========== Tokenization ==========

std::vector<ExpressionEvaluator::Token> ExpressionEvaluator::tokenize(const std::string &expression) {
    std::vector<Token> tokens;
    size_t pos = 0;
    const size_t length = expression.length();

    while (pos < length) {
        char c = expression[pos];

        // Skip whitespace
        if (std::isspace(c)) {
            pos++;
            continue;
        }

        // String literals
        if (c == '\'' || c == '"') {
            char quote = c;
            size_t start = pos++;
            std::string value;

            while (pos < length && expression[pos] != quote) {
                if (expression[pos] == '\\' && pos + 1 < length) {
                    // Handle escape sequences
                    pos++;
                    char escaped = expression[pos];
                    switch (escaped) {
                    case 'n':
                        value += '\n';
                        break;
                    case 't':
                        value += '\t';
                        break;
                    case 'r':
                        value += '\r';
                        break;
                    case '\\':
                        value += '\\';
                        break;
                    case '\'':
                        value += '\'';
                        break;
                    case '"':
                        value += '"';
                        break;
                    default:
                        value += escaped;
                        break;
                    }
                } else {
                    value += expression[pos];
                }
                pos++;
            }

            if (pos < length && expression[pos] == quote) {
                pos++;  // Skip closing quote
            }

            tokens.emplace_back(TokenType::STRING_LITERAL, value, start);
            continue;
        }

        // Numbers
        if (std::isdigit(c) || (c == '.' && pos + 1 < length && std::isdigit(expression[pos + 1]))) {
            size_t start = pos;
            std::string value;
            bool hasDecimal = false;

            while (pos < length && (std::isdigit(expression[pos]) || (!hasDecimal && expression[pos] == '.'))) {
                if (expression[pos] == '.') {
                    hasDecimal = true;
                }
                value += expression[pos];
                pos++;
            }

            tokens.emplace_back(TokenType::NUMBER, value, start);
            continue;
        }

        // Identifiers and keywords
        if (isIdentifierStart(c)) {
            size_t start = pos;
            std::string value;

            while (pos < length && isIdentifierContinuation(expression[pos])) {
                value += expression[pos];
                pos++;
            }

            // Check for boolean literals
            if (value == "true" || value == "false") {
                tokens.emplace_back(TokenType::BOOLEAN, value, start);
            } else {
                tokens.emplace_back(TokenType::IDENTIFIER, value, start);
            }
            continue;
        }

        // Two-character operators
        if (pos + 1 < length) {
            std::string twoChar = expression.substr(pos, 2);
            if (twoChar == "==" || twoChar == "!=" || twoChar == "<=" || twoChar == ">=" || twoChar == "&&" ||
                twoChar == "||") {
                tokens.emplace_back(TokenType::OPERATOR, twoChar, pos);
                pos += 2;
                continue;
            }
        }

        // Single-character tokens
        switch (c) {
        case '(':
        case ')':
            tokens.emplace_back(TokenType::PARENTHESIS, std::string(1, c), pos);
            break;
        case '.':
            tokens.emplace_back(TokenType::DOT, ".", pos);
            break;
        case ',':
            tokens.emplace_back(TokenType::COMMA, ",", pos);
            break;
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case '<':
        case '>':
        case '!':
            tokens.emplace_back(TokenType::OPERATOR, std::string(1, c), pos);
            break;
        default:
            throw std::runtime_error("Unexpected character: " + std::string(1, c) + " at position " +
                                     std::to_string(pos));
        }

        pos++;
    }

    tokens.emplace_back(TokenType::END_OF_INPUT, "", pos);
    return tokens;
}

// ========== Parsing (Recursive Descent Parser) ==========

std::unique_ptr<ExpressionEvaluator::ExpressionNode> ExpressionEvaluator::parse(const std::vector<Token> &tokens) {
    if (tokens.empty()) {
        return nullptr;
    }

    size_t position = 0;
    auto root = parseExpression(tokens, position);

    if (position < tokens.size() && tokens[position].type != TokenType::END_OF_INPUT) {
        throw std::runtime_error("Unexpected token: " + tokens[position].value);
    }

    return root;
}

std::unique_ptr<ExpressionEvaluator::ExpressionNode>
ExpressionEvaluator::parseExpression(const std::vector<Token> &tokens, size_t &position) {
    return parseLogicalOr(tokens, position);
}

std::unique_ptr<ExpressionEvaluator::ExpressionNode>
ExpressionEvaluator::parseLogicalOr(const std::vector<Token> &tokens, size_t &position) {
    auto left = parseLogicalAnd(tokens, position);

    while (position < tokens.size() && tokens[position].type == TokenType::OPERATOR && tokens[position].value == "||") {
        std::string op = tokens[position].value;
        position++;
        auto right = parseLogicalAnd(tokens, position);

        auto node = std::make_unique<ExpressionNode>(NodeType::BINARY_OP, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }

    return left;
}

std::unique_ptr<ExpressionEvaluator::ExpressionNode>
ExpressionEvaluator::parseLogicalAnd(const std::vector<Token> &tokens, size_t &position) {
    auto left = parseEquality(tokens, position);

    while (position < tokens.size() && tokens[position].type == TokenType::OPERATOR && tokens[position].value == "&&") {
        std::string op = tokens[position].value;
        position++;
        auto right = parseEquality(tokens, position);

        auto node = std::make_unique<ExpressionNode>(NodeType::BINARY_OP, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }

    return left;
}

std::unique_ptr<ExpressionEvaluator::ExpressionNode>
ExpressionEvaluator::parseEquality(const std::vector<Token> &tokens, size_t &position) {
    auto left = parseComparison(tokens, position);

    while (position < tokens.size() && tokens[position].type == TokenType::OPERATOR &&
           (tokens[position].value == "==" || tokens[position].value == "!=")) {
        std::string op = tokens[position].value;
        position++;
        auto right = parseComparison(tokens, position);

        auto node = std::make_unique<ExpressionNode>(NodeType::BINARY_OP, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }

    return left;
}

std::unique_ptr<ExpressionEvaluator::ExpressionNode>
ExpressionEvaluator::parseComparison(const std::vector<Token> &tokens, size_t &position) {
    auto left = parseArithmetic(tokens, position);

    while (position < tokens.size() && tokens[position].type == TokenType::OPERATOR &&
           (tokens[position].value == "<" || tokens[position].value == "<=" || tokens[position].value == ">" ||
            tokens[position].value == ">=")) {
        std::string op = tokens[position].value;
        position++;
        auto right = parseArithmetic(tokens, position);

        auto node = std::make_unique<ExpressionNode>(NodeType::BINARY_OP, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }

    return left;
}

std::unique_ptr<ExpressionEvaluator::ExpressionNode>
ExpressionEvaluator::parseArithmetic(const std::vector<Token> &tokens, size_t &position) {
    auto left = parseTerm(tokens, position);

    while (position < tokens.size() && tokens[position].type == TokenType::OPERATOR &&
           (tokens[position].value == "+" || tokens[position].value == "-")) {
        std::string op = tokens[position].value;
        position++;
        auto right = parseTerm(tokens, position);

        auto node = std::make_unique<ExpressionNode>(NodeType::BINARY_OP, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }

    return left;
}

std::unique_ptr<ExpressionEvaluator::ExpressionNode> ExpressionEvaluator::parseTerm(const std::vector<Token> &tokens,
                                                                                    size_t &position) {
    auto left = parseUnary(tokens, position);

    while (position < tokens.size() && tokens[position].type == TokenType::OPERATOR &&
           (tokens[position].value == "*" || tokens[position].value == "/" || tokens[position].value == "%")) {
        std::string op = tokens[position].value;
        position++;
        auto right = parseUnary(tokens, position);

        auto node = std::make_unique<ExpressionNode>(NodeType::BINARY_OP, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }

    return left;
}

std::unique_ptr<ExpressionEvaluator::ExpressionNode> ExpressionEvaluator::parseUnary(const std::vector<Token> &tokens,
                                                                                     size_t &position) {
    if (position < tokens.size() && tokens[position].type == TokenType::OPERATOR &&
        (tokens[position].value == "!" || tokens[position].value == "-" || tokens[position].value == "+")) {
        std::string op = tokens[position].value;
        position++;
        auto operand = parseUnary(tokens, position);

        auto node = std::make_unique<ExpressionNode>(NodeType::UNARY_OP, op);
        node->children.push_back(std::move(operand));
        return node;
    }

    return parsePrimary(tokens, position);
}

std::unique_ptr<ExpressionEvaluator::ExpressionNode> ExpressionEvaluator::parsePrimary(const std::vector<Token> &tokens,
                                                                                       size_t &position) {
    if (position >= tokens.size()) {
        throw std::runtime_error("Unexpected end of expression");
    }

    const Token &token = tokens[position];

    switch (token.type) {
    case TokenType::NUMBER:
    case TokenType::STRING_LITERAL:
    case TokenType::BOOLEAN:
        position++;
        return std::make_unique<ExpressionNode>(NodeType::LITERAL, token.value);

    case TokenType::IDENTIFIER: {
        std::string name = token.value;
        position++;

        // Check for function call
        if (position < tokens.size() && tokens[position].type == TokenType::PARENTHESIS &&
            tokens[position].value == "(") {
            position++;  // Skip '('

            auto funcNode = std::make_unique<ExpressionNode>(NodeType::FUNCTION_CALL, name);

            // Parse arguments
            if (position < tokens.size() && tokens[position].type != TokenType::PARENTHESIS) {
                do {
                    funcNode->children.push_back(parseExpression(tokens, position));

                    if (position < tokens.size() && tokens[position].type == TokenType::COMMA) {
                        position++;  // Skip ','
                    } else {
                        break;
                    }
                } while (position < tokens.size());
            }

            if (position >= tokens.size() || tokens[position].type != TokenType::PARENTHESIS ||
                tokens[position].value != ")") {
                throw std::runtime_error("Expected ')' after function arguments");
            }
            position++;  // Skip ')'

            return funcNode;
        }

        // Variable access (possibly with dot notation)
        auto varNode = std::make_unique<ExpressionNode>(NodeType::VARIABLE, name);

        // Handle dot notation (e.g., event.data, data.field)
        while (position < tokens.size() && tokens[position].type == TokenType::DOT) {
            position++;  // Skip '.'

            if (position >= tokens.size() || tokens[position].type != TokenType::IDENTIFIER) {
                throw std::runtime_error("Expected identifier after '.'");
            }

            name += "." + tokens[position].value;
            position++;
        }

        varNode->value = name;
        return varNode;
    }

    case TokenType::PARENTHESIS:
        if (token.value == "(") {
            position++;  // Skip '('
            auto expr = parseExpression(tokens, position);

            if (position >= tokens.size() || tokens[position].type != TokenType::PARENTHESIS ||
                tokens[position].value != ")") {
                throw std::runtime_error("Expected ')'");
            }
            position++;  // Skip ')'

            return expr;
        }
        break;

    default:
        break;
    }

    throw std::runtime_error("Unexpected token: " + token.value);
}

// ========== AST Evaluation ==========

ExpressionEvaluator::Value ExpressionEvaluator::evaluateNode(const ExpressionNode *node, const DataModel &dataModel,
                                                             const EventContext *eventContext) {
    if (!node) {
        return std::monostate{};
    }

    switch (node->type) {
    case NodeType::LITERAL: {
        const std::string &value = node->value;

        // Boolean literal
        if (value == "true") {
            return true;
        }
        if (value == "false") {
            return false;
        }

        // Try to parse as number
        try {
            if (value.find('.') != std::string::npos) {
                return std::stod(value);
            } else {
                return std::stoll(value);
            }
        } catch (...) {
            // Not a number, treat as string
            return value;
        }
    }

    case NodeType::VARIABLE:
        return getVariable(node->value, dataModel, eventContext);

    case NodeType::BINARY_OP: {
        if (node->children.size() != 2) {
            throw std::runtime_error("Binary operation must have exactly 2 operands");
        }

        Value left = evaluateNode(node->children[0].get(), dataModel, eventContext);
        Value right = evaluateNode(node->children[1].get(), dataModel, eventContext);

        return performBinaryOperation(node->value, left, right);
    }

    case NodeType::UNARY_OP: {
        if (node->children.size() != 1) {
            throw std::runtime_error("Unary operation must have exactly 1 operand");
        }

        Value operand = evaluateNode(node->children[0].get(), dataModel, eventContext);
        return performUnaryOperation(node->value, operand);
    }

    case NodeType::FUNCTION_CALL: {
        const std::string &funcName = node->value;

        auto it = customFunctions_.find(funcName);
        if (it == customFunctions_.end()) {
            throw std::runtime_error("Unknown function: " + funcName);
        }

        std::vector<Value> args;
        for (const auto &child : node->children) {
            args.push_back(evaluateNode(child.get(), dataModel, eventContext));
        }

        return it->second(args);
    }
    }

    return std::monostate{};
}

ExpressionEvaluator::Value ExpressionEvaluator::getVariable(const std::string &name, const DataModel &dataModel,
                                                            const EventContext *eventContext) {
    // Handle special SCXML variables
    if (name.substr(0, 6) == "event.") {
        if (!eventContext) {
            if (strictMode_) {
                throw std::runtime_error("Event context not available for: " + name);
            }
            return std::monostate{};
        }

        if (name == "event.name") {
            return eventContext->eventName;
        } else if (name == "event.data") {
            return eventContext->eventData;
        } else if (name.substr(0, 11) == "event.data.") {
            std::string field = name.substr(11);  // Remove "event.data."
            auto it = eventContext->eventFields.find(field);
            return it != eventContext->eventFields.end() ? it->second : std::string{};
        }
    }

    // Handle data model variables
    auto it = dataModel.find(name);
    if (it != dataModel.end()) {
        const std::string &value = it->second;

        // Try to parse as appropriate type
        if (value == "true") {
            return true;
        }
        if (value == "false") {
            return false;
        }

        try {
            if (value.find('.') != std::string::npos) {
                return std::stod(value);
            } else {
                return std::stoll(value);
            }
        } catch (...) {
            // Return as string
            return value;
        }
    }

    // Handle dotted notation (e.g., data.field)
    if (name.substr(0, 5) == "data.") {
        std::string fieldName = name.substr(5);  // Remove "data."
        return getVariable(fieldName, dataModel, eventContext);
    }

    if (strictMode_) {
        throw std::runtime_error("Undefined variable: " + name);
    }

    return std::monostate{};  // null/undefined
}

// ========== Operations ==========

ExpressionEvaluator::Value ExpressionEvaluator::performBinaryOperation(const std::string &op, const Value &left,
                                                                       const Value &right) {
    if (op == "&&") {
        return valueToBoolean(left) && valueToBoolean(right);
    } else if (op == "||") {
        return valueToBoolean(left) || valueToBoolean(right);
    } else if (op == "==") {
        // Type-flexible equality
        if (left.index() == right.index()) {
            return left == right;
        }
        // Convert both to string for comparison
        return valueToString(left) == valueToString(right);
    } else if (op == "!=") {
        // Type-flexible inequality
        if (left.index() == right.index()) {
            return left != right;
        }
        return valueToString(left) != valueToString(right);
    } else if (op == "<") {
        return valueToNumber(left) < valueToNumber(right);
    } else if (op == "<=") {
        return valueToNumber(left) <= valueToNumber(right);
    } else if (op == ">") {
        return valueToNumber(left) > valueToNumber(right);
    } else if (op == ">=") {
        return valueToNumber(left) >= valueToNumber(right);
    } else if (op == "+") {
        // String concatenation or numeric addition
        if (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right)) {
            return valueToString(left) + valueToString(right);
        } else {
            return valueToNumber(left) + valueToNumber(right);
        }
    } else if (op == "-") {
        return valueToNumber(left) - valueToNumber(right);
    } else if (op == "*") {
        return valueToNumber(left) * valueToNumber(right);
    } else if (op == "/") {
        double rightNum = valueToNumber(right);
        if (rightNum == 0.0) {
            throw std::runtime_error("Division by zero");
        }
        return valueToNumber(left) / rightNum;
    } else if (op == "%") {
        double rightNum = valueToNumber(right);
        if (rightNum == 0.0) {
            throw std::runtime_error("Modulo by zero");
        }
        return std::fmod(valueToNumber(left), rightNum);
    }

    throw std::runtime_error("Unknown binary operator: " + op);
}

ExpressionEvaluator::Value ExpressionEvaluator::performUnaryOperation(const std::string &op, const Value &operand) {
    if (op == "!") {
        return !valueToBoolean(operand);
    } else if (op == "-") {
        return -valueToNumber(operand);
    } else if (op == "+") {
        return valueToNumber(operand);
    }

    throw std::runtime_error("Unknown unary operator: " + op);
}

// ========== Type Conversion ==========

bool ExpressionEvaluator::valueToBoolean(const Value &value) {
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        return std::get<int64_t>(value) != 0;
    } else if (std::holds_alternative<double>(value)) {
        double d = std::get<double>(value);
        return d != 0.0 && !std::isnan(d);
    } else if (std::holds_alternative<std::string>(value)) {
        const std::string &str = std::get<std::string>(value);
        return !str.empty() && str != "false" && str != "0";
    } else {
        return false;  // null/monostate is false
    }
}

std::string ExpressionEvaluator::valueToString(const Value &value) {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    } else if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    } else {
        return "null";
    }
}

double ExpressionEvaluator::valueToNumber(const Value &value) {
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        return static_cast<double>(std::get<int64_t>(value));
    } else if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? 1.0 : 0.0;
    } else if (std::holds_alternative<std::string>(value)) {
        const std::string &str = std::get<std::string>(value);
        try {
            return std::stod(str);
        } catch (...) {
            return 0.0;  // NaN in JavaScript, but we'll use 0
        }
    } else {
        return 0.0;  // null/monostate is 0
    }
}

// ========== Helper Methods ==========

bool ExpressionEvaluator::isIdentifierStart(char c) {
    return std::isalpha(c) || c == '_' || c == '$';
}

bool ExpressionEvaluator::isIdentifierContinuation(char c) {
    return std::isalnum(c) || c == '_' || c == '$';
}

bool ExpressionEvaluator::isReservedKeyword(const std::string &str) {
    static const std::vector<std::string> keywords = {"true", "false", "null", "undefined", "In"};

    return std::find(keywords.begin(), keywords.end(), str) != keywords.end();
}

std::string ExpressionEvaluator::unescapeString(const std::string &str) {
    // This is handled during tokenization
    return str;
}

// ========== Factory Functions ==========

std::unique_ptr<ExpressionEvaluator> createExpressionEvaluator(const std::string &datamodel) {
    auto evaluator = std::make_unique<ExpressionEvaluator>();
    evaluator->setDatamodel(datamodel);
    return evaluator;
}

ExpressionEvaluator &getGlobalEvaluator() {
    static ExpressionEvaluator globalEvaluator;
    return globalEvaluator;
}

}  // namespace Runtime
}  // namespace SCXML