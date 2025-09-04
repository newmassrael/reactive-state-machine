#pragma once

#include "common/SCXMLCommon.h"
#include <any>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
namespace Runtime {
class RuntimeContext;
}

namespace Core {

/**
 * @brief Data Model with ECMAScript support
 *
 * This class provides a comprehensive data model implementation supporting
 * ECMAScript expressions, JSON data manipulation, and data operations
 * as defined in W3C SCXML 1.0 specification.
 */
class DataModel {
public:
    /**
     * @brief Data types supported by the data model
     */
    enum class DataType { UNDEFINED, NULL_TYPE, BOOLEAN, NUMBER, STRING, OBJECT, ARRAY, FUNCTION };

    /**
     * @brief Represents a data value with type information
     */
    struct DataValue {
        DataType type;
        std::any value;

        DataValue() : type(DataType::UNDEFINED) {}

        DataValue(DataType t, std::any v) : type(t), value(std::move(v)) {}

        template <typename T> T get() const {
            return std::any_cast<T>(value);
        }

        bool isEmpty() const {
            return type == DataType::UNDEFINED;
        }

        std::string toString() const;
    };

    /**
     * @brief Constructor
     */
    DataModel();

    /**
     * @brief Destructor
     */
    virtual ~DataModel() = default;

    /**
     * @brief Initialize the data model
     * @param context Runtime context for initialization
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> initialize(SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Set a variable value
     * @param name Variable name
     * @param value Variable value
     * @param type Data type (optional, will be inferred if not specified)
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> setVariable(const std::string &name, const std::string &value,
                                            DataType type = DataType::UNDEFINED);

    /**
     * @brief Set a variable with DataValue
     * @param name Variable name
     * @param value DataValue containing type and value
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> setVariable(const std::string &name, const DataValue &value);

    /**
     * @brief Get a variable value
     * @param name Variable name
     * @return Result containing the variable value or error
     */
    SCXML::Common::Result<DataValue> getVariable(const std::string &name) const;

    /**
     * @brief Get a variable value as string
     * @param name Variable name
     * @return Result containing the string value or error
     */
    SCXML::Common::Result<std::string> getVariableAsString(const std::string &name) const;

    /**
     * @brief Check if a variable exists
     * @param name Variable name
     * @return True if variable exists
     */
    bool hasVariable(const std::string &name) const;

    /**
     * @brief Remove a variable
     * @param name Variable name
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> removeVariable(const std::string &name);

    /**
     * @brief Get all variable names
     * @return Vector of variable names
     */
    std::vector<std::string> getVariableNames() const;

    /**
     * @brief Get all variables as a map
     * @return Map of variable name to DataValue
     */
    std::map<std::string, DataValue> getAllVariables() const;

    /**
     * @brief Clear all variables
     */
    void clear();

    /**
     * @brief Evaluate ECMAScript expression
     * @param expression Expression to evaluate
     * @param context Runtime context for evaluation
     * @return Result containing evaluation result or error
     */
    SCXML::Common::Result<DataValue> evaluateExpression(const std::string &expression,
                                                        SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Assign value using location expression
     * @param location Location expression (e.g., "obj.prop", "arr[0]")
     * @param value Value to assign
     * @param context Runtime context
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> assignToLocation(const std::string &location, const DataValue &value,
                                                 SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get value using location expression
     * @param location Location expression
     * @param context Runtime context
     * @return Result containing the value or error
     */
    SCXML::Common::Result<DataValue> getFromLocation(const std::string &location,
                                                     SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Parse JSON string into DataValue
     * @param json JSON string
     * @return Result containing parsed DataValue or error
     */
    SCXML::Common::Result<DataValue> parseJSON(const std::string &json);

    /**
     * @brief Convert DataValue to JSON string
     * @param value DataValue to convert
     * @return Result containing JSON string or error
     */
    SCXML::Common::Result<std::string> toJSON(const DataValue &value);

    /**
     * @brief Create deep copy of a DataValue
     * @param value Value to clone
     * @return Cloned DataValue
     */
    DataValue cloneValue(const DataValue &value);

    /**
     * @brief Compare two DataValues for equality
     * @param left Left operand
     * @param right Right operand
     * @return True if values are equal
     */
    bool compareValues(const DataValue &left, const DataValue &right);

    /**
     * @brief Convert value to boolean following ECMAScript rules
     * @param value Value to convert
     * @return Boolean result
     */
    bool toBoolean(const DataValue &value);

    /**
     * @brief Convert value to number following ECMAScript rules
     * @param value Value to convert
     * @return Result containing number or error
     */
    SCXML::Common::Result<double> toNumber(const DataValue &value);

    /**
     * @brief Get system variables (_sessionid, _name, _ioprocessors)
     * @param context Runtime context
     * @return Map of system variables
     */
    std::map<std::string, DataValue> getSystemVariables(SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Set system variable
     * @param name System variable name
     * @param value Variable value
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> setSystemVariable(const std::string &name, const DataValue &value);

    /**
     * @brief Validate variable name according to ECMAScript rules
     * @param name Variable name to validate
     * @return True if name is valid
     */
    static bool isValidVariableName(const std::string &name);

    /**
     * @brief Get data model type identifier
     * @return "ecmascript" for ECMAScript data model
     */
    std::string getDataModelType() const {
        return "ecmascript";
    }

private:
    std::map<std::string, DataValue> variables_;   ///< User-defined variables
    std::map<std::string, DataValue> systemVars_;  ///< System variables

    /**
     * @brief Infer data type from string value
     * @param value String value
     * @return Inferred DataType
     */
    DataType inferDataType(const std::string &value);

    /**
     * @brief Parse string value according to inferred type
     * @param value String value
     * @param type Target type
     * @return Result containing parsed DataValue or error
     */
    SCXML::Common::Result<DataValue> parseValue(const std::string &value, DataType type);

    /**
     * @brief Initialize system variables
     * @param context Runtime context
     */
    void initializeSystemVariables(SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Parse location expression into components
     * @param location Location expression
     * @return Vector of location components
     */
    std::vector<std::string> parseLocationExpression(const std::string &location);

    /**
     * @brief Evaluate complex location path
     * @param components Location components
     * @param context Runtime context
     * @param forAssignment True if for assignment (create missing paths)
     * @return Result containing target value or error
     */
    SCXML::Common::Result<DataValue> evaluateLocationPath(const std::vector<std::string> &components,
                                                          SCXML::Runtime::RuntimeContext &context,
                                                          bool forAssignment = false);
};

} // namespace Model
}  // namespace SCXML
