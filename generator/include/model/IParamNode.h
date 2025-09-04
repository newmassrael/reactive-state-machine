#pragma once
#include "IExecutionContext.h"
#include "common/SCXMLCommon.h"
#include "model/IDataNode.h"
#include <string>
#include <vector>

namespace SCXML {
namespace Model {

// Forward declarations

/**
 * @brief Interface for SCXML <param> element nodes
 *
 * The <param> element is used to specify name-value pairs that can be
 * included in events, donedata, or send operations. It supports both
 * literal values and dynamic expression evaluation.
 *
 * SCXML W3C Specification:
 * - Must have 'name' attribute
 * - Can have 'expr' attribute for dynamic values
 * - Can have 'location' attribute for referencing data model values
 * - Used within <send>, <donedata>, <invoke>, etc.
 */
class IParamNode : public IDataNode {
public:
    virtual ~IParamNode() = default;

    /**
     * @brief Get the parameter name
     * @return The name of the parameter
     */
    virtual const std::string &getName() const = 0;

    /**
     * @brief Get the parameter value (evaluated if expression)
     * @param context Execution context for expression evaluation
     * @return The parameter value as string
     */
    virtual std::string getValue(SCXML::Model::IExecutionContext &context) const = 0;

    /**
     * @brief Set the parameter name
     * @param name The parameter name
     */
    virtual void setName(const std::string &name) = 0;

    /**
     * @brief Set a literal value for the parameter
     * @param value The literal value
     */
    virtual void setValue(const std::string &value) = 0;

    /**
     * @brief Set an expression for dynamic value evaluation
     * @param expr The expression to evaluate
     */
    virtual void setExpression(const std::string &expr) = 0;

    /**
     * @brief Set a location reference in the data model
     * @param location The data model location reference
     */
    virtual void setLocation(const std::string &location) = 0;

    /**
     * @brief Check if this parameter uses an expression
     * @return True if using expression for value
     */
    virtual bool hasExpression() const = 0;

    /**
     * @brief Check if this parameter uses a location reference
     * @return True if using location reference for value
     */
    virtual bool hasLocation() const = 0;

    /**
     * @brief Process this parameter and add it to the given context/event
     * @param context Execution context
     * @param eventName Optional event name for event data
     * @return Success/failure result
     */
    virtual SCXML::Common::Result<void> processParameter(SCXML::Model::IExecutionContext &context,
                                                         const std::string &eventName = "") const = 0;
};

}  // namespace Model
}  // namespace SCXML
