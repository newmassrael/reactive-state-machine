#pragma once

#include "StateMachine.h"
#include "events/IEventDispatcher.h"
#include "runtime/IEventRaiser.h"
#include <memory>

namespace RSM {

/**
 * @brief RAII wrapper for StateMachine with automatic cleanup
 *
 * StateMachineContext owns StateMachine (shared ownership for callback safety).
 * EventRaiser/EventDispatcher are owned externally (e.g., TestResources) and can be
 * shared across multiple StateMachine instances.
 *
 * Cleanup on destruction:
 * 1. StateMachine::stop() if running
 * 2. StateMachine destruction (when last shared_ptr released)
 *
 * Note: EventRaiser/EventDispatcher are NOT owned by StateMachineContext.
 * They must be managed separately by the caller (e.g., via TestResources RAII wrapper).
 */
class StateMachineContext {
public:
    /**
     * @brief Construct context with StateMachine (shared ownership)
     * @param stateMachine The state machine instance (shared ownership for callback safety)
     */
    explicit StateMachineContext(std::shared_ptr<StateMachine> stateMachine);

    /**
     * @brief Destructor - performs automatic cleanup in correct order
     */
    ~StateMachineContext();

    // Non-copyable
    StateMachineContext(const StateMachineContext &) = delete;
    StateMachineContext &operator=(const StateMachineContext &) = delete;

    // Movable
    StateMachineContext(StateMachineContext &&) noexcept = default;
    StateMachineContext &operator=(StateMachineContext &&) noexcept = default;

    /**
     * @brief Access StateMachine via pointer semantics
     * @return Pointer to owned StateMachine
     */
    StateMachine *operator->() {
        return stateMachine_.get();
    }

    const StateMachine *operator->() const {
        return stateMachine_.get();
    }

    /**
     * @brief Get raw pointer to StateMachine
     * @return Pointer to owned StateMachine
     */
    StateMachine *get() {
        return stateMachine_.get();
    }

    const StateMachine *get() const {
        return stateMachine_.get();
    }

    /**
     * @brief Get shared_ptr to StateMachine for callback safety
     * @return Shared pointer to StateMachine
     */
    std::shared_ptr<StateMachine> getShared() {
        return stateMachine_;
    }

    /**
     * @brief Check if context has a valid StateMachine
     * @return true if StateMachine exists
     */
    explicit operator bool() const {
        return stateMachine_ != nullptr;
    }

private:
    std::shared_ptr<StateMachine> stateMachine_;  // Shared ownership for callback safety
};

}  // namespace RSM