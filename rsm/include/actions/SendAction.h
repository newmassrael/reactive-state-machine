#pragma once

#include "BaseAction.h"
#include <chrono>
#include <map>
#include <string>

namespace RSM {

/**
 * @brief SCXML <send> action implementation
 *
 * The <send> element is used to send events to external systems or other SCXML interpreters.
 * This is one of the most critical SCXML actions for event-driven state machine operation.
 *
 * W3C SCXML Specification compliance:
 * - Supports event, eventexpr attributes for dynamic event names
 * - Supports target, targetexpr for dynamic target specification
 * - Supports delay, delayexpr for scheduled event delivery
 * - Supports data and param elements for event payload
 * - Generates unique sendid for event tracking and cancellation
 *
 * Example SCXML:
 * <send event="user.notify" target="http://api.example.com/webhook"
 *       delay="5s" data="'Hello World'" sendid="msg_001"/>
 */
class SendAction : public BaseAction {
public:
    /**
     * @brief Construct a new Send Action
     * @param event Event name to send (optional, can use eventexpr)
     * @param id Action identifier (optional)
     */
    explicit SendAction(const std::string &event = "", const std::string &id = "");

    /**
     * @brief Destructor
     */
    virtual ~SendAction() = default;

    /**
     * @brief Set the event name to send
     * @param event Event name (e.g., "user.click", "system.ready")
     */
    void setEvent(const std::string &event);

    /**
     * @brief Get the event name
     * @return Event name string
     */
    const std::string &getEvent() const;

    /**
     * @brief Set the event expression for dynamic event names
     * @param eventExpr Expression to evaluate for event name (W3C SCXML eventexpr attribute)
     */
    void setEventExpr(const std::string &eventExpr);

    /**
     * @brief Get the event expression
     * @return Event expression string
     */
    const std::string &getEventExpr() const;

    /**
     * @brief Set the target for the event
     * @param target Target URI (e.g., "scxml:session123", "http://example.com", empty for session-scoped)
     */
    void setTarget(const std::string &target);

    /**
     * @brief Get the target URI
     * @return Target URI string
     */
    const std::string &getTarget() const;

    /**
     * @brief Set the target expression for dynamic target specification
     * @param targetExpr Expression to evaluate for target URI (W3C SCXML targetexpr attribute)
     */
    void setTargetExpr(const std::string &targetExpr);

    /**
     * @brief Get the target expression
     * @return Target expression string
     */
    const std::string &getTargetExpr() const;

    /**
     * @brief Set event data payload
     * @param data Data to include with the event
     */
    void setData(const std::string &data);

    /**
     * @brief Get event data
     * @return Event data string
     */
    const std::string &getData() const;

    /**
     * @brief Set delay for event delivery
     * @param delay Delay specification (e.g., "5s", "100ms")
     */
    void setDelay(const std::string &delay);

    /**
     * @brief Get delay specification
     * @return Delay string
     */
    const std::string &getDelay() const;

    /**
     * @brief Set delay expression for dynamic delay values
     * @param delayExpr Expression to evaluate for delay (W3C SCXML delayexpr attribute)
     */
    void setDelayExpr(const std::string &delayExpr);

    /**
     * @brief Get delay expression
     * @return Delay expression string
     */
    const std::string &getDelayExpr() const;

    /**
     * @brief Set sender ID for event tracking
     * @param sendId Unique identifier for this send operation
     */
    void setSendId(const std::string &sendId);

    /**
     * @brief Get sender ID
     * @return Sender ID string
     */
    const std::string &getSendId() const;

    /**
     * @brief Set ID location for storing generated sendid
     * @param idLocation Variable name to store the sendid (W3C SCXML idlocation attribute)
     */
    void setIdLocation(const std::string &idLocation);

    /**
     * @brief Get ID location
     * @return ID location variable name
     */
    const std::string &getIdLocation() const;

    /**
     * @brief Set event type override
     * @param type Event type ("platform", "internal", "external")
     */
    void setType(const std::string &type);

    /**
     * @brief Get event type
     * @return Event type string
     */
    const std::string &getType() const;

    /**
     * @brief Set type expression for dynamic type evaluation (W3C SCXML 6.2 typeexpr attribute)
     * @param typeExpr Expression to evaluate for event type at send time
     */
    void setTypeExpr(const std::string &typeExpr);

    /**
     * @brief Get type expression
     * @return Type expression string
     */
    const std::string &getTypeExpr() const;

    /**
     * @brief Set namelist for W3C SCXML compliant data passing
     * @param namelist Space-separated list of variable names to include in event data
     */
    void setNamelist(const std::string &namelist);

    /**
     * @brief Get namelist
     * @return Namelist string
     */
    const std::string &getNamelist() const;

    /**
     * @brief Parameter structure for SCXML send params with expr support
     */
    struct SendParam {
        std::string name;
        std::string expr;  // SCXML expr attribute for dynamic evaluation

        SendParam(const std::string &paramName, const std::string &paramExpr) : name(paramName), expr(paramExpr) {}
    };

    /**
     * @brief Add a parameter with expression for dynamic evaluation (W3C SCXML compliant)
     * @param name Parameter name
     * @param expr Parameter expression to evaluate at send time
     */
    void addParamWithExpr(const std::string &name, const std::string &expr);

    /**
     * @brief Get parameters with expressions for W3C SCXML compliance
     * @return Vector of SendParam structures with name and expr
     */
    const std::vector<SendParam> &getParamsWithExpr() const;

    /**
     * @brief Clear all parameters
     */
    void clearParams();

    /**
     * @brief Set the content for the send action (W3C SCXML C.2)
     * @param content The content to send as HTTP body
     * @note When content is set, it takes priority over data attribute for HTTP transmission.
     *       Maximum recommended size: 10MB (enforced by validation).
     *       Content will be sent with Content-Type: text/plain.
     */
    void setContent(const std::string &content);

    /**
     * @brief Get the content for the send action
     * @return The content
     */
    const std::string &getContent() const;

    /**
     * @brief Parse delay string to milliseconds
     * @param delayStr Delay specification (e.g., "5s", "100ms", "2min")
     * @return Delay in milliseconds, 0 if immediate or invalid
     */
    std::chrono::milliseconds parseDelayString(const std::string &delayStr) const;

    // IActionNode implementation
    bool execute(IExecutionContext &context) override;
    std::string getActionType() const override;
    std::shared_ptr<IActionNode> clone() const override;

protected:
    // BaseAction implementation
    std::vector<std::string> validateSpecific() const override;
    std::string getSpecificDescription() const override;

private:
    std::string event_;      // Event name to send
    std::string eventExpr_;  // Event expression for dynamic event names
    std::string
        target_;  // Target URI for event delivery (empty = session-scoped)          // Target URI for event delivery
    std::string targetExpr_;                 // Target expression for dynamic targets
    std::string data_;                       // Event data payload
    std::string delay_;                      // Delivery delay specification
    std::string delayExpr_;                  // Delay expression for dynamic delays
    std::string sendId_;                     // Sender ID for tracking
    std::string idLocation_;                 // Variable name to store sendid (W3C SCXML idlocation)
    std::string type_;                       // Event type (empty by default per W3C SCXML)
    std::string typeExpr_;                   // Type expression for dynamic type evaluation (W3C SCXML 6.2)
    std::string namelist_;                   // Space-separated list of variables for event data (W3C SCXML C.1)
    std::vector<SendParam> paramsWithExpr_;  // W3C SCXML compliant params with expr
    std::string content_;                    // Content to send as HTTP body (W3C SCXML C.2)
};

}  // namespace RSM