#pragma once
#include "IExecutionContext.h"
#include "common/SCXMLCommon.h"
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
namespace Model {

// Forward declarations

/**
 * @brief Interface for SCXML <foreach> element implementation
 *
 * The <foreach> element allows iteration over arrays or collections
 * in the data model, executing child elements for each item.
 */
class IForeachNode {
public:
    virtual ~IForeachNode() = default;

    /**
     * @brief Execute the foreach operation
     * @param context Execution context containing state machine state
     * @return Result indicating success or failure with error details
     */
    virtual SCXML::Common::Result<void> execute(IExecutionContext &context) = 0;

    /**
     * @brief Get the array expression to iterate over
     * @return Array expression or location
     */
    virtual const std::string &getArray() const = 0;

    /**
     * @brief Set the array expression to iterate over
     * @param array Array expression or location
     */
    virtual void setArray(const std::string &array) = 0;

    /**
     * @brief Get the item variable name
     * @return Variable name that will hold each item during iteration
     */
    virtual const std::string &getItem() const = 0;

    /**
     * @brief Set the item variable name
     * @param item Variable name for each item
     */
    virtual void setItem(const std::string &item) = 0;

    /**
     * @brief Get the index variable name
     * @return Variable name that will hold the current index (optional)
     */
    virtual const std::string &getIndex() const = 0;

    /**
     * @brief Set the index variable name
     * @param index Variable name for current index
     */
    virtual void setIndex(const std::string &index) = 0;

    /**
     * @brief Add a child executable element
     * @param child Child element to execute for each item
     */
    virtual void addChild(std::shared_ptr<void> child) = 0;

    /**
     * @brief Get all child elements
     * @return Vector of child elements
     */
    virtual std::vector<std::shared_ptr<void>> getChildren() const = 0;

    /**
     * @brief Validate the foreach node configuration
     * @return List of validation errors, empty if valid
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Clone this foreach node
     * @return Deep copy of this foreach node
     */
    virtual std::shared_ptr<IForeachNode> clone() const = 0;

    /**
     * @brief Get the resolved array to iterate over
     * @param context Execution context for expression evaluation
     * @return Result containing array items or error
     */
    virtual SCXML::Common::Result<std::vector<std::string>>
    resolveArray(IExecutionContext &context) const = 0;
};

/**
 * @brief Interface for SCXML <log> element implementation
 *
 * The <log> element is used for logging and debugging purposes,
 * allowing state machines to output diagnostic information.
 */
class ILogNode {
public:
    virtual ~ILogNode() = default;

    /**
     * @brief Execute the log operation
     * @param context Execution context containing state machine state
     * @return Result indicating success or failure with error details
     */
    virtual SCXML::Common::Result<void> execute(IExecutionContext &context) = 0;

    /**
     * @brief Get the log message or expression
     * @return Message string or expression that evaluates to message
     */
    virtual const std::string &getExpr() const = 0;

    /**
     * @brief Set the log message or expression
     * @param expr Message string or expression
     */
    virtual void setExpr(const std::string &expr) = 0;

    /**
     * @brief Get the log label
     * @return Label string for categorizing log messages
     */
    virtual const std::string &getLabel() const = 0;

    /**
     * @brief Set the log label
     * @param label Label string for categorizing log messages
     */
    virtual void setLabel(const std::string &label) = 0;

    /**
     * @brief Get the log level
     * @return Log level (info, debug, warn, error)
     */
    virtual const std::string &getLevel() const = 0;

    /**
     * @brief Set the log level
     * @param level Log level
     */
    virtual void setLevel(const std::string &level) = 0;

    /**
     * @brief Validate the log node configuration
     * @return List of validation errors, empty if valid
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Clone this log node
     * @return Deep copy of this log node
     */
    virtual std::shared_ptr<ILogNode> clone() const = 0;

    /**
     * @brief Get the resolved log message
     * @param context Execution context for expression evaluation
     * @return Result containing resolved message or error
     */
    virtual SCXML::Common::Result<std::string> resolveMessage(IExecutionContext &context) const = 0;
};

}  // namespace Model
}  // namespace SCXML
