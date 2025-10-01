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

        // CRITICAL: Wait for any in-flight events to complete before destroying StateMachine
        // The EventScheduler may still be processing events in its callback thread
        // that reference this StateMachine. We must ensure all event processing completes.
        LOG_DEBUG("StateMachineContext: Waiting for in-flight events to complete");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        LOG_DEBUG("StateMachineContext: Destroying StateMachine");
        stateMachine_.reset();
    }

    LOG_DEBUG("StateMachineContext: Automatic cleanup completed");
}

}  // namespace RSM