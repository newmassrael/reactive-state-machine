#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace SCXML {
// Forward declarations
namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {
class RuntimeContext;

/**
 * @brief Manages scheduled events for SCXML <send> with delay
 *
 * The EventScheduler handles delayed event delivery and cancellation,
 * providing the timing infrastructure needed for SCXML send/cancel operations.
 */
class EventScheduler {
public:
    /**
     * @brief Scheduled event entry
     */
    struct ScheduledEvent {
        std::string sendId;
        SCXML::Events::EventPtr event;
        std::chrono::steady_clock::time_point deliveryTime;
        std::string target;
        bool cancelled;

        ScheduledEvent(const std::string &id, SCXML::Events::EventPtr evt, std::chrono::steady_clock::time_point time,
                       const std::string &tgt)
            : sendId(id), event(evt), deliveryTime(time), target(tgt), cancelled(false) {}
    };

    /**
     * @brief Comparison for priority queue (earliest delivery first)
     */
    struct EventComparator {
        bool operator()(const std::shared_ptr<ScheduledEvent> &a, const std::shared_ptr<ScheduledEvent> &b) const {
            return a->deliveryTime > b->deliveryTime;
        }
    };

public:
    /**
     * @brief Construct EventScheduler
     */
    EventScheduler();

    /**
     * @brief Destructor - stops scheduler thread
     */
    ~EventScheduler();

    /**
     * @brief Start the scheduler thread
     */
    void start();

    /**
     * @brief Stop the scheduler thread
     */
    void stop();

    /**
     * @brief Schedule an event for delayed delivery
     * @param sendId Unique identifier for this scheduled event
     * @param event Event to deliver
     * @param delayMs Delay in milliseconds
     * @param target Target for event delivery
     * @param context Runtime context for event delivery
     * @return true if event was scheduled successfully
     */
    bool scheduleEvent(const std::string &sendId, SCXML::Events::EventPtr event, uint64_t delayMs,
                       const std::string &target, SCXML::Runtime::RuntimeContext *context);

    /**
     * @brief Cancel a scheduled event
     * @param sendId Identifier of event to cancel
     * @return true if event was found and cancelled (or already delivered)
     */
    bool cancelEvent(const std::string &sendId);

    /**
     * @brief Check if an event is currently scheduled
     * @param sendId Identifier to check
     * @return true if event exists and hasn't been delivered/cancelled
     */
    bool isEventScheduled(const std::string &sendId) const;

    /**
     * @brief Get number of pending scheduled events
     * @return Count of events waiting for delivery
     */
    size_t getPendingEventCount() const;

    /**
     * @brief Clear all scheduled events
     */
    void clearAllEvents();

private:
    /**
     * @brief Main scheduler thread function
     */
    void schedulerThreadMain();

    /**
     * @brief Deliver an event to its target
     * @param scheduledEvent Event to deliver
     * @param context Runtime context for delivery
     */
    void deliverEvent(std::shared_ptr<ScheduledEvent> scheduledEvent, SCXML::Runtime::RuntimeContext *context);

private:
    // Thread management
    std::thread schedulerThread_;
    std::atomic<bool> running_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;

    // Event storage
    std::priority_queue<std::shared_ptr<ScheduledEvent>, std::vector<std::shared_ptr<ScheduledEvent>>, EventComparator>
        eventQueue_;

    std::unordered_map<std::string, std::shared_ptr<ScheduledEvent>> eventLookup_;

    // Context for event delivery
    SCXML::Runtime::RuntimeContext *context_;
};
}  // namespace Runtime
}  // namespace SCXML
