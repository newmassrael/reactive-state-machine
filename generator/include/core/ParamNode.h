#pragma once
#include "common/SCXMLCommon.h"
#include "model/IParamNode.h"
#include <string>
#include <vector>

using SCXML::Model::IParamNode;


// Forward declarations
namespace SCXML {
namespace Runtime {
class RuntimeContext;
}

namespace Core {

/**
 * @brief Implementation of SCXML <param> element
 *
 * The ParamNode represents a name-value parameter that can be passed
 * to events, invocations, or included in data outputs. It supports
 * multiple ways of specifying the value: literal, expression, or
 * data model location reference.
 *
 * Features:
 * - Required name attribute
 * - Value specification via expr, location, or literal value
 * - Runtime value evaluation and data model integration
 * - Validation of parameter configuration
 *
 * SCXML W3C Specification compliance:
 * - Supports name attribute (required)
 * - Supports expr attribute for dynamic values
 * - Supports location attribute for data model references
 * - Proper precedence: location > expr > literal value
 */
class ParamNode : public IParamNode {
private:
    std::string id_;          ///< Optional identifier
    std::string name_;        ///< Parameter name (required)
    std::string value_;       ///< Literal value
    std::string expression_;  ///< Expression for dynamic value
    std::string location_;    ///< Data model location reference

    // Value source priority flags
    bool hasExpression_;  ///< True if using expression
    bool hasLocation_;    ///< True if using location reference

public:
    /**
     * @brief Constructor
     * @param name Parameter name (required)
     * @param id Optional identifier for the parameter node
     */
    explicit ParamNode(const std::string &name, const std::string &id = "");

    /**
     * @brief Destructor
     */
    virtual ~ParamNode() = default;

    // IDataNode interface
    bool initialize(SCXML::Model::IExecutionContext &context) override;
    std::vector<std::string> validate() const override;
    std::shared_ptr<Model::IDataNode> clone() const override;

    // IParamNode interface
    const std::string &getName() const override;
    std::string getValue(SCXML::Model::IExecutionContext &context) const override;
    void setName(const std::string &name) override;
    void setValue(const std::string &value) override;
    void setExpression(const std::string &expr) override;
    void setLocation(const std::string &location) override;
    bool hasExpression() const override;
    bool hasLocation() const override;
    SCXML::Common::Result<void> processParameter(SCXML::Model::IExecutionContext &context,
                                                 const std::string &eventName = "") const override;

    // Additional methods
    /**
     * @brief Get the node identifier
     * @return The node ID
     */
    const std::string &getId() const;

    /**
     * @brief Get the raw literal value (without evaluation)
     * @return The literal value string
     */
    const std::string &getLiteralValue() const;

    /**
     * @brief Get the expression (without evaluation)
     * @return The expression string
     */
    const std::string &getExpression() const;

    /**
     * @brief Get the location reference
     * @return The location reference string
     */
    const std::string &getLocation() const;

    /**
     * @brief Get the effective value source being used
     * @return String indicating source: "location", "expression", "literal", or "none"
     */
    std::string getValueSource() const;

    /**
     * @brief Clear all values and expressions
     */
    void clear();
};

} // namespace Core
}  // namespace SCXML
