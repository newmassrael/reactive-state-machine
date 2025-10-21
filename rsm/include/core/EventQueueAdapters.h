#pragma once

#include "core/EventQueueManager.h"
#include "runtime/EventRaiserImpl.h"
#include <memory>

namespace RSM::Core {

/**
 * @brief Internal event queue adapter for AOT engine
 *
 * Wraps EventQueueManager<Event> with unified interface
 * usable by EventProcessingAlgorithms.
 *
 * @tparam EventType Event type (usually enum class Event)
 *
 * @example
 * @code
 * EventQueueManager<Event> eventQueue_;
 * AOTEventQueue<Event> adapter(eventQueue_);
 *
 * EventProcessingAlgorithms::processInternalEventQueue(
 *     adapter,
 *     [this](Event e) { return processInternalEvent(e); }
 * );
 * @endcode
 */
template <typename EventType> class AOTEventQueue {
public:
    /**
     * @brief Constructor
     * @param queue EventQueueManager reference
     */
    explicit AOTEventQueue(EventQueueManager<EventType> &queue) : queue_(queue) {}

    /**
     * @brief Check if queue has events
     * @return true if queue has events
     */
    bool hasEvents() const {
        return queue_.hasEvents();
    }

    /**
     * @brief Pop next event from queue (FIFO)
     * @return Popped event
     */
    EventType popNext() {
        return queue_.pop();
    }

private:
    EventQueueManager<EventType> &queue_;
};

/**
 * @brief Internal event queue adapter for Interpreter engine
 *
 * Wraps EventRaiserImpl with unified interface
 * usable by EventProcessingAlgorithms.
 *
 * @note Since EventRaiserImpl's processNextQueuedEvent() processes
 *       events internally via callback, popNext() returns only
 *       processing success status, not actual event value.
 *
 * @example
 * @code
 * std::shared_ptr<EventRaiserImpl> eventRaiser_;
 * InterpreterEventQueue adapter(eventRaiser_);
 *
 * EventProcessingAlgorithms::processInternalEventQueue(
 *     adapter,
 *     [](bool) { return true; }  // EventRaiser handles internally
 * );
 * @endcode
 */
class InterpreterEventQueue {
public:
    /**
     * @brief Constructor
     * @param raiser EventRaiserImpl shared_ptr
     */
    explicit InterpreterEventQueue(std::shared_ptr<EventRaiserImpl> raiser) : raiser_(raiser) {}

    /**
     * @brief Check if queue has events
     * @return true if queue has events
     */
    bool hasEvents() const {
        return raiser_ && raiser_->hasQueuedEvents();
    }

    /**
     * @brief Process next event from queue
     *
     * Calls EventRaiserImpl::processNextQueuedEvent() to
     * process events internally via callback.
     *
     * @return Processing success status (does not return actual event value)
     */
    bool popNext() {
        return raiser_ && raiser_->processNextQueuedEvent();
    }

private:
    std::shared_ptr<EventRaiserImpl> raiser_;
};

}  // namespace RSM::Core
