#pragma once
#include "common/Result.h"
#include "core/ActionNode.h"
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
 * @brief SCXML <log> action implementation
 *
 * The <log> element allows an SCXML document to generate a logging or debug message.
 * This is useful for debugging state machine execution and monitoring state changes.
 */
class LogActionNode : public ActionNode {
public:
    /**
     * @brief Construct a new Log Action Node
     * @param id Action identifier
     */
    explicit LogActionNode(const std::string &id);

    /**
     * @brief Destructor
     */
    virtual ~LogActionNode() = default;

    /**
     * @brief Set the log expression to evaluate and output
     * @param expr Expression to evaluate and log (e.g., "'Current state: ' + currentState")
     */
    void setExpr(const std::string &expr);

    /**
     * @brief Get the log expression
     * @return expression string
     */
    const std::string &getExpr() const {
        return expr_;
    }

    /**
     * @brief Set the log label (optional descriptive prefix)
     * @param label Label to prepend to log message
     */
    void setLabel(const std::string &label);

    /**
     * @brief Get the log label
     * @return label string
     */
    const std::string &getLabel() const {
        return label_;
    }

    /**
     * @brief Set the log level (optional: debug, info, warning, error)
     * @param level Log level specification
     */
    void setLevel(const std::string &level);

    /**
     * @brief Get the log level
     * @return level string
     */
    const std::string &getLevel() const {
        return level_;
    }

    /**
     * @brief Execute the log action
     * @param context Runtime context for execution
     * @return true if logging was successful
     */
    bool execute(::SCXML::Runtime::RuntimeContext &context) override;

    /**
     * @brief Get action type name
     * @return "log"
     */
    std::string getActionType() const {
        return "log";
    }

    /**
     * @brief Clone this action node
     * @return Deep copy of this LogActionNode
     */
    std::shared_ptr<IActionNode> clone() const;

    /**
     * @brief Validate log action configuration
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate() const;

    // Helper methods removed - now handled by LogActionExecutor

private:
    std::string expr_;   // Expression to evaluate and log
    std::string label_;  // Optional label prefix
    std::string level_;  // Log level (debug, info, warning, error)
};

} // namespace Core
}  // namespace SCXML
