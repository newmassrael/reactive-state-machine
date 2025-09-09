#pragma once

#include "model/IActionNode.h"
#include "runtime/ActionExecutor.h"
#include <memory>

namespace SCXML {
namespace Runtime {

class IfActionExecutor : public ActionExecutor {
public:
    IfActionExecutor() = default;
    ~IfActionExecutor() override = default;

    bool execute(const Core::ActionNode &actionNode, RuntimeContext &context) override;
    std::vector<std::string> validate(const Core::ActionNode &actionNode) const override;

    std::string getActionType() const override {
        return "if";
    }

private:
    /**
     * @brief Execute a nested action within an if branch
     * @param action The action to execute
     * @param context Runtime context for execution
     * @return true if action executed successfully
     */
    bool executeNestedAction(std::shared_ptr<SCXML::Model::IActionNode> action, RuntimeContext &context);
};

}  // namespace Runtime
}  // namespace SCXML