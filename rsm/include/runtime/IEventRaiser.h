#pragma once

#include <string>

namespace RSM {

/**
 * @brief Interface for raising events in the SCXML system
 *
 * This interface implements the SCXML "fire and forget" event model as specified
 * by W3C SCXML standard. Events are processed asynchronously to prevent deadlocks
 * and ensure proper event ordering. The interface separates event raising from
 * action execution, following the Single Responsibility Principle.
 */
class IEventRaiser {
public:
    virtual ~IEventRaiser() = default;

    /**
     * @brief Raise an event with the given name and data (SCXML "fire and forget")
     *
     * Events are queued for asynchronous processing and this method returns immediately.
     * This implements the SCXML "fire and forget" model to prevent deadlocks and ensure
     * proper event ordering as specified by W3C SCXML standard.
     *
     * @param eventName Name of the event to raise
     * @param eventData Data associated with the event
     * @return true if the event was successfully queued, false if the raiser is not ready
     */
    virtual bool raiseEvent(const std::string &eventName, const std::string &eventData) = 0;

    /**
     * @brief Raise an event with origin tracking for W3C SCXML finalize support
     *
     * Events are queued for asynchronous processing with origin session information.
     * This enables proper finalize handler execution as specified by W3C SCXML 6.4.
     *
     * @param eventName Name of the event to raise
     * @param eventData Data associated with the event
     * @param originSessionId Session ID that originated this event (for finalize)
     * @return true if the event was successfully queued, false if the raiser is not ready
     */
    virtual bool raiseEvent(const std::string &eventName, const std::string &eventData,
                            const std::string &originSessionId) = 0;

    /**
     * @brief Raise an error event with sendid for W3C SCXML 5.10 compliance
     *
     * When send actions fail, error events must include the sendid of the failed send element.
     * This enables test 332 compliance where error.execution event must contain sendid.
     *
     * @param eventName Name of the event to raise (typically "error.execution")
     * @param eventData Data associated with the event
     * @param sendId Send ID from the failed send element
     * @param unused Discriminator parameter for overload resolution (unused, always pass false)
     * @return true if the event was successfully queued, false if the raiser is not ready
     *
     * @note The bool parameter exists solely for C++ overload resolution to distinguish
     *       this variant from raiseEvent(name, data, originSessionId). Both take three
     *       string parameters, requiring a discriminator to avoid ambiguity.
     */
    virtual bool raiseEvent(const std::string &eventName, const std::string &eventData, const std::string &sendId,
                            bool unused) = 0;

    /**
     * @brief Check if the event raiser is ready to raise events
     * @return true if ready, false otherwise
     */
    virtual bool isReady() const = 0;

    /**
     * @brief Set execution mode for SCXML compliance
     * @param immediate true for immediate processing, false for queued processing
     */
    virtual void setImmediateMode(bool immediate) = 0;

    /**
     * @brief Process all queued events synchronously (for SCXML compliance)
     * This method processes queued events in order and returns when all are processed
     */
    virtual void processQueuedEvents() = 0;

    /**
     * @brief W3C SCXML compliance: Process only ONE event from the queue
     * @return true if an event was processed, false if queue is empty
     */
    virtual bool processNextQueuedEvent() = 0;

    /**
     * @brief Check if there are queued events waiting to be processed
     * @return true if queue has events, false if empty
     */
    virtual bool hasQueuedEvents() const = 0;
};

}  // namespace RSM