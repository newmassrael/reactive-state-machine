#pragma once

#include "runtime/ActionExecutor.h"

namespace SCXML {
namespace Runtime {

class SendActionExecutor : public ActionExecutor {
public:
    SendActionExecutor() = default;
    ~SendActionExecutor() override = default;

    bool execute(const Core::ActionNode& actionNode, RuntimeContext& context) override;
    std::string getActionType() const override { return "send"; }
};

} // namespace Runtime
} // namespace SCXML