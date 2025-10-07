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
    using EventCallbackWithOrigin = std::function<bool(const std::string &, const std::string &, const std::string &)>;

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
        std::string originSessionId;  // W3C SCXML 6.4: Session that originated this event (for finalize)
        std::string sendId;           // W3C SCXML 5.10: sendid from failed send element (for error events)
        std::string invokeId;         // W3C SCXML 5.10: invokeid from invoked child process (test 338)
        std::string originType;       // W3C SCXML 5.10: origintype from event processor type (test 253, 331, 352, 372)
        std::chrono::steady_clock::time_point timestamp;
        EventPriority priority;

        QueuedEvent(const std::string &name, const std::string &data, EventPriority prio = EventPriority::INTERNAL,
                    const std::string &origin = "", const std::string &sid = "", const std::string &iid = "",
                    const std::string &otype = "")
            : eventName(name), eventData(data), originSessionId(origin), sendId(sid), invokeId(iid), originType(otype),
              timestamp(std::chrono::steady_clock::now()), priority(prio) {}
    };

    /**
     * @brief Comparator for priority queue - orders by priority (INTERNAL first) then timestamp (FIFO)
     * Note: std::priority_queue is a max-heap, so we invert the comparison
     */
    struct QueuedEventComparator {
        bool operator()(const QueuedEvent &a, const QueuedEvent &b) const {
            // Invert comparison for max-heap: we want INTERNAL (0) before EXTERNAL (1)
            if (a.priority != b.priority) {
                return a.priority > b.priority;  // Lower priority value = higher actual priority
            }
            // For same priority, older timestamp should come first (FIFO)
            return a.timestamp > b.timestamp;  // Older timestamp = lower in heap
        }
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
    bool raiseEvent(const std::string &eventName, const std::string &eventData,
                    const std::string &originSessionId) override;
    bool raiseEvent(const std::string &eventName, const std::string &eventData, const std::string &sendId,
                    bool) override;
    bool raiseEvent(const std::string &eventName, const std::string &eventData, const std::string &originSessionId,
                    const std::string &invokeId) override;
    bool raiseEvent(const std::string &eventName, const std::string &eventData, const std::string &originSessionId,
                    const std::string &invokeId, const std::string &originType) override;
    bool raiseInternalEvent(const std::string &eventName, const std::string &eventData) override;
    bool raiseExternalEvent(const std::string &eventName, const std::string &eventData) override;
    bool isReady() const override;
    void setImmediateMode(bool immediate) override;

    /**
     * @brief Check if immediate mode is currently enabled
     * @return true if immediate mode is enabled, false otherwise
     */
    bool isImmediateModeEnabled() const {
        return immediateMode_.load();
    }

    void processQueuedEvents() override;

    /**
     * @brief W3C SCXML compliance: Process only ONE event from the queue
     * @return true if an event was processed, false if queue is empty
     */
    bool processNextQueuedEvent();

    /**
     * @brief Check if there are queued events waiting to be processed
     * @return true if queue has events, false if empty
     */
    bool hasQueuedEvents() const;

    /**
     * @brief Internal method to raise event with specific priority (for W3C SCXML compliance)
     * @param eventName Name of the event to raise
     * @param eventData Data associated with the event
     * @param priority Event priority (INTERNAL or EXTERNAL)
     * @param originSessionId Session ID that originated this event (for finalize)
     * @param sendId Send ID from failed send element (for error events)
     * @param invokeId Invoke ID from invoked child process (test 338)
     * @return true if the event was successfully queued, false if the raiser is not ready
     */
    bool raiseEventWithPriority(const std::string &eventName, const std::string &eventData, EventPriority priority,
                                const std::string &originSessionId = "", const std::string &sendId = "",
                                const std::string &invokeId = "", const std::string &originType = "");

private:
    /**
     * @brief Background worker thread for processing events
     */
    void eventProcessingWorker();

    /**
     * @brief Process a single event from the queue
     */
    void processEvent(const QueuedEvent &event);

    /**
     * @brief Execute callback for a queued event (synchronous processing)
     * @param event Event to process
     * @return true if callback was executed successfully, false otherwise
     */
    bool executeEventCallback(const QueuedEvent &event);

    // Event callback
    EventCallback eventCallback_;

    // W3C SCXML 6.4: Thread-local storage for origin session ID during callback execution
    static thread_local std::string currentOriginSessionId_;

    // W3C SCXML 5.10: Thread-local storage for send ID from failed send elements (for error events)
    static thread_local std::string currentSendId_;

    // W3C SCXML 5.10: Thread-local storage for invoke ID from invoked child processes (test 338)
    static thread_local std::string currentInvokeId_;

    // W3C SCXML 5.10: Thread-local storage for origin type from event processor (test 253, 331, 352, 372)
    static thread_local std::string currentOriginType_;

    // W3C SCXML 5.10: Thread-local storage for event type ("internal", "platform", "external") (test 331)
    static thread_local std::string currentEventType_;

public:
    /**
     * @brief Get current origin session ID (for W3C SCXML 6.4 finalize support)
     * This is set during event callback execution to allow StateMachine to identify event origin
     * @return Origin session ID if set, empty string otherwise
     */
    static std::string getCurrentOriginSessionId() {
        return currentOriginSessionId_;
    }

    /**
     * @brief Get current send ID (for W3C SCXML 5.10 error event compliance)
     * This is set during error event callback execution to allow StateMachine to set event.sendid
     * @return Send ID if set, empty string otherwise
     */
    static std::string getCurrentSendId() {
        return currentSendId_;
    }

    /**
     * @brief Get current invoke ID (for W3C SCXML 5.10 test 338 compliance)
     * This is set during event callback execution from invoked children to allow StateMachine to set event.invokeid
     * @return Invoke ID if set, empty string otherwise
     */
    static std::string getCurrentInvokeId() {
        return currentInvokeId_;
    }

    /**
     * @brief Get current origin type (for W3C SCXML 5.10 origintype field compliance)
     * This is set during event callback execution to allow StateMachine to set event.origintype
     * @return Origin type if set, empty string otherwise
     */
    static std::string getCurrentOriginType() {
        return currentOriginType_;
    }

    /**
     * @brief Get current event type (for W3C SCXML 5.10 event type field compliance)
     * This is set during event callback execution to allow StateMachine to set event.type correctly
     * @return Event type ("internal", "platform", "external") if set, empty string otherwise
     */
    static std::string getCurrentEventType() {
        return currentEventType_;
    }

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
    std::priority_queue<QueuedEvent, std::vector<QueuedEvent>, QueuedEventComparator> synchronousQueue_;
    mutable std::mutex synchronousQueueMutex_;
};

}  // namespace RSM