#include "runtime/StateMachineContext.h"
#include "common/Logger.h"

namespace RSM {

StateMachineContext::StateMachineContext(std::unique_ptr<StateMachine> stateMachine)
    : stateMachine_(std::move(stateMachine)) {
    LOG_DEBUG("StateMachineContext: Created with StateMachine: {}", (void *)stateMachine_.get());
}

StateMachineContext::~StateMachineContext() {
    LOG_DEBUG("StateMachineContext: Starting automatic cleanup");

    // Only cleanup StateMachine
    // EventRaiser/EventDispatcher are owned externally (e.g., TestResources)
    if (stateMachine_) {
        if (stateMachine_->isRunning()) {
            LOG_DEBUG("StateMachineContext: Stopping StateMachine");
            stateMachine_->stop();
        }
        LOG_DEBUG("StateMachineContext: Destroying StateMachine");
        stateMachine_.reset();
    }

    LOG_DEBUG("StateMachineContext: Automatic cleanup completed");
}

}  // namespace RSM