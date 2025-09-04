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
 * @brief SCXML <raise> action implementation
 *
 * The <raise> element is used to raise internal events within the same SCXML interpreter.
 * These events are processed immediately and have higher priority than external events.
 */
class RaiseActionNode : public ActionNode {
public:
    /**
     * @brief Construct a new Raise Action Node
     * @param id Action identifier
     */
    explicit RaiseActionNode(const std::string &id);

    /**
     * @brief Destructor
     */
    virtual ~RaiseActionNode() = default;

    /**
     * @brief Set the event name to raise
     * @param event Event name (e.g., "internal.ready", "error.validation")
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
     * @param data Data to include with the raised event
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
     * @brief Execute the raise action
     * @param context Runtime context for execution
     * @return true if action executed successfully
     */
    bool execute(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get action type name
     * @return "raise"
     */
    std::string getActionType() const {
        return "raise";
    }

    /**
     * @brief Clone this action node
     * @return Deep copy of this RaiseActionNode
     */
    std::shared_ptr<IActionNode> clone() const;

protected:
    /**
     * @brief Create internal event from action parameters
     * @param context Runtime context
     * @return Created event ready for raising
     */
    SCXML::Events::EventPtr createEvent(::SCXML::Runtime::RuntimeContext &context);

private:
    std::string event_;  // Event name to raise
    std::string data_;   // Event data payload
};

} // namespace Core
}  // namespace SCXML
