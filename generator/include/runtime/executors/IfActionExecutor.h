#pragma once

#include "runtime/ActionExecutor.h"

namespace SCXML {
namespace Runtime {

class IfActionExecutor : public ActionExecutor {
public:
    IfActionExecutor() = default;
    ~IfActionExecutor() override = default;

    bool execute(const Core::ActionNode& actionNode, RuntimeContext& context) override;
    std::vector<std::string> validate(const Core::ActionNode& actionNode) const override;
    std::string getActionType() const override { return "if"; }
};

} // namespace Runtime
} // namespace SCXML