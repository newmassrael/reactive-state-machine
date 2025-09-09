#include "runtime/ActionExecutor.h"
#include "common/Logger.h"
#include "core/ActionNode.h"

// Include specific executors
#include "runtime/executors/AssignActionExecutor.h"
#include "runtime/executors/CancelActionExecutor.h"
#include "runtime/executors/ForeachActionExecutor.h"
#include "runtime/executors/IfActionExecutor.h"
#include "runtime/executors/LogActionExecutor.h"
#include "runtime/executors/RaiseActionExecutor.h"
#include "runtime/executors/ScriptActionExecutor.h"
#include "runtime/executors/SendActionExecutor.h"

namespace SCXML {
namespace Runtime {

// ========== ActionExecutor Base Class ==========

void ActionExecutor::logExecutionError(const std::string &actionType, const std::string &errorMessage,
                                       RuntimeContext &context) const {
    std::string fullMessage = "Action execution failed [" + actionType + "]: " + errorMessage;
    SCXML::Common::Logger::error(fullMessage);
    context.log("error", fullMessage);
}

// ========== DefaultActionExecutorFactory ==========

DefaultActionExecutorFactory::DefaultActionExecutorFactory() {
    registerBuiltInExecutors();
}

std::shared_ptr<ActionExecutor> DefaultActionExecutorFactory::createExecutor(const std::string &actionType) {
    SCXML::Common::Logger::error("DefaultActionExecutorFactory::createExecutor - Requested action type: " + actionType);
    auto it = executorCreators_.find(actionType);
    if (it != executorCreators_.end()) {
        SCXML::Common::Logger::error("DefaultActionExecutorFactory::createExecutor - Found executor for: " +
                                     actionType);
        return it->second();
    }
    SCXML::Common::Logger::error("DefaultActionExecutorFactory::createExecutor - NO EXECUTOR FOUND for: " + actionType);
    return nullptr;
}

bool DefaultActionExecutorFactory::supportsActionType(const std::string &actionType) const {
    return executorCreators_.find(actionType) != executorCreators_.end();
}

std::vector<std::string> DefaultActionExecutorFactory::getSupportedActionTypes() const {
    std::vector<std::string> types;
    types.reserve(executorCreators_.size());

    for (const auto &pair : executorCreators_) {
        types.push_back(pair.first);
    }

    return types;
}

void DefaultActionExecutorFactory::registerExecutor(const std::string &actionType,
                                                    std::function<std::shared_ptr<ActionExecutor>()> creator) {
    executorCreators_[actionType] = creator;
}

void DefaultActionExecutorFactory::registerBuiltInExecutors() {
    // Register all built-in action executors
    registerExecutor("assign", []() { return std::make_shared<AssignActionExecutor>(); });
    registerExecutor("log", []() { return std::make_shared<LogActionExecutor>(); });
    registerExecutor("send", []() { return std::make_shared<SendActionExecutor>(); });
    registerExecutor("raise", []() { return std::make_shared<RaiseActionExecutor>(); });
    registerExecutor("script", []() { return std::make_shared<ScriptActionExecutor>(); });
    registerExecutor("cancel", []() { return std::make_shared<CancelActionExecutor>(); });
    registerExecutor("foreach", []() { return std::make_shared<ForeachActionExecutor>(); });
    registerExecutor("if", []() {
        SCXML::Common::Logger::error("DefaultActionExecutorFactory: Creating IfActionExecutor instance");
        return std::make_shared<IfActionExecutor>();
    });
}

}  // namespace Runtime
}  // namespace SCXML