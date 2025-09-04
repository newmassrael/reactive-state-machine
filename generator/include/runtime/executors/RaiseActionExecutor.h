#pragma once

#include "runtime/ActionExecutor.h"
#include "events/Event.h"
#include <string>
#include <vector>
#include <memory>

namespace SCXML {

// Forward declarations
namespace Core {
class RaiseActionNode;
}

namespace Runtime {

/**
 * @brief Executor for Raise Actions
 *
 * Handles the execution of internal event raising in SCXML state machines.
 * Creates and raises internal events with optional data payload.
 */
class RaiseActionExecutor : public ActionExecutor {
public:
    /**
     * @brief Execute raise action
     * @param actionNode The raise action node containing configuration
     * @param context Runtime context for execution
     * @return true if event was raised successfully
     */
    bool execute(const Core::ActionNode& actionNode, RuntimeContext& context) override;

    /**
     * @brief Get the action type that this executor handles
     * @return "raise"
     */
    std::string getActionType() const override {
        return "raise";
    }

    /**
     * @brief Validate raise action configuration
     * @param actionNode The raise action node to validate
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate(const Core::ActionNode& actionNode) const override;

private:
    /**
     * @brief Create event from raise action configuration
     * @param raiseNode The raise action node
     * @param context Runtime context for evaluation
     * @return Shared pointer to created event, nullptr if creation fails
     */
    std::shared_ptr<Events::Event> createEvent(const Core::RaiseActionNode& raiseNode, 
                                              RuntimeContext& context) const;
};

} // namespace Runtime
} // namespace SCXML