#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

// Events module integration
#include "events/Event.h"
#include "events/EventQueue.h"

namespace SCXML {
// Forward declarations
namespace Runtime {
class RuntimeContext;
}

namespace Runtime {

/**
 * @brief Bridge between Runtime and Events modules
 *
 * This class provides seamless integration between the SCXML Runtime
 * and the Events system, enabling proper event handling according to
 * the SCXML specification.
 */
class EventSystemBridge {
public:
    /**
     * @brief Event processing callback type
     * Called when an event is ready to be processed by the runtime
     */
    using EventProcessor = std::function<void(::SCXML::Events::EventPtr)>;

    /**
     * @brief Construct a new Event System Bridge
     */
    EventSystemBridge();

    /**
     * @brief Construct a new Event System Bridge
     * @param context Runtime context to integrate with
     */
    explicit EventSystemBridge(std::shared_ptr<SCXML::Runtime::RuntimeContext> context);

    /**
     * @brief Destructor
     */
    ~EventSystemBridge();

    /**
     * @brief Initialize the event system integration
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * @brief Set the runtime context for this bridge
     * @param context Runtime context to integrate with
     */
    void setRuntimeContext(std::shared_ptr<SCXML::Runtime::RuntimeContext> context);

    /**
     * @brief Shutdown the event system
     */
    void shutdown();

    /**
     * @brief Check if event system is initialized
     * @return true if initialized and ready
     */
    bool isInitialized() const;

    /**
     * @brief Send an external event to the state machine
     * @param eventName Name of the event
     * @param data Optional event data
     * @param sendId Optional sender identifier
     * @return true if event was successfully queued
     */
    bool sendEvent(const std::string &eventName, const ::SCXML::Events::EventData &data = std::monostate{},
                   const std::string &sendId = "");

    /**
     * @brief Raise an internal event within the state machine
     * @param eventName Name of the event
     * @param data Optional event data
     * @return true if event was successfully queued
     */
    bool raiseEvent(const std::string &eventName, const ::SCXML::Events::EventData &data = std::monostate{});

    /**
     * @brief Send a delayed event
     * @param eventName Name of the event
     * @param delayMs Delay in milliseconds
     * @param data Optional event data
     * @param sendId Optional sender identifier for cancellation
     * @return true if event was successfully scheduled
     */
    bool sendDelayedEvent(const std::string &eventName, int delayMs,
                          const ::SCXML::Events::EventData &data = std::monostate{}, const std::string &sendId = "");

    /**
     * @brief Cancel a delayed event
     * @param sendId Sender identifier used when scheduling the event
     * @return true if event was found and cancelled
     */
    bool cancelEvent(const std::string &sendId);

    /**
     * @brief Process next available event
     * @param timeoutMs Timeout for waiting for events (0 = no wait, -1 = infinite)
     * @return true if an event was processed
     */
    bool processNextEvent(int timeoutMs = 0);

    /**
     * @brief Start continuous event processing
     * @param processor Event processing callback
     * @return true if processing started successfully
     */
    bool startEventProcessing(EventProcessor processor);

    /**
     * @brief Stop continuous event processing
     */
    void stopEventProcessing();

    /**
     * @brief Check if event processing is active
     * @return true if actively processing events
     */
    bool isProcessingEvents() const;

    /**
     * @brief Get the internal event queue
     * @return Pointer to internal event queue
     */
    std::shared_ptr<::SCXML::Events::EventQueue> getInternalQueue() const;

    /**
     * @brief Get the external event queue
     * @return Pointer to external event queue
     */
    std::shared_ptr<::SCXML::Events::EventQueue> getExternalQueue() const;

    /**
     * @brief Get the event dispatcher
     * @return Pointer to event dispatcher
     */
    std::shared_ptr<::SCXML::Events::EventDispatcher> getDispatcher() const;

    /**
     * @brief Get queue statistics
     */
    struct QueueStats {
        size_t internalQueueSize = 0;
        size_t externalQueueSize = 0;
        size_t totalProcessed = 0;
        size_t totalDropped = 0;
    };

    /**
     * @brief Get current queue statistics
     * @return Current statistics
     */
    QueueStats getQueueStats() const;

    /**
     * @brief Clear all queued events
     */
    void clearAllEvents();

    /**
     * @brief Set maximum queue sizes
     * @param internalMaxSize Maximum size for internal queue
     * @param externalMaxSize Maximum size for external queue
     */
    void setQueueLimits(size_t internalMaxSize, size_t externalMaxSize);

    /**
     * @brief Create a done event for state completion
     * @param stateId State that completed
     * @param data Optional completion data
     * @return true if event was successfully raised
     */
    bool raiseDoneEvent(const std::string &stateId, const ::SCXML::Events::EventData &data = std::monostate{});

    /**
     * @brief Create an error event
     * @param errorType Type of error (e.g., "execution", "communication")
     * @param errorMessage Error description
     * @param data Optional error data
     * @return true if event was successfully raised
     */
    bool raiseErrorEvent(const std::string &errorType, const std::string &errorMessage,
                         const ::SCXML::Events::EventData &data = std::monostate{});

protected:
    /**
     * @brief Process a single event
     * @param event Event to process
     */
    void processEvent(::SCXML::Events::EventPtr event);

    /**
     * @brief Determine appropriate queue for event
     * @param eventType Type of event
     * @return Pointer to appropriate queue
     */
    std::shared_ptr<::SCXML::Events::EventQueue> selectQueue(::SCXML::Events::Event::Type eventType);

    /**
     * @brief Event processing thread main loop
     */
    void eventProcessingLoop();

private:
    // Runtime integration
    std::shared_ptr<SCXML::Runtime::RuntimeContext> runtimeContext_;

    // Event system components
    std::shared_ptr<::SCXML::Events::EventQueue> internalQueue_;
    std::shared_ptr<::SCXML::Events::EventQueue> externalQueue_;
    std::shared_ptr<::SCXML::Events::EventDispatcher> dispatcher_;

    // Processing state
    bool initialized_;
    bool processingActive_;
    EventProcessor eventProcessor_;

    // Threading support
    std::thread processingThread_;
    std::atomic<bool> shouldStop_;

    // Statistics
    mutable std::mutex statsMutex_;
    size_t totalProcessedEvents_;
    size_t totalDroppedEvents_;

    // Delayed event management
    struct DelayedEventInfo {
        std::shared_ptr<std::thread> timer;
        ::SCXML::Events::EventPtr event;
    };

    std::mutex delayedEventsMutex_;
    std::unordered_map<std::string, DelayedEventInfo> delayedEvents_;

    // Send ID generation
    std::atomic<size_t> sendIdCounter_;
    std::string generateSendId();
};

}  // namespace Runtime
}  // namespace SCXML