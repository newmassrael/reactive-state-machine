#pragma once
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
 * @brief SCXML <send> action implementation
 *
 * The <send> element is used to send events to external systems or other SCXML interpreters.
 * This is one of the most critical SCXML actions for event-driven state machine operation.
 */
class SendActionNode : public ActionNode {
public:
    /**
     * @brief Construct a new Send Action Node
     * @param id Action identifier
     */
    explicit SendActionNode(const std::string &id);

    /**
     * @brief Destructor
     */
    virtual ~SendActionNode() = default;

    /**
     * @brief Set the target for the event
     * @param target Target URI (e.g., "#_internal", "http://example.com", "scxml:session123")
     */
    void setTarget(const std::string &target);

    /**
     * @brief Get the target URI
     * @return Target URI string
     */
    const std::string &getTarget() const {
        return target_;
    }

    /**
     * @brief Set the event name to send
     * @param event Event name (e.g., "user.click", "system.ready")
     */
    void setEvent(const std::string &event);

    /**
     * @brief Get the event name
     * @return Event name string
     */
    const std::string &getEvent() const {
        return event_;
    }

    /**
     * @brief Set event data payload
     * @param data Data to include with the event
     */
    void setData(const std::string &data);

    /**
     * @brief Get event data
     * @return Event data string
     */
    const std::string &getData() const {
        return data_;
    }

    /**
     * @brief Set delay for event delivery
     * @param delay Delay specification (e.g., "5s", "100ms")
     */
    void setDelay(const std::string &delay);

    /**
     * @brief Get delay specification
     * @return Delay string
     */
    const std::string &getDelay() const {
        return delay_;
    }

    /**
     * @brief Set sender ID for event tracking
     * @param sendId Unique identifier for this send operation
     */
    void setSendId(const std::string &sendId);

    /**
     * @brief Get sender ID
     * @return Sender ID string
     */
    const std::string &getSendId() const {
        return sendId_;
    }

    /**
     * @brief Set event type override
     * @param type Event type ("platform", "internal", "external")
     */
    void setType(const std::string &type);

    /**
     * @brief Get event type
     * @return Event type string
     */
    const std::string &getType() const {
        return type_;
    }

    /**
     * @brief Execute the send action
     * @param context Runtime context for execution
     * @return true if action executed successfully
     */
    bool execute(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get action type name
     * @return "send"
     */
    std::string getActionType() const {
        return "send";
    }

    /**
     * @brief Validate send action configuration
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate() const;

    /**
     * @brief Clone this action node
     * @return Deep copy of this SendActionNode
     */
    std::shared_ptr<IActionNode> clone() const;

protected:
    /**
     * @brief Create event from action parameters
     * @param context Runtime context
     * @return Created event ready for sending
     */
    SCXML::Events::EventPtr createEvent(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Parse delay string to milliseconds
     * @param delayStr Delay specification (e.g., "5s", "100ms", "2min")
     * @return Delay in milliseconds, 0 if immediate
     */
    uint64_t parseDelay(const std::string &delayStr) const;

private:
    std::string target_;  // Target URI for event delivery
    std::string event_;   // Event name to send
    std::string data_;    // Event data payload
    std::string delay_;   // Delivery delay specification
    std::string sendId_;  // Sender ID for tracking
    std::string type_;    // Event type override
};

} // namespace Core
}  // namespace SCXML
