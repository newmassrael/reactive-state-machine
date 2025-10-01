#include "runtime/StateMachineContext.h"
#include "common/Logger.h"

namespace RSM {

StateMachineContext::StateMachineContext(std::shared_ptr<StateMachine> stateMachine)
    : stateMachine_(std::move(stateMachine)) {
    LOG_DEBUG("StateMachineContext: Created with StateMachine (shared): {}", (void *)stateMachine_.get());
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

        // With shared_ptr ownership, callbacks using weak_ptr are safe
        // No sleep needed - callbacks will check weak_ptr validity
        LOG_DEBUG("StateMachineContext: Releasing StateMachine (shared_ptr, use_count: {})", stateMachine_.use_count());
        stateMachine_.reset();
    }

    LOG_DEBUG("StateMachineContext: Automatic cleanup completed");
}

}  // namespace RSM