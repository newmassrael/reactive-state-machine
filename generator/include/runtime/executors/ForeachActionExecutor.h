#pragma once

#include "runtime/ActionExecutor.h"

namespace SCXML {
namespace Runtime {

class ForeachActionExecutor : public ActionExecutor {
public:
    ForeachActionExecutor() = default;
    ~ForeachActionExecutor() override = default;

    bool execute(const Core::ActionNode& actionNode, RuntimeContext& context) override;
    std::vector<std::string> validate(const Core::ActionNode& actionNode) const override;
    std::string getActionType() const override { return "foreach"; }
};

} // namespace Runtime
} // namespace SCXML