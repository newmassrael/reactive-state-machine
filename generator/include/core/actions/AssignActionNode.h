#pragma once
#include "../../common/Result.h"
#include "../ActionNode.h"
#include <memory>

namespace SCXML {
namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {
class RuntimeContext;
}

namespace Core {

/**
 * @brief SCXML <assign> action implementation
 *
 * The <assign> element is used to modify data model values. This is a fundamental
 * action for maintaining state machine data and variables during transitions.
 */
class AssignActionNode : public ActionNode {
public:
    /**
     * @brief Construct a new Assign Action Node
     * @param id Action identifier
     */
    explicit AssignActionNode(const std::string &id);

    /**
     * @brief Destructor
     */
    virtual ~AssignActionNode() = default;

    /**
     * @brief Set the location (variable path) to assign to
     * @param location Data model location (e.g., "myVar", "data.field")
     */
    void setLocation(const std::string &location);

    /**
     * @brief Get the assignment location
     * @return location string
     */
    const std::string &getLocation() const {
        return location_;
    }

    /**
     * @brief Set the expression to evaluate and assign
     * @param expr Expression to evaluate (e.g., "5", "otherVar + 1")
     */
    void setExpr(const std::string &expr);

    /**
     * @brief Get the assignment expression
     * @return expression string
     */
    const std::string &getExpr() const {
        return expr_;
    }

    /**
     * @brief Set assignment attribute (alternative to expr)
     * @param attr Attribute name to read value from
     */
    void setAttr(const std::string &attr);

    /**
     * @brief Get the assignment attribute
     * @return attribute string
     */
    const std::string &getAttr() const {
        return attr_;
    }

    /**
     * @brief Set assignment type (optional, for type safety)
     * @param type Data type hint ("string", "number", "boolean", etc.)
     */
    void setType(const std::string &type);

    /**
     * @brief Get the assignment type
     * @return type string
     */
    const std::string &getType() const {
        return type_;
    }

    /**
     * @brief Get action type name
     * @return "assign"
     */
    std::string getActionType() const {
        return "assign";
    }

    /**
     * @brief Validate assign action configuration
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate() const;

    /**
     * @brief Execute assign action using Executor pattern
     * @param context Runtime context for execution
     * @return true if assignment was successful
     */
    bool execute(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Clone this action node
     * @return Deep copy of this AssignActionNode
     */
    std::shared_ptr<IActionNode> clone() const override;

private:
    std::string location_;  // Data model location to assign to
    std::string expr_;      // Expression to evaluate
    std::string attr_;      // Attribute to read from (alternative to expr)
    std::string type_;      // Optional type hint
};

} // namespace Core
}  // namespace SCXML
