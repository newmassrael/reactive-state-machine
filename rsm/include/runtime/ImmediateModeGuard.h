#ifndef RSM_IMMEDIATE_MODE_GUARD_H
#define RSM_IMMEDIATE_MODE_GUARD_H

#include "EventRaiserImpl.h"
#include "IEventRaiser.h"
#include <memory>

namespace RSM {

/**
 * @brief RAII guard for managing EventRaiser immediate mode
 *
 * @details W3C SCXML 3.13 compliance requirement:
 * When processing events in parallel states, immediate mode must be temporarily disabled
 * to prevent re-entrancy bugs. This guard ensures immediate mode is restored even if
 * exceptions occur during event processing.
 *
 * Usage:
 * @code
 * {
 *     ImmediateModeGuard guard(eventRaiser, false);  // Disables immediate mode
 *     // ... process events ...
 * }  // Automatic restoration on scope exit
 * @endcode
 *
 * @see W3C SCXML 1.0 Section 3.13 "Selecting and Executing Transitions"
 */
class ImmediateModeGuard {
public:
    /**
     * @brief Construct guard and set immediate mode
     * @param raiser EventRaiser instance to control
     * @param enabled Desired immediate mode state
     */
    explicit ImmediateModeGuard(std::shared_ptr<IEventRaiser> raiser, bool enabled)
        : raiser_(std::dynamic_pointer_cast<EventRaiserImpl>(raiser)), previousState_(false) {
        if (raiser_) {
            previousState_ = raiser_->isImmediateModeEnabled();
            raiser_->setImmediateMode(enabled);
        }
    }

    /**
     * @brief Destructor restores previous immediate mode state
     * @note noexcept to prevent exception propagation during stack unwinding
     */
    ~ImmediateModeGuard() noexcept {
        if (raiser_) {
            raiser_->setImmediateMode(previousState_);
        }
    }

    // Non-copyable, non-movable (RAII idiom)
    ImmediateModeGuard(const ImmediateModeGuard &) = delete;
    ImmediateModeGuard &operator=(const ImmediateModeGuard &) = delete;
    ImmediateModeGuard(ImmediateModeGuard &&) = delete;
    ImmediateModeGuard &operator=(ImmediateModeGuard &&) = delete;

private:
    std::shared_ptr<EventRaiserImpl> raiser_;
    bool previousState_;
};

}  // namespace RSM

#endif  // RSM_IMMEDIATE_MODE_GUARD_H
