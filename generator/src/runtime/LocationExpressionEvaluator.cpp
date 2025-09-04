#include "runtime/LocationExpressionEvaluator.h"
#include "common/Logger.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <regex>
#include <sstream>

namespace SCXML {

LocationExpressionEvaluator::LocationExpressionEvaluator() {
    Logger::debug("LocationExpressionEvaluator::Constructor - Initializing location expression evaluator");
}

LocationExpressionEvaluator::LocationPath
LocationExpressionEvaluator::parseLocationExpression(const std::string &expression) {
    Logger::debug("LocationExpressionEvaluator::parseLocationExpression - Parsing: " + expression);

    LocationPath path;
    path.originalExpression = expression;
    path.isValid = false;

    if (expression.empty()) {
        Logger::warning("LocationExpressionEvaluator::parseLocationExpression - Empty expression");
        return path;
    }

    try {
        // Tokenize the expression
        auto tokens = tokenizeExpression(expression);
        if (tokens.empty()) {
            Logger::warning("LocationExpressionEvaluator::parseLocationExpression - No tokens found");
            return path;
        }

        for (const auto &token : tokens) {
            // Check if this is a dynamic property access
            if (token.find('[') != std::string::npos && token.find(']') != std::string::npos) {
                // Extract base and dynamic part
                size_t bracketStart = token.find('[');
                std::string base = token.substr(0, bracketStart);
                std::string dynamicPart = token.substr(bracketStart + 1, token.find(']') - bracketStart - 1);

                if (!base.empty()) {
                    path.segments.push_back(base);
                    path.isDynamic.push_back(false);
                }

                path.segments.push_back(dynamicPart);
                path.isDynamic.push_back(true);
            } else {
                path.segments.push_back(token);
                path.isDynamic.push_back(false);
            }
        }

        path.isValid = !path.segments.empty();
        Logger::debug("LocationExpressionEvaluator::parseLocationExpression - Parsed " +
                      std::to_string(path.segments.size()) + " segments");

    } catch (const std::exception &e) {
        Logger::error("LocationExpressionEvaluator::parseLocationExpression - Error: " + std::string(e.what()));
        path.isValid = false;
    }

    return path;
}

SCXML::Common::Result<void> LocationExpressionEvaluator::assignValue(SCXML::Runtime::RuntimeContext &context,
                                                                     const std::string &locationExpr,
                                                                     const std::string &value) {
    Logger::debug("LocationExpressionEvaluator::assignValue - Assigning to: " + locationExpr + " = " + value);

    try {
        auto path = parseLocationExpression(locationExpr);
        if (!path.isValid) {
            return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::assignValue",
                                                                        "Invalid location expression: " + locationExpr,
                                                                        "Failed to parse location path"});
        }

        // Use complex assignment for multi-segment paths
        if (path.segments.size() > 1) {
            return executeComplexAssignment(context, path, value);
        }

        // Simple variable assignment
        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return SCXML::Common::Result<void>(
                SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::assignValue", "No data model engine available",
                                         "RuntimeContext must have an initialized data model engine"});
        }

        // Build assignment expression
        std::string assignmentExpr = locationExpr + " = " + value;
        auto result = dataModel->evaluateExpression(assignmentExpr);

        if (!result.isSuccess()) {
            return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::assignValue",
                                                                        "Assignment failed", result.getErrorMessage()});
        }

        Logger::debug("LocationExpressionEvaluator::assignValue - Assignment successful");
        return SCXML::Common::Result<void>();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>(
            SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::assignValue", "Assignment error", e.what()});
    }
}

SCXML::Common::Result<std::string> LocationExpressionEvaluator::getValue(SCXML::Runtime::RuntimeContext &context,
                                                                         const std::string &locationExpr) {
    Logger::debug("LocationExpressionEvaluator::getValue - Getting value from: " + locationExpr);

    try {
        auto path = parseLocationExpression(locationExpr);
        if (!path.isValid) {
            return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
                "LocationExpressionEvaluator::getValue", "Invalid location expression: " + locationExpr,
                "Failed to parse location path"});
        }

        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return SCXML::Common::Result<std::string>(
                SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::getValue", "No data model engine available",
                                         "RuntimeContext must have an initialized data model engine"});
        }

        // Evaluate the location expression
        auto result = dataModel->evaluateExpression(locationExpr);
        if (!result.isSuccess()) {
            return SCXML::Common::Result<std::string>(
                SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::getValue",
                                         "Failed to evaluate location: " + locationExpr, result.getErrorMessage()});
        }

        // Convert result to string
        std::string stringValue = dataModel->convertToString(result.value);
        Logger::debug("LocationExpressionEvaluator::getValue - Retrieved value: " + stringValue);

        return SCXML::Common::Result<std::string>(stringValue);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>(
            SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::getValue", "Error retrieving value", e.what()});
    }
}

bool LocationExpressionEvaluator::locationExists(SCXML::Runtime::RuntimeContext &context,
                                                 const std::string &locationExpr) {
    Logger::debug("LocationExpressionEvaluator::locationExists - Checking: " + locationExpr);

    try {
        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return false;
        }

        // Try to evaluate the location - if it succeeds, it exists
        auto result = dataModel->evaluateExpression(locationExpr);
        return result.isSuccess();

    } catch (const std::exception &e) {
        Logger::debug("LocationExpressionEvaluator::locationExists - Exception: " + std::string(e.what()));
        return false;
    }
}

SCXML::Common::Result<void> LocationExpressionEvaluator::createLocation(SCXML::Runtime::RuntimeContext &context,
                                                                        const std::string &locationExpr,
                                                                        const std::string &initialValue) {
    Logger::debug("LocationExpressionEvaluator::createLocation - Creating: " + locationExpr + " = " + initialValue);

    // If location already exists, just assign the value
    if (locationExists(context, locationExpr)) {
        return assignValue(context, locationExpr, initialValue);
    }

    try {
        auto path = parseLocationExpression(locationExpr);
        if (!path.isValid) {
            return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::createLocation",
                                                                        "Invalid location expression: " + locationExpr,
                                                                        "Failed to parse location path"});
        }

        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{
                "LocationExpressionEvaluator::createLocation", "No data model engine available",
                "RuntimeContext must have an initialized data model engine"});
        }

        // For complex paths, ensure parent objects exist
        if (path.segments.size() > 1) {
            std::string parentPath;
            for (size_t i = 0; i < path.segments.size() - 1; ++i) {
                if (i > 0) {
                    parentPath += ".";
                }
                parentPath += path.segments[i];

                if (!locationExists(context, parentPath)) {
                    // Create parent object
                    std::string createParentExpr = parentPath + " = {}";
                    auto result = dataModel->evaluateExpression(createParentExpr);
                    if (!result.isSuccess()) {
                        return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{
                            "LocationExpressionEvaluator::createLocation",
                            "Failed to create parent object: " + parentPath, result.getErrorMessage()});
                    }
                }
            }
        }

        // Now create the final location
        return assignValue(context, locationExpr, initialValue);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::createLocation",
                                                                    "Error creating location", e.what()});
    }
}

SCXML::Common::Result<void> LocationExpressionEvaluator::deleteLocation(SCXML::Runtime::RuntimeContext &context,
                                                                        const std::string &locationExpr) {
    Logger::debug("LocationExpressionEvaluator::deleteLocation - Deleting: " + locationExpr);

    try {
        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{
                "LocationExpressionEvaluator::deleteLocation", "No data model engine available",
                "RuntimeContext must have an initialized data model engine"});
        }

        // Use delete operator for object properties or set to undefined for variables
        auto path = parseLocationExpression(locationExpr);
        std::string deleteExpr;

        if (path.segments.size() > 1) {
            deleteExpr = "delete " + locationExpr;
        } else {
            deleteExpr = locationExpr + " = undefined";
        }

        auto result = dataModel->evaluateExpression(deleteExpr);
        if (!result.isSuccess()) {
            return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::deleteLocation",
                                                                        "Failed to delete location: " + locationExpr,
                                                                        result.getErrorMessage()});
        }

        Logger::debug("LocationExpressionEvaluator::deleteLocation - Successfully deleted");
        return SCXML::Common::Result<void>();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::deleteLocation",
                                                                    "Error deleting location", e.what()});
    }
}

std::vector<std::string> LocationExpressionEvaluator::validateLocationExpression(const std::string &expression) {
    std::vector<std::string> errors;

    if (expression.empty()) {
        errors.push_back("Location expression cannot be empty");
        return errors;
    }

    // Check for balanced brackets
    int bracketCount = 0;
    for (char c : expression) {
        if (c == '[') {
            bracketCount++;
        } else if (c == ']') {
            bracketCount--;
        }
        if (bracketCount < 0) {
            errors.push_back("Unmatched closing bracket in location expression");
            break;
        }
    }
    if (bracketCount > 0) {
        errors.push_back("Unmatched opening bracket in location expression");
    }

    // Check for invalid characters
    if (expression.find("..") != std::string::npos) {
        errors.push_back("Double dots (..) not allowed in location expression");
    }

    // Check for valid identifier start
    if (!std::isalpha(expression[0]) && expression[0] != '_' && expression[0] != '$') {
        errors.push_back("Location expression must start with letter, underscore, or dollar sign");
    }

    return errors;
}

std::vector<std::string> LocationExpressionEvaluator::getChildLocations(SCXML::Runtime::RuntimeContext &context,
                                                                        const std::string &locationExpr) {
    std::vector<std::string> children;

    try {
        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return children;
        }

        // Use Object.keys() to get child properties
        std::string keysExpr = "Object.keys(" + locationExpr + ")";
        auto result = dataModel->evaluateExpression(keysExpr);

        if (result.isSuccess()) {
            // Parse the array result to extract child names
            std::string keysString = dataModel->convertToString(result.value);
            // This would need more sophisticated parsing for a full implementation
            Logger::debug("LocationExpressionEvaluator::getChildLocations - Found keys: " + keysString);
        }

    } catch (const std::exception &e) {
        Logger::debug("LocationExpressionEvaluator::getChildLocations - Exception: " + std::string(e.what()));
    }

    return children;
}

std::vector<std::string> LocationExpressionEvaluator::tokenizeExpression(const std::string &expression) {
    std::vector<std::string> tokens;

    // Simple tokenization based on dots, but preserve array brackets
    std::string current;
    bool inBrackets = false;

    for (char c : expression) {
        if (c == '[') {
            inBrackets = true;
            current += c;
        } else if (c == ']') {
            inBrackets = false;
            current += c;
        } else if (c == '.' && !inBrackets) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

int LocationExpressionEvaluator::parseArrayIndex(const std::string &token, SCXML::Runtime::RuntimeContext &context) {
    size_t bracketStart = token.find('[');
    if (bracketStart == std::string::npos) {
        return -1;  // Not array access
    }

    size_t bracketEnd = token.find(']', bracketStart);
    if (bracketEnd == std::string::npos) {
        return -1;  // Malformed
    }

    std::string indexStr = token.substr(bracketStart + 1, bracketEnd - bracketStart - 1);

    try {
        // Try to parse as integer first
        return std::stoi(indexStr);
    } catch (const std::exception &) {
        // Could be a variable name - would need to evaluate
        return -1;
    }
}

SCXML::Common::Result<std::string>
LocationExpressionEvaluator::resolveDynamicProperty(SCXML::Runtime::RuntimeContext &context,
                                                    const std::string &dynamicExpr) {
    try {
        auto *dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
                "LocationExpressionEvaluator::resolveDynamicProperty", "No data model engine available",
                "RuntimeContext must have an initialized data model engine"});
        }

        auto result = dataModel->evaluateExpression(dynamicExpr);
        if (!result.isSuccess()) {
            return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
                "LocationExpressionEvaluator::resolveDynamicProperty",
                "Failed to resolve dynamic property: " + dynamicExpr, result.getErrorMessage()});
        }

        std::string resolvedName = dataModel->convertToString(result.value);
        return SCXML::Common::Result<std::string>(resolvedName);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
            "LocationExpressionEvaluator::resolveDynamicProperty", "Error resolving dynamic property", e.what()});
    }
}

SCXML::Common::Result<void> LocationExpressionEvaluator::navigateToParent(SCXML::Runtime::RuntimeContext &context,
                                                                          const LocationPath &path) {
    // This would implement navigation to parent objects in the data model
    // For now, just validate that the path is reasonable
    if (path.segments.empty()) {
        return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{"LocationExpressionEvaluator::navigateToParent",
                                                                    "Cannot navigate - empty path",
                                                                    "Path must have at least one segment"});
    }

    return SCXML::Common::Result<void>();
}

SCXML::Common::Result<void>
LocationExpressionEvaluator::executeComplexAssignment(SCXML::Runtime::RuntimeContext &context, const LocationPath &path,
                                                      const std::string &value) {
    Logger::debug("LocationExpressionEvaluator::executeComplexAssignment - Complex assignment for " +
                  path.originalExpression);

    try {
        // For complex paths, we need to ensure all parent objects exist
        // and then perform the assignment

        // This is a simplified implementation - a full version would handle
        // dynamic property resolution, array index evaluation, etc.

        return assignValue(context, path.originalExpression, value);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{
            "LocationExpressionEvaluator::executeComplexAssignment", "Complex assignment failed", e.what()});
    }
}

}  // namespace SCXML