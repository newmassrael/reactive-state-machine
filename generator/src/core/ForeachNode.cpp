#include "core/ForeachNode.h"
#include "core/DataModel.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/LocationExpressionEvaluator.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>

namespace SCXML {
namespace Core {

// ============================================================================
// ForeachNode Implementation
// ============================================================================

ForeachNode::ForeachNode() : array_(""), item_(""), index_("") {}

SCXML::Common::Result<void> ForeachNode::execute(SCXML::Runtime::RuntimeContext &context) {
    try {
        // Validate the foreach node before execution
        auto validationErrors = validate();
        if (!validationErrors.empty()) {
            std::string errorMsg = "Foreach node validation failed: ";
            for (const auto &error : validationErrors) {
                errorMsg += error + "; ";
            }
            return SCXML::Common::Result<void>::failure(errorMsg);
        }

        // Resolve array to iterate over
        auto arrayResult = resolveArray(context);
        if (!arrayResult.isSuccess()) {
            return SCXML::Common::Result<void>::failure("Failed to resolve array: " + arrayResult.getError());
        }

        const std::vector<std::string> &arrayItems = arrayResult.getValue();

        // Store original values of item and index variables (if they exist)
        std::string originalItemValue;
        std::string originalIndexValue;
        bool hadItem = context.getDataModel().hasVariable(item_);
        bool hadIndex = !index_.empty() && context.getDataModel().hasVariable(index_);

        if (hadItem) {
            auto itemResult = context.getDataModel().getVariable(item_);
            if (itemResult.isSuccess()) {
                originalItemValue = itemResult.getValue();
            }
        }

        if (hadIndex) {
            auto indexResult = context.getDataModel().getVariable(index_);
            if (indexResult.isSuccess()) {
                originalIndexValue = indexResult.getValue();
            }
        }

        // Execute iteration for each array item
        for (size_t i = 0; i < arrayItems.size(); ++i) {
            auto iterationResult = executeIteration(context, arrayItems[i], static_cast<int>(i));
            if (!iterationResult.isSuccess()) {
                // Restore original values before returning error
                if (hadItem) {
                    context.getDataModel().setVariable(item_, originalItemValue);
                } else {
                    context.getDataModel().removeVariable(item_);
                }

                if (hadIndex) {
                    context.getDataModel().setVariable(index_, originalIndexValue);
                } else if (!index_.empty()) {
                    context.getDataModel().removeVariable(index_);
                }

                return SCXML::Common::Result<void>::failure("Iteration " + std::to_string(i) +
                                                            " failed: " + iterationResult.getError());
            }
        }

        // Restore original variable values
        if (hadItem) {
            context.getDataModel().setVariable(item_, originalItemValue);
        } else {
            context.getDataModel().removeVariable(item_);
        }

        if (hadIndex) {
            context.getDataModel().setVariable(index_, originalIndexValue);
        } else if (!index_.empty()) {
            context.getDataModel().removeVariable(index_);
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception during foreach execution: " + std::string(e.what()));
    }
}

const std::string &ForeachNode::getArray() const {
    return array_;
}

void ForeachNode::setArray(const std::string &array) {
    array_ = array;
}

const std::string &ForeachNode::getItem() const {
    return item_;
}

void ForeachNode::setItem(const std::string &item) {
    item_ = item;
}

const std::string &ForeachNode::getIndex() const {
    return index_;
}

void ForeachNode::setIndex(const std::string &index) {
    index_ = index;
}

void ForeachNode::addChild(std::shared_ptr<void> child) {
    children_.push_back(child);
}

std::vector<std::shared_ptr<void>> ForeachNode::getChildren() const {
    return children_;
}

std::vector<std::string> ForeachNode::validate() const {
    std::vector<std::string> errors;

    // Array expression is required
    if (array_.empty()) {
        errors.push_back("Array expression is required");
    }

    // Item variable name is required
    if (item_.empty()) {
        errors.push_back("Item variable name is required");
    }

    // Item variable name must be valid identifier
    if (!item_.empty() && !DataModel::isValidVariableName(item_)) {
        errors.push_back("Invalid item variable name: " + item_);
    }

    // Index variable name must be valid if specified
    if (!index_.empty() && !DataModel::isValidVariableName(index_)) {
        errors.push_back("Invalid index variable name: " + index_);
    }

    // Item and index cannot be the same
    if (!item_.empty() && !index_.empty() && item_ == index_) {
        errors.push_back("Item and index variable names cannot be the same");
    }

    return errors;
}

std::shared_ptr<IForeachNode> ForeachNode::clone() const {
    auto cloned = std::make_shared<ForeachNode>();
    cloned->array_ = array_;
    cloned->item_ = item_;
    cloned->index_ = index_;
    cloned->children_ = children_;  // Shallow copy of children pointers
    return cloned;
}

SCXML::Common::Result<std::vector<std::string>>
ForeachNode::resolveArray(SCXML::Runtime::RuntimeContext &context) const {
    try {
        LocationExpressionEvaluator locationEval;

        // Try to resolve as location expression first
        auto locationResult = locationEval.getValue(context, array_);
        if (locationResult.isSuccess()) {
            std::string arrayData = locationResult.getValue();
            auto items = parseArrayData(arrayData, context);
            return SCXML::Common::Result<std::vector<std::string>>::success(items);
        }

        // Try to resolve as expression
        EnhancedExpressionEvaluator exprEval;
        auto exprResult = exprEval.evaluateStringExpression(context, array_);
        if (exprResult.isSuccess()) {
            std::string arrayData = exprResult.getValue();
            auto items = parseArrayData(arrayData, context);
            return SCXML::Common::Result<std::vector<std::string>>::success(items);
        }

        return SCXML::Common::Result<std::vector<std::string>>::failure("Cannot resolve array expression: " + array_);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::vector<std::string>>::failure("Exception resolving array: " +
                                                                        std::string(e.what()));
    }
}

SCXML::Common::Result<void> ForeachNode::executeIteration(SCXML::Runtime::RuntimeContext &context,
                                                          const std::string &itemValue, int indexValue) {
    try {
        // Set iteration variables
        context.getDataModel().setVariable(item_, itemValue);

        if (!index_.empty()) {
            context.getDataModel().setVariable(index_, std::to_string(indexValue));
        }

        // Execute child elements
        // Note: In a full implementation, this would execute actual child nodes
        // For now, we'll just log the iteration
        context.getLogger().info("Foreach iteration " + std::to_string(indexValue) + ": " + item_ + " = " + itemValue);

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception during iteration: " + std::string(e.what()));
    }
}

std::vector<std::string> ForeachNode::parseArrayData(const std::string &arrayData,
                                                     SCXML::Runtime::RuntimeContext &context) const {
    std::vector<std::string> items;

    try {
        // Check if it's a JSON array
        std::string trimmed = arrayData;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

        if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']') {
            // Simple JSON array parsing
            std::string content = trimmed.substr(1, trimmed.length() - 2);

            if (!content.empty()) {
                std::stringstream ss(content);
                std::string item;

                while (std::getline(ss, item, ',')) {
                    // Trim whitespace and quotes
                    item.erase(0, item.find_first_not_of(" \t\n\r"));
                    item.erase(item.find_last_not_of(" \t\n\r") + 1);

                    if (!item.empty()) {
                        items.push_back(item);
                    }
                }
            }
        }
        catch (const std::exception &e) {
            // Fallback: treat entire string as single item
            items.push_back(arrayData);
        }

        return items;
    }

    // ============================================================================
    // LogNode Implementation
    // ============================================================================

    LogNode::LogNode() : expr_(""), label_(""), level_("info") {}

    SCXML::Common::Result<void> LogNode::execute(SCXML::Runtime::RuntimeContext & context) {
        try {
            // Validate the log node before execution
            auto validationErrors = validate();
            if (!validationErrors.empty()) {
                std::string errorMsg = "Log node validation failed: ";
                for (const auto &error : validationErrors) {
                    errorMsg += error + "; ";
                }
                return SCXML::Common::Result<void>::failure(errorMsg);
            }

            // Resolve log message
            auto messageResult = resolveMessage(context);
            if (!messageResult.isSuccess()) {
                return SCXML::Common::Result<void>::failure("Failed to resolve log message: " +
                                                            messageResult.getError());
            }

            const std::string &message = messageResult.getValue();

            // Format log message with metadata
            std::string formattedMessage = formatLogMessage(message, context);

            // Output log message based on level
            auto &logger = context.getLogger();

            if (level_ == "error") {
                logger.error(formattedMessage);
            } else if (level_ == "warn" || level_ == "warning") {
                logger.warning(formattedMessage);
            } else if (level_ == "debug") {
                logger.debug(formattedMessage);
            } else {
                logger.info(formattedMessage);
            }

            return SCXML::Common::Result<void>::success();

        } catch (const std::exception &e) {
            return SCXML::Common::Result<void>::failure("Exception during log execution: " + std::string(e.what()));
        }
    }

    const std::string &LogNode::getExpr() const {
        return expr_;
    }

    void LogNode::setExpr(const std::string &expr) {
        expr_ = expr;
    }

    const std::string &LogNode::getLabel() const {
        return label_;
    }

    void LogNode::setLabel(const std::string &label) {
        label_ = label;
    }

    const std::string &LogNode::getLevel() const {
        return level_;
    }

    void LogNode::setLevel(const std::string &level) {
        level_ = level;
    }

    std::vector<std::string> LogNode::validate() const {
        std::vector<std::string> errors;

        // Expression is required
        if (expr_.empty()) {
            errors.push_back("Log expression is required");
        }

        // Log level must be valid
        if (!isValidLogLevel(level_)) {
            errors.push_back("Invalid log level: " + level_);
        }

        return errors;
    }

    std::shared_ptr<ILogNode> LogNode::clone() const {
        auto cloned = std::make_shared<LogNode>();
        cloned->expr_ = expr_;
        cloned->label_ = label_;
        cloned->level_ = level_;
        return cloned;
    }

    SCXML::Common::Result<std::string> LogNode::resolveMessage(SCXML::Runtime::RuntimeContext & context) const {
        try {
            EnhancedExpressionEvaluator evaluator;
            auto result = evaluator.evaluateStringExpression(context, expr_);

            if (!result.isSuccess()) {
                return SCXML::Common::Result<std::string>::failure("Failed to evaluate log expression: " +
                                                                   result.getError());
            }

            return SCXML::Common::Result<std::string>::success(result.getValue());

        } catch (const std::exception &e) {
            return SCXML::Common::Result<std::string>::failure("Exception resolving log message: " +
                                                               std::string(e.what()));
        }
    }

    bool LogNode::isValidLogLevel(const std::string &level) const {
        return level == "info" || level == "debug" || level == "warn" || level == "warning" || level == "error";
    }

    std::string LogNode::formatLogMessage(const std::string &message, SCXML::Runtime::RuntimeContext &context) const {
        std::stringstream formatted;

        // Add timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        formatted << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        formatted << "." << std::setfill('0') << std::setw(3) << ms.count();

        // Add level
        formatted << " [" << level_ << "]";

        // Add label if specified
        if (!label_.empty()) {
            formatted << " [" << label_ << "]";
        }

        // Add session ID if available
        if (context.getDataModel().hasVariable("_sessionid")) {
            auto sessionResult = context.getDataModel().getVariable("_sessionid");
            if (sessionResult.isSuccess()) {
                formatted << " [session:" << sessionResult.getValue() << "]";
            }
        }

        // Add message
        formatted << " " << message;

        return formatted.str();
    }

}  // namespace Core
}  // namespace SCXML
