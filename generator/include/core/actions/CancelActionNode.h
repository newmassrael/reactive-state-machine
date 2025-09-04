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
 * @brief SCXML <cancel> action implementation
 *
 * The <cancel> element is used to cancel a delayed <send> event that has not yet
 * been delivered. This is critical for managing scheduled events in SCXML.
 */
class CancelActionNode : public ActionNode {
public:
    /**
     * @brief Construct a new Cancel Action Node
     * @param id Action identifier
     */
    explicit CancelActionNode(const std::string &id);

    /**
     * @brief Destructor
     */
    virtual ~CancelActionNode() = default;

    /**
     * @brief Set the sendid of the event to cancel
     * @param sendId The sendid value from a previously sent event
     */
    void setSendId(const std::string &sendId);

    /**
     * @brief Get the sendid to cancel
     * @return sendid string
     */
    const std::string &getSendId() const {
        return sendId_;
    }

    /**
     * @brief Set sendid expression to evaluate at runtime
     * @param expr Expression that evaluates to a sendid
     */
    void setSendIdExpr(const std::string &expr);

    /**
     * @brief Get the sendid expression
     * @return sendid expression string
     */
    const std::string &getSendIdExpr() const {
        return sendIdExpr_;
    }

    /**
     * @brief Execute the cancel action
     * @param context Runtime context for execution
     * @return true if cancellation was successful or event was already sent
     */
    virtual bool execute(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get action type name
     * @return "cancel"
     */
    std::string getActionType() const {
        return "cancel";
    }

    /**
     * @brief Clone this action node
     * @return Deep copy of this CancelActionNode
     */
    std::shared_ptr<IActionNode> clone() const;

protected:
    /**
     * @brief Resolve sendid from literal value or expression
     * @param context Runtime context for expression evaluation
     * @return Resolved sendid string, empty if resolution fails
     */
    std::string resolveSendId(::SCXML::Runtime::RuntimeContext &context);

private:
    std::string sendId_;      // Literal sendid value
    std::string sendIdExpr_;  // Expression that evaluates to sendid
};

} // namespace Core
}  // namespace SCXML
