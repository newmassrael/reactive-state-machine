#pragma once

#include <deque>
#include <functional>
#include <string>

namespace RSM::Core {

/**
 * @brief W3C SCXML 3.12.1: Internal Event Queue Management
 *
 * This class implements the W3C SCXML internal event queue semantics.
 * Internal events are placed at the back of the queue and processed
 * in FIFO order before external events (macrostep completion).
 *
 * Design Goals:
 * - Single source of truth for event queue logic
 * - Shared between static and dynamic engines
 * - Zero overhead when not used (inline methods)
 * - Template-based for type safety
 *
 * W3C SCXML References:
 * - Section 3.12.1: Internal Events
 * - Appendix D.1: Algorithm for SCXML Interpretation
 */
template <typename EventType = std::string> class EventQueueManager {
public:
    /**
     * @brief Raise an internal event (W3C SCXML 3.14.1)
     *
     * Internal events are placed at the back of the internal event queue.
     * They are processed before external events but after currently queued
     * internal events (FIFO ordering).
     *
     * @param event Event to raise internally
     */
    void raise(const EventType &event) {
        queue_.push_back(event);
    }

    /**
     * @brief Check if internal queue has events
     * @return true if queue is not empty
     */
    bool hasEvents() const {
        return !queue_.empty();
    }

    /**
     * @brief Get number of queued events
     * @return Number of events in queue
     */
    size_t size() const {
        return queue_.size();
    }

    /**
     * @brief Pop next internal event from queue (FIFO)
     * @return Next event from front of queue
     * @throws std::runtime_error if queue is empty
     */
    EventType pop() {
        if (queue_.empty()) {
            throw std::runtime_error("EventQueueManager: Cannot pop from empty queue");
        }
        EventType event = queue_.front();
        queue_.pop_front();
        return event;
    }

    /**
     * @brief Clear all queued events
     */
    void clear() {
        queue_.clear();
    }

    /**
     * @brief Process all internal events with a handler (W3C SCXML D.1)
     *
     * Processes all queued internal events in FIFO order. This implements
     * the macrostep completion logic where all internal events generated
     * during state entry are processed before external events.
     *
     * The handler is called for each event and should return true if a
     * transition was taken, false otherwise.
     *
     * @param handler Function to process each event: bool handler(EventType)
     */
    template <typename Handler> void processAll(Handler handler) {
        while (!queue_.empty()) {
            EventType event = queue_.front();
            queue_.pop_front();
            handler(event);
        }
    }

private:
    std::deque<EventType> queue_;  // FIFO ordering per W3C SCXML 3.12.1
};

}  // namespace RSM::Core
