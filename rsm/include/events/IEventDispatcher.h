#pragma once

#include "EventDescriptor.h"
#include "IEventTarget.h"
#include <chrono>
#include <functional>
#include <future>
#include <string>

namespace RSM {

// Forward declarations
class EventDescriptor;
class IEventTarget;

/**
 * @brief Callback type for event execution
 *
 * Called when scheduled events are ready for execution.
 * Parameters: EventDescriptor, IEventTarget, sendId
 * Returns: true if execution succeeded
 */
using EventExecutionCallback =
    std::function<bool(const EventDescriptor &, std::shared_ptr<IEventTarget>, const std::string &)>;

/**
 * @brief Interface for dispatching SCXML events
 *
 * Provides high-level event sending capabilities with support for
 * delayed delivery, cancellation, and various target types.
 * Follows the Command pattern for flexible event handling.
 */
class IEventDispatcher {
public:
    virtual ~IEventDispatcher() = default;

    /**
     * @brief Send an event immediately
     * @param event Event descriptor containing all event information
     * @return Future with send result including assigned sendId
     */
    virtual std::future<SendResult> sendEvent(const EventDescriptor &event) = 0;

    /**
     * @brief Send an event with delay
     * @param event Event descriptor
     * @param delay Delay before sending
     * @return Future with send result including assigned sendId
     */
    virtual std::future<SendResult> sendEventDelayed(const EventDescriptor &event, std::chrono::milliseconds delay) = 0;

    /**
     * @brief Cancel a previously scheduled event
     * @param sendId Send ID returned from sendEvent or sendEventDelayed
     * @return true if event was successfully cancelled
     */
    virtual bool cancelEvent(const std::string &sendId) = 0;

    /**
     * @brief Check if an event is still pending
     * @param sendId Send ID to check
     * @return true if event is scheduled but not yet sent
     */
    virtual bool isEventPending(const std::string &sendId) const = 0;

    /**
     * @brief Get dispatcher statistics for monitoring
     * @return Statistics string (sent count, pending count, etc.)
     */
    virtual std::string getStatistics() const = 0;

    /**
     * @brief Shutdown the dispatcher and cancel all pending events
     */
    virtual void shutdown() = 0;

    /**
     * @brief Cancel all events for a specific session (W3C SCXML 6.2 compliance)
     * @param sessionId Session whose events should be cancelled
     * @return Number of events cancelled
     */
    virtual size_t cancelEventsForSession(const std::string &sessionId) = 0;
};

/**
 * @brief Interface for event scheduling
 *
 * Handles delayed event delivery with cancellation support.
 * Separated from IEventDispatcher for better testability.
 */
class IEventScheduler {
public:
    virtual ~IEventScheduler() = default;

    /**
     * @brief Schedule an event for future delivery
     * @param event Event to schedule
     * @param delay Delay before delivery
     * @param target Event target for delivery
     * @param sendId Optional send ID (auto-generated if empty)
     * @param sessionId Session ID that created this event (for cancellation on session termination)
     * @return Future containing the assigned send ID
     */
    virtual std::future<std::string> scheduleEvent(const EventDescriptor &event, std::chrono::milliseconds delay,
                                                   std::shared_ptr<IEventTarget> target, const std::string &sendId = "",
                                                   const std::string &sessionId = "") = 0;

    /**
     * @brief Cancel a scheduled event
     * @param sendId Send ID of the event to cancel
     * @return true if event was found and cancelled
     */
    virtual bool cancelEvent(const std::string &sendId) = 0;

    /**
     * @brief Cancel all scheduled events for a specific session
     * @param sessionId Session ID whose events should be cancelled
     * @return Number of events that were cancelled
     */
    virtual size_t cancelEventsForSession(const std::string &sessionId) = 0;

    /**
     * @brief Check if an event is still scheduled
     * @param scheduleId Schedule ID to check
     * @return true if event is scheduled but not yet delivered
     */
    virtual bool hasEvent(const std::string &sendId) const = 0;

    /**
     * @brief Get number of currently scheduled events
     * @return Count of pending scheduled events
     */
    virtual size_t getScheduledEventCount() const = 0;

    /**
     * @brief Shutdown scheduler and cancel all pending events
     * @param waitForCompletion Whether to wait for running events to complete
     */
    virtual void shutdown(bool waitForCompletion = true) = 0;

    /**
     * @brief Check if scheduler is currently running
     * @return true if scheduler is active
     */
    virtual bool isRunning() const = 0;
};

;

}  // namespace RSM