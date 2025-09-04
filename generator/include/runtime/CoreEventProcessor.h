#pragma once

#include "events/Event.h"
#include "events/EventQueue.h"
#include "runtime/RuntimeContext.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>

namespace SCXML {
namespace Events {

/**
 * @brief SCXML Event Processor - Core event processing engine for generated state machines
 *
 * This class implements the W3C SCXML event processing algorithm:
 * 1. Maintains internal and external event queues
 * 2. Processes events according to SCXML priority rules
 * 3. Handles delayed events and cancellation
 * 4. Integrates with generated state machine code
 *
 * Used by generated C++ code to provide proper SCXML-compliant event handling.
 */
class CoreEventProcessor {
public:
    /**
     * @brief Event processing callback type
     * Called when an event should be processed by the state machine
     * @param event The event to process
     * @return true if event was handled successfully
     */
    using EventHandler = std::function<bool(const SCXML::Events::EventPtr &)>;

    /**
     * @brief State change notification callback type
     * Called when state machine changes state
     * @param from Previous state (empty for initial)
     * @param to New state
     */
    using StateChangeCallback = std::function<void(const std::string &from, const std::string &to)>;

    /**
     * @brief Error callback type
     * Called when an error occurs during event processing
     * @param errorType Type of error
     * @param message Error message
     * @param event Event that caused the error (may be null)
     */
    using ErrorCallback = std::function<void(const std::string &errorType, const std::string &message,
                                             const SCXML::Events::EventPtr &event)>;

    /**
     * @brief Constructor
     */
    CoreEventProcessor();

    /**
     * @brief Destructor
     */
    ~CoreEventProcessor();

    // Non-copyable, non-movable
    CoreEventProcessor(const CoreEventProcessor &) = delete;
    CoreEventProcessor &operator=(const CoreEventProcessor &) = delete;
    CoreEventProcessor(CoreEventProcessor &&) = delete;
    CoreEventProcessor &operator=(CoreEventProcessor &&) = delete;

    // ========== Lifecycle Management ==========

    /**
     * @brief Start the event processor
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the event processor
     * @return true if stopped successfully
     */
    bool stop();

    /**
     * @brief Pause event processing
     * @return true if paused successfully
     */
    bool pause();

    /**
     * @brief Resume event processing
     * @return true if resumed successfully
     */
    bool resume();

    /**
     * @brief Check if event processor is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Check if event processor is paused
     * @return true if paused
     */
    bool isPaused() const;

    // ========== Event Queue Operations ==========

    /**
     * @brief Send external event to state machine
     * @param eventName Event name
     * @param data Event data (optional)
     * @param sendId Sender ID for tracking (optional)
     * @return true if event was queued successfully
     */
    bool sendEvent(const std::string &eventName, const std::string &data = "", const std::string &sendId = "");

    /**
     * @brief Raise internal event within state machine
     * @param eventName Event name
     * @param data Event data (optional)
     * @return true if event was queued successfully
     */
    bool raiseEvent(const std::string &eventName, const std::string &data = "");

    /**
     * @brief Send delayed event
     * @param eventName Event name
     * @param data Event data (optional)
     * @param delayMs Delay in milliseconds
     * @param sendId Sender ID for cancellation
     * @return true if delayed event was scheduled
     */
    bool sendDelayedEvent(const std::string &eventName, const std::string &data = "", uint64_t delayMs = 0,
                          const std::string &sendId = "");

    /**
     * @brief Cancel delayed event
     * @param sendId Sender ID used when scheduling
     * @return true if event was found and cancelled
     */
    bool cancelEvent(const std::string &sendId);

    /**
     * @brief Send event object directly
     * @param event Event to send
     * @param priority Event priority
     * @return true if event was queued successfully
     */
    bool sendEventDirect(SCXML::Events::EventPtr event,
                         SCXML::Events::EventQueue::Priority priority = SCXML::Events::EventQueue::Priority::NORMAL);

    // ========== Queue Status ==========

    /**
     * @brief Get number of pending events in all queues
     * @return Total number of pending events
     */
    size_t getPendingEventCount() const;

    /**
     * @brief Get number of pending internal events
     * @return Number of internal events
     */
    size_t getInternalEventCount() const;

    /**
     * @brief Get number of pending external events
     * @return Number of external events
     */
    size_t getExternalEventCount() const;

    /**
     * @brief Clear all pending events
     */
    void clearAllEvents();

    /**
     * @brief Clear only external events
     */
    void clearExternalEvents();

    // ========== Callbacks ==========

    /**
     * @brief Set event handler callback
     * @param handler Callback to handle events
     */
    void setEventHandler(EventHandler handler);

    /**
     * @brief Set state change callback
     * @param callback Callback for state changes
     */
    void setStateChangeCallback(StateChangeCallback callback);

    /**
     * @brief Set error callback
     * @param callback Callback for errors
     */
    void setErrorCallback(ErrorCallback callback);

    // ========== Configuration ==========

    /**
     * @brief Set maximum queue sizes
     * @param internalMaxSize Max internal queue size (0 = unlimited)
     * @param externalMaxSize Max external queue size (0 = unlimited)
     */
    void setMaxQueueSizes(size_t internalMaxSize, size_t externalMaxSize);

    /**
     * @brief Enable/disable event tracing
     * @param enabled True to enable tracing
     */
    void setEventTracing(bool enabled);

    /**
     * @brief Check if event tracing is enabled
     * @return true if tracing is enabled
     */
    bool isEventTracingEnabled() const;

    // ========== Statistics ==========

    /**
     * @brief Event processing statistics
     */
    struct Statistics {
        uint64_t totalEventsProcessed = 0;
        uint64_t totalInternalEvents = 0;
        uint64_t totalExternalEvents = 0;
        uint64_t totalDelayedEvents = 0;
        uint64_t totalCancelledEvents = 0;
        uint64_t totalDroppedEvents = 0;
        uint64_t totalErrorEvents = 0;
        std::chrono::milliseconds totalProcessingTime{0};
        std::chrono::milliseconds averageProcessingTime{0};
    };

    /**
     * @brief Get processing statistics
     * @return Current statistics
     */
    Statistics getStatistics() const;

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

    // ========== Advanced Features ==========

    /**
     * @brief Set event filter for external events
     * @param filter Filter function (nullptr to remove)
     */
    void setEventFilter(SCXML::Events::EventQueue::EventFilter filter);

    /**
     * @brief Process single event immediately (for testing)
     * @param event Event to process
     * @return true if event was handled
     */
    bool processEventImmediate(SCXML::Events::EventPtr event);

    /**
     * @brief Process all pending events (for testing)
     * @return Number of events processed
     */
    size_t processAllEvents();

private:
    // ========== Internal State ==========

    enum class State { STOPPED, STARTING, RUNNING, PAUSED, STOPPING };

    mutable std::mutex mutex_;                       // Mutex for thread safety
    std::atomic<State> state_;                       // Current processor state
    std::unique_ptr<std::thread> processingThread_;  // Event processing thread
    std::condition_variable stateCondition_;         // Condition for state changes

    // Event Queues (SCXML requires separate internal/external queues)
    std::unique_ptr<SCXML::Events::EventQueue> internalQueue_;  // Internal events (high priority)
    std::unique_ptr<SCXML::Events::EventQueue> externalQueue_;  // External events (normal priority)

    // Delayed Events
    struct DelayedEventEntry {
        SCXML::Events::EventPtr event;
        std::chrono::steady_clock::time_point executeTime;
        std::string sendId;
        bool cancelled = false;
    };

    std::vector<DelayedEventEntry> delayedEvents_;  // Delayed events list
    mutable std::mutex delayedEventsMutex_;         // Delayed events mutex

    // Callbacks
    EventHandler eventHandler_;                // Event processing callback
    StateChangeCallback stateChangeCallback_;  // State change callback
    ErrorCallback errorCallback_;              // Error callback

    // Configuration
    bool eventTracing_ = false;  // Event tracing enabled

    // Statistics
    mutable std::mutex statisticsMutex_;  // Statistics mutex
    mutable Statistics statistics_;       // Processing statistics

    // ========== Internal Methods ==========

    /**
     * @brief Main event processing loop
     */
    void processingLoop();

    /**
     * @brief Process single event from queues
     * @return true if an event was processed
     */
    bool processNextEvent();

    /**
     * @brief Process delayed events that are ready
     * @return Number of delayed events processed
     */
    size_t processDelayedEvents();

    /**
     * @brief Handle event processing
     * @param event Event to process
     * @return true if event was handled successfully
     */
    bool handleEvent(const SCXML::Events::EventPtr &event);

    /**
     * @brief Report error
     * @param errorType Error type
     * @param message Error message
     * @param event Event that caused error (optional)
     */
    void reportError(const std::string &errorType, const std::string &message,
                     const SCXML::Events::EventPtr &event = nullptr);

    /**
     * @brief Update processing statistics
     * @param processingTime Time taken to process event
     */
    void updateStatistics(std::chrono::microseconds processingTime);

    /**
     * @brief Generate unique send ID
     * @return Unique send ID string
     */
    std::string generateSendId();

    /**
     * @brief Wait for state change
     * @param expectedState State to wait for
     * @param timeout_ms Timeout in milliseconds
     * @return true if state reached within timeout
     */
    bool waitForState(State expectedState, int timeout_ms = 5000);
};

/**
 * @brief Convenience function to create event processor
 * @return Unique pointer to new event processor
 */
std::unique_ptr<CoreEventProcessor> createEventProcessor();

}  // namespace Events
}  // namespace SCXML