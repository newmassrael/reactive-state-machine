#pragma once

#include "common/Result.h"
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
// Forward declarations
namespace Runtime {
class RuntimeContext;
}
class DataModelEngine;

/**
 * @brief Enhanced location expression evaluator for complex data paths
 *
 * Supports complex location expressions for data assignment and manipulation:
 * - Simple variables: "myVar"
 * - Object properties: "obj.property"
 * - Array elements: "arr[0]", "arr[index]"
 * - Nested paths: "obj.arr[0].property"
 * - Dynamic properties: "obj[dynamicKey]"
 */
class LocationExpressionEvaluator {
public:
    struct LocationPath {
        std::vector<std::string> segments;  ///< Path segments
        std::vector<bool> isDynamic;        ///< Whether each segment is dynamic
        bool isValid;                       ///< Whether path is valid
        std::string originalExpression;     ///< Original expression string
    };

    /**
     * @brief Constructor
     */
    LocationExpressionEvaluator();

    /**
     * @brief Destructor
     */
    ~LocationExpressionEvaluator() = default;

    /**
     * @brief Parse a location expression into path components
     * @param expression Location expression string
     * @return Parsed location path
     */
    LocationPath parseLocationExpression(const std::string &expression);

    /**
     * @brief Assign a value to a location expression
     * @param context Runtime context
     * @param locationExpr Location expression
     * @param value Value to assign
     * @return Result of assignment operation
     */
    SCXML::Common::Result<void> assignValue(SCXML::Runtime::RuntimeContext &context, const std::string &locationExpr,
                                            const std::string &value);

    /**
     * @brief Get value from a location expression
     * @param context Runtime context
     * @param locationExpr Location expression
     * @return Result containing the value
     */
    SCXML::Common::Result<std::string> getValue(SCXML::Runtime::RuntimeContext &context,
                                                const std::string &locationExpr);

    /**
     * @brief Check if a location expression exists/is valid
     * @param context Runtime context
     * @param locationExpr Location expression
     * @return true if location exists
     */
    bool locationExists(SCXML::Runtime::RuntimeContext &context, const std::string &locationExpr);

    /**
     * @brief Create a location if it doesn't exist
     * @param context Runtime context
     * @param locationExpr Location expression
     * @param initialValue Initial value for created location
     * @return Result of creation operation
     */
    SCXML::Common::Result<void> createLocation(SCXML::Runtime::RuntimeContext &context, const std::string &locationExpr,
                                               const std::string &initialValue = "");

    /**
     * @brief Delete a location
     * @param context Runtime context
     * @param locationExpr Location expression
     * @return Result of deletion operation
     */
    SCXML::Common::Result<void> deleteLocation(SCXML::Runtime::RuntimeContext &context,
                                               const std::string &locationExpr);

    /**
     * @brief Validate a location expression syntax
     * @param expression Location expression
     * @return Validation errors (empty if valid)
     */
    std::vector<std::string> validateLocationExpression(const std::string &expression);

    /**
     * @brief Get all child locations of a given location
     * @param context Runtime context
     * @param locationExpr Parent location expression
     * @return List of child location names
     */
    std::vector<std::string> getChildLocations(SCXML::Runtime::RuntimeContext &context,
                                               const std::string &locationExpr);

private:
    /**
     * @brief Tokenize a location expression
     * @param expression Expression to tokenize
     * @return Vector of tokens
     */
    std::vector<std::string> tokenizeExpression(const std::string &expression);

    /**
     * @brief Parse array index from token
     * @param token Token potentially containing array index
     * @return Array index or -1 if not array access
     */
    int parseArrayIndex(const std::string &token, SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Resolve dynamic property names
     * @param context Runtime context
     * @param dynamicExpr Dynamic property expression
     * @return Resolved property name
     */
    SCXML::Common::Result<std::string> resolveDynamicProperty(SCXML::Runtime::RuntimeContext &context,
                                                              const std::string &dynamicExpr);

    /**
     * @brief Navigate to parent location of given path
     * @param context Runtime context
     * @param path Location path
     * @return Result containing parent location info
     */
    SCXML::Common::Result<void> navigateToParent(SCXML::Runtime::RuntimeContext &context, const LocationPath &path);

    /**
     * @brief Execute complex assignment with type conversion
     * @param context Runtime context
     * @param path Location path
     * @param value Value to assign
     * @return Assignment result
     */
    SCXML::Common::Result<void> executeComplexAssignment(SCXML::Runtime::RuntimeContext &context,
                                                         const LocationPath &path, const std::string &value);
};
}  // namespace SCXML
