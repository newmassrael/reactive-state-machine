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
        // CRITICAL: Always call stop(), even if isRunning_ is false
        // W3C Test 415: isRunning_=false may be set when entering top-level final state
        // stop() must always execute to unregister from JSEngine and prevent race conditions
        LOG_DEBUG("StateMachineContext: Calling StateMachine::stop() (isRunning: {})", stateMachine_->isRunning());
        stateMachine_->stop();

        // With shared_ptr ownership, callbacks using weak_ptr are safe
        // No sleep needed - callbacks will check weak_ptr validity
        LOG_DEBUG("StateMachineContext: Releasing StateMachine (shared_ptr, use_count: {})", stateMachine_.use_count());
        stateMachine_.reset();
    }

    LOG_DEBUG("StateMachineContext: Automatic cleanup completed");
}

}  // namespace RSM