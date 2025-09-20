#pragma once

#include "BaseAction.h"
#include <string>

namespace RSM {

/**
 * @brief SCXML <cancel> action implementation
 *
 * The <cancel> element is used to cancel a delayed <send> event that has not yet
 * been delivered. This is critical for managing scheduled events in SCXML.
 *
 * W3C SCXML Specification compliance:
 * - Supports sendid attribute for literal send ID
 * - Supports sendidexpr attribute for dynamic send ID evaluation
 * - Cancellation only affects delayed events, immediate events cannot be cancelled
 * - No error if the specified send ID does not exist or was already sent
 *
 * Example SCXML:
 * <cancel sendid="msg_001"/>
 * <cancel sendidexpr="currentMessageId"/>
 */
class CancelAction : public BaseAction {
public:
    /**
     * @brief Construct a new Cancel Action
     * @param sendId Send ID to cancel (optional, can use sendidexpr)
     * @param id Action identifier (optional)
     */
    explicit CancelAction(const std::string &sendId = "", const std::string &id = "");

    /**
     * @brief Destructor
     */
    virtual ~CancelAction() = default;

    /**
     * @brief Set the sendid of the event to cancel
     * @param sendId The sendid value from a previously sent event
     */
    void setSendId(const std::string &sendId);

    /**
     * @brief Get the sendid to cancel
     * @return sendid string
     */
    const std::string &getSendId() const;

    /**
     * @brief Set sendid expression to evaluate at runtime
     * @param expr Expression that evaluates to a sendid
     */
    void setSendIdExpr(const std::string &expr);

    /**
     * @brief Get the sendid expression
     * @return sendid expression string
     */
    const std::string &getSendIdExpr() const;

    // IActionNode implementation
    bool execute(IExecutionContext &context) override;
    std::string getActionType() const override;
    std::shared_ptr<IActionNode> clone() const override;

protected:
    // BaseAction implementation
    std::vector<std::string> validateSpecific() const override;
    std::string getSpecificDescription() const override;

private:
    std::string sendId_;      // Literal sendid value
    std::string sendIdExpr_;  // Expression that evaluates to sendid
};

}  // namespace RSM