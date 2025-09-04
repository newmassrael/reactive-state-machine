#pragma once
#include "IExecutionContext.h"
#include "common/SCXMLCommon.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
namespace Model {

// Forward declarations
class Event;

/**
 * @brief Interface for SCXML <send> element implementation
 *
 * The <send> element is used to send events to external systems or other
 * state machines. It supports various event processors and communication
 * mechanisms as defined in SCXML 1.0 specification.
 */
class ISendNode {
public:
    virtual ~ISendNode() = default;

    /**
     * @brief Execute the send operation
     * @param context Execution context containing state machine state
     * @return Result indicating success or failure with error details
     */
    virtual SCXML::Common::Result<void> execute(IExecutionContext &context) = 0;

    /**
     * @brief Get the event name or expression to be sent
     * @return Event name or expression that evaluates to event name
     */
    virtual const std::string &getEvent() const = 0;

    /**
     * @brief Set the event name or expression
     * @param event Event name or expression
     */
    virtual void setEvent(const std::string &event) = 0;

    /**
     * @brief Get the target URI or expression
     * @return Target URI or expression that evaluates to target
     */
    virtual const std::string &getTarget() const = 0;

    /**
     * @brief Set the target URI or expression
     * @param target Target URI or expression
     */
    virtual void setTarget(const std::string &target) = 0;

    /**
     * @brief Get the event processor type
     * @return Event processor type (e.g., "scxml", "basichttp")
     */
    virtual const std::string &getType() const = 0;

    /**
     * @brief Set the event processor type
     * @param type Event processor type
     */
    virtual void setType(const std::string &type) = 0;

    /**
     * @brief Get the send ID for cancellation
     * @return Send ID or expression that evaluates to send ID
     */
    virtual const std::string &getId() const = 0;

    /**
     * @brief Set the send ID for cancellation
     * @param id Send ID or expression
     */
    virtual void setId(const std::string &id) = 0;

    /**
     * @brief Get the delay before sending
     * @return Delay expression in milliseconds
     */
    virtual const std::string &getDelay() const = 0;

    /**
     * @brief Set the delay before sending
     * @param delay Delay expression in milliseconds
     */
    virtual void setDelay(const std::string &delay) = 0;

    /**
     * @brief Get the event data content
     * @return Content to be sent as event data
     */
    virtual const std::string &getContent() const = 0;

    /**
     * @brief Set the event data content
     * @param content Content to be sent as event data
     */
    virtual void setContent(const std::string &content) = 0;

    /**
     * @brief Get content expression that evaluates to event data
     * @return Expression that evaluates to event data
     */
    virtual const std::string &getContentExpr() const = 0;

    /**
     * @brief Set content expression
     * @param contentExpr Expression that evaluates to event data
     */
    virtual void setContentExpr(const std::string &contentExpr) = 0;

    /**
     * @brief Get name-location pairs for event data
     * @return Map of parameter names to location expressions
     */
    virtual const std::map<std::string, std::string> &getNameLocationPairs() const = 0;

    /**
     * @brief Add a name-location pair for event data
     * @param name Parameter name
     * @param location Location expression for parameter value
     */
    virtual void addNameLocationPair(const std::string &name, const std::string &location) = 0;

    /**
     * @brief Validate the send node configuration
     * @return List of validation errors, empty if valid
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Clone this send node
     * @return Deep copy of this send node
     */
    virtual std::shared_ptr<ISendNode> clone() const = 0;

    /**
     * @brief Check if this is a delayed send operation
     * @return True if delay is specified and > 0
     */
    virtual bool isDelayed() const = 0;

    /**
     * @brief Check if send operation is to internal target
     * @return True if target is internal (same state machine)
     */
    virtual bool isInternalTarget() const = 0;

    /**
     * @brief Get the resolved event data for sending
     * @param context Execution context for expression evaluation
     * @return Result containing resolved event data or error
     */
    virtual SCXML::Common::Result<std::string> resolveEventData(IExecutionContext &context) const = 0;

    /**
     * @brief Get the resolved target URI
     * @param context Execution context for expression evaluation
     * @return Result containing resolved target URI or error
     */
    virtual SCXML::Common::Result<std::string> resolveTarget(IExecutionContext &context) const = 0;

    /**
     * @brief Get the resolved event name
     * @param context Execution context for expression evaluation
     * @return Result containing resolved event name or error
     */
    virtual SCXML::Common::Result<std::string> resolveEventName(IExecutionContext &context) const = 0;
};

/**
 * @brief Interface for SCXML <cancel> element implementation
 *
 * The <cancel> element is used to cancel previously sent events that have
 * not yet been delivered. It works with the send ID to identify which
 * sent event to cancel.
 */
class ICancelNode {
public:
    virtual ~ICancelNode() = default;

    /**
     * @brief Execute the cancel operation
     * @param context Execution context containing state machine state
     * @return Result indicating success or failure with error details
     */
    virtual SCXML::Common::Result<void> execute(IExecutionContext &context) = 0;

    /**
     * @brief Get the send ID to cancel
     * @return Send ID or expression that evaluates to send ID
     */
    virtual const std::string &getSendId() const = 0;

    /**
     * @brief Set the send ID to cancel
     * @param sendId Send ID or expression
     */
    virtual void setSendId(const std::string &sendId) = 0;

    /**
     * @brief Get the send ID expression
     * @return Expression that evaluates to send ID
     */
    virtual const std::string &getSendIdExpr() const = 0;

    /**
     * @brief Set the send ID expression
     * @param sendIdExpr Expression that evaluates to send ID
     */
    virtual void setSendIdExpr(const std::string &sendIdExpr) = 0;

    /**
     * @brief Validate the cancel node configuration
     * @return List of validation errors, empty if valid
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Clone this cancel node
     * @return Deep copy of this cancel node
     */
    virtual std::shared_ptr<ICancelNode> clone() const = 0;

    /**
     * @brief Get the resolved send ID to cancel
     * @param context Execution context for expression evaluation
     * @return Result containing resolved send ID or error
     */
    virtual SCXML::Common::Result<std::string> resolveSendId(IExecutionContext &context) const = 0;
};

}  // namespace Model
}  // namespace SCXML
