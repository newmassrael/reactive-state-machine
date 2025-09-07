#pragma once

#include "runtime/ActionExecutor.h"

namespace SCXML {
namespace Runtime {

class CancelActionExecutor : public ActionExecutor {
public:
    CancelActionExecutor() = default;
    ~CancelActionExecutor() override = default;

    bool execute(const Core::ActionNode& actionNode, RuntimeContext& context) override;
    std::vector<std::string> validate(const Core::ActionNode& actionNode) const override;
    std::string getActionType() const override { return "cancel"; }
};

} // namespace Runtime
} // namespace SCXML