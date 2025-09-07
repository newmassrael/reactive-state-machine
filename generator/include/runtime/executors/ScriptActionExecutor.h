#pragma once

#include "runtime/ActionExecutor.h"

namespace SCXML {
namespace Runtime {

class ScriptActionExecutor : public ActionExecutor {
public:
    ScriptActionExecutor() = default;
    ~ScriptActionExecutor() override = default;

    bool execute(const Core::ActionNode& actionNode, RuntimeContext& context) override;
    std::vector<std::string> validate(const Core::ActionNode& actionNode) const override;
    std::string getActionType() const override { return "script"; }
};

} // namespace Runtime
} // namespace SCXML