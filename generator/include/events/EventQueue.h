#pragma once

#include "Event.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace SCXML {
namespace Events {

/**
 * @brief Thread-safe event queue for SCXML processors
 *
 * Provides FIFO event storage with priority support and thread-safe operations.
 */
class EventQueue {
public:
    /**
     * @brief Event priority levels
     */
    enum class Priority { LOW = 0, NORMAL = 1, HIGH = 2 };

    /**
     * @brief Event filter function type
     */
    using EventFilter = std::function<bool(std::shared_ptr<Event>)>;

    /**
     * @brief Constructor
     */
    EventQueue();

    /**
     * @brief Destructor
     */
    ~EventQueue();

    /**
     * @brief Enqueue event with normal priority
     * @param event Event to enqueue
     */
    void enqueue(std::shared_ptr<Event> event);

    /**
     * @brief Enqueue event with specific priority
     * @param event Event to enqueue
     * @param priority Event priority
     */
    void enqueue(std::shared_ptr<Event> event, Priority priority);

    /**
     * @brief Dequeue next event (blocking)
     * @return Next event or nullptr if queue is shutdown
     */
    std::shared_ptr<Event> dequeue();

    /**
     * @brief Try to dequeue event (non-blocking)
     * @param event Output parameter for dequeued event
     * @return true if event was dequeued, false if queue is empty
     */
    bool tryDequeue(std::shared_ptr<Event> &event);

    /**
     * @brief Get queue size
     * @return Number of events in queue
     */
    size_t size() const;

    /**
     * @brief Check if queue is empty
     * @return true if queue is empty
     */
    bool empty() const;

    /**
     * @brief Clear all events from queue
     */
    void clear();

    /**
     * @brief Shutdown queue (unblocks waiting dequeuers)
     */
    void shutdown();

    /**
     * @brief Check if queue is shutdown
     * @return true if queue is shutdown
     */
    bool isShutdown() const;

private:
    struct PriorityEvent {
        std::shared_ptr<Event> event;
        Priority priority;
        uint64_t sequence;  // For FIFO within same priority

        bool operator<(const PriorityEvent &other) const {
            if (priority != other.priority) {
                return priority < other.priority;  // Higher priority first
            }
            return sequence > other.sequence;  // Earlier sequence first (FIFO)
        }
    };

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::priority_queue<PriorityEvent> queue_;
    std::atomic<uint64_t> sequence_;
    std::atomic<bool> shutdown_;
};

/**
 * @brief Event dispatcher for external event handling
 *
 * Manages event distribution to external systems and processors.
 */
class EventDispatcher {
public:
    /**
     * @brief Constructor
     */
    EventDispatcher();

    /**
     * @brief Destructor
     */
    ~EventDispatcher();

    /**
     * @brief Dispatch event to external target
     * @param event Event to dispatch
     * @param target Target identifier
     * @return true if dispatch was successful
     */
    bool dispatch(std::shared_ptr<Event> event, const std::string &target);

    /**
     * @brief Register event handler
     * @param target Target identifier
     * @param handler Handler function
     */
    void registerHandler(const std::string &target, std::function<bool(std::shared_ptr<Event>)> handler);

    /**
     * @brief Unregister event handler
     * @param target Target identifier
     */
    void unregisterHandler(const std::string &target);

    /**
     * @brief Get registered targets
     * @return Vector of registered target identifiers
     */
    std::vector<std::string> getRegisteredTargets() const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::function<bool(std::shared_ptr<Event>)>> handlers_;
};

}  // namespace Events
}  // namespace SCXML