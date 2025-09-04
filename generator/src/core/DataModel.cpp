#include "core/DataModel.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <regex>
#include <sstream>

namespace SCXML {
namespace Core {

std::string DataModel::DataValue::toString() const {
    switch (type) {
    case DataType::UNDEFINED:
        return "undefined";
    case DataType::NULL_TYPE:
        return "null";
    case DataType::BOOLEAN:
        return std::any_cast<bool>(value) ? "true" : "false";
    case DataType::NUMBER:
        return std::to_string(std::any_cast<double>(value));
    case DataType::STRING:
        return std::any_cast<std::string>(value);
    case DataType::OBJECT:
        return "[object Object]";
    case DataType::ARRAY:
        return "[array Array]";
    case DataType::FUNCTION:
        return "[function Function]";
    default:
        return "unknown";
    }
}

DataModel::DataModel() {
    // Initialize with empty state
}

SCXML::Common::Result<void> DataModel::initialize(SCXML::Runtime::RuntimeContext &context) {
    try {
        // Clear existing variables
        variables_.clear();
        systemVars_.clear();

        // Initialize system variables
        initializeSystemVariables(context);

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to initialize data model: " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> DataModel::setVariable(const std::string &name, const std::string &value, DataType type) {
    if (!isValidVariableName(name)) {
        return SCXML::Common::Result<void>::failure("Invalid variable name: " + name);
    }

    try {
        // Infer type if not specified
        if (type == DataType::UNDEFINED) {
            type = inferDataType(value);
        }

        // Parse value according to type
        auto parseResult = parseValue(value, type);
        if (!parseResult.isSuccess()) {
            return SCXML::Common::Result<void>::failure("Parse error");
        }

        variables_[name] = parseResult.getValue();
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to set variable '" + name + "': " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> DataModel::setVariable(const std::string &name, const DataValue &value) {
    if (!isValidVariableName(name)) {
        return SCXML::Common::Result<void>::failure("Invalid variable name: " + name);
    }

    variables_[name] = value;
    return SCXML::Common::Result<void>::success();
}

SCXML::Common::Result<DataModel::DataValue> DataModel::getVariable(const std::string &name) const {
    // Check user variables first
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return SCXML::Common::Result<DataValue>::success(it->second);
    }

    // Check system variables
    auto sysIt = systemVars_.find(name);
    if (sysIt != systemVars_.end()) {
        return SCXML::Common::Result<DataValue>::success(sysIt->second);
    }

    // Variable not found
    return SCXML::Common::Result<DataValue>::failure("Variable not found: " + name);
}

SCXML::Common::Result<std::string> DataModel::getVariableAsString(const std::string &name) const {
    auto result = getVariable(name);
    if (!result.isSuccess()) {
        return SCXML::Common::Result<std::string>::failure("Variable not found");
    }

    return SCXML::Common::Result<std::string>::success(result.getValue().toString());
}

bool DataModel::hasVariable(const std::string &name) const {
    return variables_.count(name) > 0 || systemVars_.count(name) > 0;
}

SCXML::Common::Result<void> DataModel::removeVariable(const std::string &name) {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        variables_.erase(it);
        return SCXML::Common::Result<void>::success();
    }

    return SCXML::Common::Result<void>::failure("Variable not found: " + name);
}

std::vector<std::string> DataModel::getVariableNames() const {
    std::vector<std::string> names;

    for (const auto &pair : variables_) {
        names.push_back(pair.first);
    }

    for (const auto &pair : systemVars_) {
        names.push_back(pair.first);
    }

    return names;
}

std::map<std::string, DataModel::DataValue> DataModel::getAllVariables() const {
    std::map<std::string, DataValue> allVars = variables_;

    // Add system variables
    for (const auto &pair : systemVars_) {
        allVars[pair.first] = pair.second;
    }

    return allVars;
}

void DataModel::clear() {
    variables_.clear();
}

SCXML::Common::Result<DataModel::DataValue> DataModel::evaluateExpression(const std::string &expression,
                                                                          SCXML::Runtime::RuntimeContext &context) {
    try {
        // Simple expression evaluation (basic implementation)
        // In a full implementation, this would use a JavaScript engine

        // Handle simple variable references
        if (hasVariable(expression)) {
            return getVariable(expression);
        }

        // Handle simple literals
        if (expression == "true") {
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::BOOLEAN, true));
        }
        if (expression == "false") {
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::BOOLEAN, false));
        }
        if (expression == "null") {
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::NULL_TYPE, nullptr));
        }
        if (expression == "undefined") {
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::UNDEFINED, nullptr));
        }

        // Handle numeric literals
        std::regex numberRegex(R"(-?\d+(?:\.\d+)?)");
        if (std::regex_match(expression, numberRegex)) {
            double value = std::stod(expression);
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::NUMBER, value));
        }

        // Handle string literals
        std::regex stringRegex(R"(["'](.*)["'])");
        std::smatch match;
        if (std::regex_match(expression, match, stringRegex)) {
            std::string value = match[1].str();
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::STRING, value));
        }

        // Handle location expressions (e.g., obj.prop)
        if (expression.find('.') != std::string::npos || expression.find('[') != std::string::npos) {
            return getFromLocation(expression, context);
        }

        // Default: treat as string literal
        return SCXML::Common::Result<DataValue>::success(DataValue(DataType::STRING, expression));

    } catch (const std::exception &e) {
        return SCXML::Common::Result<DataValue>::failure("Failed to evaluate expression '" + expression +
                                                         "': " + std::string(e.what()));
    }
}

SCXML::Common::Result<void> DataModel::assignToLocation(const std::string &location, const DataValue &value,
                                                        SCXML::Runtime::RuntimeContext & /* context */) {
    try {
        // Parse location expression
        auto components = parseLocationExpression(location);
        if (components.empty()) {
            return SCXML::Common::Result<void>::failure("Invalid location expression: " + location);
        }

        // Simple assignment to variable
        if (components.size() == 1) {
            return setVariable(components[0], value);
        }

        // Complex location assignment (simplified implementation)
        // In full implementation, would handle nested object/array assignments
        std::string varName = components[0];
        if (!hasVariable(varName)) {
            // Create empty object
            DataValue emptyObj(DataType::OBJECT, std::map<std::string, DataValue>());
            setVariable(varName, emptyObj);
        }

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to assign to location '" + location +
                                                    "': " + std::string(e.what()));
    }
}

SCXML::Common::Result<DataModel::DataValue> DataModel::getFromLocation(const std::string &location,
                                                                       SCXML::Runtime::RuntimeContext & /* context */) {
    try {
        // Parse location expression
        auto components = parseLocationExpression(location);
        if (components.empty()) {
            return SCXML::Common::Result<DataValue>::failure("Invalid location expression: " + location);
        }

        // Simple variable access
        if (components.size() == 1) {
            return getVariable(components[0]);
        }

        // Complex location access (simplified implementation)
        std::string varName = components[0];
        auto varResult = getVariable(varName);
        if (!varResult.isSuccess()) {
            return varResult;
        }

        // For now, return the base variable
        return varResult;

    } catch (const std::exception &e) {
        return SCXML::Common::Result<DataValue>::failure("Failed to get from location '" + location +
                                                         "': " + std::string(e.what()));
    }
}

SCXML::Common::Result<DataModel::DataValue> DataModel::parseJSON(const std::string &json) {
    try {
        // Simple JSON parsing (basic implementation)
        std::string trimmed = json;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

        if (trimmed.empty()) {
            return SCXML::Common::Result<DataValue>::failure("Empty JSON string");
        }

        if (trimmed == "null") {
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::NULL_TYPE, nullptr));
        }

        if (trimmed == "true") {
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::BOOLEAN, true));
        }

        if (trimmed == "false") {
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::BOOLEAN, false));
        }

        // String
        if (trimmed.front() == '"' && trimmed.back() == '"') {
            std::string value = trimmed.substr(1, trimmed.length() - 2);
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::STRING, value));
        }

        // Number
        std::regex numberRegex(R"(-?\d+(?:\.\d+)?)");
        if (std::regex_match(trimmed, numberRegex)) {
            double value = std::stod(trimmed);
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::NUMBER, value));
        }

        // Object or Array (simplified - just mark as object/array type)
        if (trimmed.front() == '{' && trimmed.back() == '}') {
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::OBJECT, trimmed));
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::ARRAY, trimmed));
        }

        return SCXML::Common::Result<DataValue>::failure("Invalid JSON: " + json);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<DataValue>::failure("Failed to parse JSON: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::string> DataModel::toJSON(const DataValue &value) {
    try {
        switch (value.type) {
        case DataType::NULL_TYPE:
            return SCXML::Common::Result<std::string>::success("null");
        case DataType::BOOLEAN:
            return SCXML::Common::Result<std::string>::success(std::any_cast<bool>(value.value) ? "true" : "false");
        case DataType::NUMBER:
            return SCXML::Common::Result<std::string>::success(std::to_string(std::any_cast<double>(value.value)));
        case DataType::STRING: {
            std::string str = std::any_cast<std::string>(value.value);
            return SCXML::Common::Result<std::string>::success("\"" + str + "\"");
        }
        case DataType::OBJECT:
        case DataType::ARRAY:
            // Return stored JSON representation
            return SCXML::Common::Result<std::string>::success(std::any_cast<std::string>(value.value));
        case DataType::UNDEFINED:
        default:
            return SCXML::Common::Result<std::string>::success("null");
        }
    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::failure("Failed to convert to JSON: " + std::string(e.what()));
    }
}

DataModel::DataType DataModel::inferDataType(const std::string &value) {
    if (value == "null") {
        return DataType::NULL_TYPE;
    }
    if (value == "undefined") {
        return DataType::UNDEFINED;
    }
    if (value == "true" || value == "false") {
        return DataType::BOOLEAN;
    }

    // Check if numeric
    std::regex numberRegex(R"(-?\d+(?:\.\d+)?)");
    if (std::regex_match(value, numberRegex)) {
        return DataType::NUMBER;
    }

    // Check if JSON object/array
    if (!value.empty()) {
        if (value.front() == '{' && value.back() == '}') {
            return DataType::OBJECT;
        }
        if (value.front() == '[' && value.back() == ']') {
            return DataType::ARRAY;
        }
    }

    // Default to string
    return DataType::STRING;
}

SCXML::Common::Result<DataModel::DataValue> DataModel::parseValue(const std::string &value, DataType type) {
    try {
        switch (type) {
        case DataType::NULL_TYPE:
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::NULL_TYPE, nullptr));
        case DataType::UNDEFINED:
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::UNDEFINED, nullptr));
        case DataType::BOOLEAN: {
            bool boolValue = (value == "true");
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::BOOLEAN, boolValue));
        }
        case DataType::NUMBER: {
            double numValue = std::stod(value);
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::NUMBER, numValue));
        }
        case DataType::STRING:
            return SCXML::Common::Result<DataValue>::success(DataValue(DataType::STRING, value));
        case DataType::OBJECT:
        case DataType::ARRAY:
            return SCXML::Common::Result<DataValue>::success(DataValue(type, value));
        default:
            return SCXML::Common::Result<DataValue>::failure("Unknown data type");
        }
    } catch (const std::exception &e) {
        return SCXML::Common::Result<DataValue>::failure("Failed to parse value: " + std::string(e.what()));
    }
}

void DataModel::initializeSystemVariables(SCXML::Runtime::RuntimeContext & /* context */) {
    // _sessionid - unique session identifier
    systemVars_["_sessionid"] =
        DataValue(DataType::STRING, "session_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                    std::chrono::steady_clock::now().time_since_epoch())
                                                                    .count()));

    // _name - state machine name
    systemVars_["_name"] = DataValue(DataType::STRING, "scxml");

    // _ioprocessors - available I/O processors
    systemVars_["_ioprocessors"] =
        DataValue(DataType::OBJECT,
                  R"({"scxml": {"location": "#_scxml_internal"}, "basichttp": {"location": "http://localhost:8080"}})");
}

std::vector<std::string> DataModel::parseLocationExpression(const std::string &location) {
    std::vector<std::string> components;

    // Simple dot notation parsing
    std::stringstream ss(location);
    std::string component;

    while (std::getline(ss, component, '.')) {
        if (!component.empty()) {
            components.push_back(component);
        }
    }

    return components;
}

bool DataModel::isValidVariableName(const std::string &name) {
    if (name.empty()) {
        return false;
    }

    // Must start with letter or underscore
    if (!std::isalpha(name[0]) && name[0] != '_') {
        return false;
    }

    // Rest must be alphanumeric or underscore
    for (size_t i = 1; i < name.length(); ++i) {
        if (!std::isalnum(name[i]) && name[i] != '_') {
            return false;
        }
    }

    return true;
}

}  // namespace Core
}  // namespace SCXML