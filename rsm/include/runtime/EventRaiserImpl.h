#pragma once

#include "IEventRaiser.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace RSM {

/**
 * @brief SCXML-compliant asynchronous implementation of IEventRaiser
 *
 * This class implements the SCXML "fire and forget" event model using
 * asynchronous event queues to prevent deadlocks and ensure proper
 * event processing order as specified by W3C SCXML standard.
 */
class EventRaiserImpl : public IEventRaiser {
public:
    using EventCallback = std::function<bool(const std::string &, const std::string &)>;

    /**
     * @brief W3C SCXML event priority for queue processing
     */
    enum class EventPriority {
        INTERNAL = 0,  // High priority - internal queue events (raise, send with target="#_internal")
        EXTERNAL = 1   // Low priority - external queue events (send without target or with external targets)
    };

    /**
     * @brief Event descriptor for queued events with W3C SCXML priority support
     */
    struct QueuedEvent {
        std::string eventName;
        std::string eventData;
        std::chrono::steady_clock::time_point timestamp;
        EventPriority priority;

        QueuedEvent(const std::string &name, const std::string &data, EventPriority prio = EventPriority::INTERNAL)
            : eventName(name), eventData(data), timestamp(std::chrono::steady_clock::now()), priority(prio) {}
    };

    /**
     * @brief Create an EventRaiser with optional callback
     * @param callback Optional event callback function
     */
    explicit EventRaiserImpl(EventCallback callback = nullptr);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~EventRaiserImpl();

    /**
     * @brief Set the event callback function
     * @param callback Function to call when events are raised
     */
    void setEventCallback(EventCallback callback);

    /**
     * @brief Clear the event callback
     */
    void clearEventCallback();

    /**
     * @brief Shutdown the async processing (for clean destruction)
     */
    void shutdown();

    // IEventRaiser interface
    bool raiseEvent(const std::string &eventName, const std::string &eventData) override;
    bool isReady() const override;
    void setImmediateMode(bool immediate) override;
    void processQueuedEvents() override;

    /**
     * @brief Internal method to raise event with specific priority (for W3C SCXML compliance)
     * @param eventName Name of the event to raise
     * @param eventData Data associated with the event
     * @param priority Event priority (INTERNAL or EXTERNAL)
     * @return true if the event was successfully queued, false if the raiser is not ready
     */
    bool raiseEventWithPriority(const std::string &eventName, const std::string &eventData, EventPriority priority);

private:
    /**
     * @brief Background worker thread for processing events
     */
    void eventProcessingWorker();

    /**
     * @brief Process a single event from the queue
     */
    void processEvent(const QueuedEvent &event);

    // Event callback
    EventCallback eventCallback_;
    mutable std::mutex callbackMutex_;

    // Asynchronous processing infrastructure
    std::queue<QueuedEvent> eventQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::thread processingThread_;
    std::atomic<bool> shutdownRequested_;
    std::atomic<bool> isRunning_;

    // SCXML compliance mode and synchronous queue
    std::atomic<bool> immediateMode_;
    std::queue<QueuedEvent> synchronousQueue_;
    std::mutex synchronousQueueMutex_;
};

}  // namespace RSM