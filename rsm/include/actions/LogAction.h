#pragma once

#include "BaseAction.h"
#include <string>

namespace RSM {

/**
 * @brief SCXML <log> action implementation
 *
 * The <log> element allows an SCXML document to generate a logging or debug message.
 * This follows W3C SCXML specification for logging functionality.
 *
 * Example SCXML:
 * <log expr="'Current state: ' + currentState" label="Debug" level="info"/>
 */
class LogAction : public BaseAction {
public:
    /**
     * @brief Construct a new Log Action
     * @param expr Expression to evaluate and log
     * @param id Action identifier (optional)
     */
    explicit LogAction(const std::string &expr = "", const std::string &id = "");

    /**
     * @brief Destructor
     */
    virtual ~LogAction() = default;

    /**
     * @brief Set the log expression to evaluate and output
     * @param expr Expression to evaluate and log (e.g., "'Current state: ' + currentState")
     */
    void setExpr(const std::string &expr);

    /**
     * @brief Get the log expression
     * @return Expression string
     */
    const std::string &getExpr() const;

    /**
     * @brief Set the log label (optional descriptive prefix)
     * @param label Label to prepend to log message
     */
    void setLabel(const std::string &label);

    /**
     * @brief Get the log label
     * @return Label string
     */
    const std::string &getLabel() const;

    /**
     * @brief Set the log level (optional: debug, info, warning, error)
     * @param level Log level specification
     */
    void setLevel(const std::string &level);

    /**
     * @brief Get the log level
     * @return Level string
     */
    const std::string &getLevel() const;

    // IActionNode implementation
    bool execute(IExecutionContext &context) override;
    std::string getActionType() const override;
    std::shared_ptr<IActionNode> clone() const override;

protected:
    // BaseAction implementation
    std::vector<std::string> validateSpecific() const override;
    std::string getSpecificDescription() const override;

private:
    std::string expr_;   // Expression to evaluate and log
    std::string label_;  // Optional label prefix
    std::string level_;  // Log level (debug, info, warning, error)
};

}  // namespace RSM