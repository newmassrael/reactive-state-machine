#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace SCXML {
namespace Events {
class Event;
}

namespace Runtime {

/**
 * @brief OS-independent, non-blocking event scheduler for delayed event delivery
 *
 * Uses std::chrono for accurate timing without sleep or blocking calls.
 * Events are processed on each event loop iteration.
 */
class EventScheduler {
public:
    /**
     * @brief Delayed event container
     */
    struct DelayedEvent {
        std::chrono::steady_clock::time_point targetTime;
        std::shared_ptr<SCXML::Events::Event> event;
        std::string target;
        std::string sendId;

        DelayedEvent(std::chrono::steady_clock::time_point time, std::shared_ptr<SCXML::Events::Event> evt,
                     const std::string &tgt = std::string(), const std::string &id = std::string())
            : targetTime(time), event(evt), target(tgt), sendId(id) {}

        // For priority queue (earliest events first)
        bool operator<(const DelayedEvent &other) const {
            return targetTime > other.targetTime;  // Reverse comparison for min-heap
        }
    };

public:
    EventScheduler() = default;
    ~EventScheduler() = default;

    /**
     * @brief Schedule an event for delayed delivery
     * @param event Event to deliver
     * @param delayMs Delay in milliseconds
     * @param target Target for event delivery (empty for internal events)
     * @param sendId Optional send ID for cancellation
     */
    void scheduleEvent(std::shared_ptr<SCXML::Events::Event> event, uint64_t delayMs,
                       const std::string &target = std::string(), const std::string &sendId = std::string());

    /**
     * @brief Get all events that are ready to be delivered now
     * @return Vector of ready events (removed from scheduler)
     */
    std::vector<DelayedEvent> getReadyEvents();

    /**
     * @brief Cancel a scheduled event by send ID
     * @param sendId Send ID to cancel
     * @return true if event was found and cancelled
     */
    bool cancelEvent(const std::string &sendId);

    /**
     * @brief Get number of scheduled events
     * @return Number of pending delayed events
     */
    size_t getScheduledEventCount() const {
        return delayedEvents_.size();
    }

    /**
     * @brief Clear all scheduled events
     */
    void clear() {
        delayedEvents_ = std::priority_queue<DelayedEvent>();
    }

    /**
     * @brief Check if there are any events ready to be delivered
     * @return true if events are ready
     */
    bool hasReadyEvents() const;

    /**
     * @brief Get time until next event (for optimization)
     * @return Duration until next event, or max if no events
     */
    std::chrono::milliseconds getTimeUntilNextEvent() const;

private:
    std::priority_queue<DelayedEvent> delayedEvents_;
};

}  // namespace Runtime
}  // namespace SCXML