#pragma once

#include "events/Event.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SCXML {
namespace Runtime {

/**
 * @brief Abstract base class for I/O Processors in SCXML
 *
 * I/O Processors enable SCXML state machines to communicate with external systems.
 * This follows the W3C SCXML specification for I/O Processor functionality.
 */
class IOProcessor {
public:
    using EventCallback = std::function<void(const std::shared_ptr<::SCXML::Events::Event> &)>;

    /**
     * @brief I/O Processor types as defined by W3C SCXML specification
     */
    enum class ProcessorType {
        HTTP,        // http://www.w3.org/TR/scxml/#HTTPEventProcessor
        BASIC_HTTP,  // http://www.w3.org/TR/scxml/#BasicHTTPEventProcessor
        SCXML,       // http://www.w3.org/TR/scxml/#SCXMLEventProcessor
        CUSTOM       // Custom I/O Processor
    };

    /**
     * @brief Send event parameters from SCXML <send> element
     */
    struct SendParameters {
        std::string event;                                    // Event name
        std::string target;                                   // Target endpoint/URI
        std::string type;                                     // I/O Processor type URI
        std::string content;                                  // Event content/payload
        std::unordered_map<std::string, std::string> params;  // Named parameters
        std::string sendId;                                   // Unique send identifier
        uint64_t delayMs = 0;                                 // Delay in milliseconds
        std::string namelist;                                 // Variable names to include
    };

    /**
     * @brief Constructor
     * @param processorType The type of this I/O Processor
     * @param typeURI The W3C specification URI for this processor type
     */
    IOProcessor(ProcessorType processorType, const std::string &typeURI);

    /**
     * @brief Virtual destructor
     */
    virtual ~IOProcessor() = default;

    /**
     * @brief Start the I/O Processor
     * @return true if started successfully
     */
    virtual bool start() = 0;

    /**
     * @brief Stop the I/O Processor
     * @return true if stopped successfully
     */
    virtual bool stop() = 0;

    /**
     * @brief Send an event through this I/O Processor
     * @param params Send parameters from SCXML <send> element
     * @return true if send was initiated successfully
     */
    virtual bool send(const SendParameters &params) = 0;

    /**
     * @brief Cancel a delayed send operation
     * @param sendId The send identifier to cancel
     * @return true if cancellation was successful
     */
    virtual bool cancelSend(const std::string &sendId) = 0;

    /**
     * @brief Check if this processor can handle the given type URI
     * @param typeURI The type URI to check
     * @return true if this processor can handle the type
     */
    virtual bool canHandle(const std::string &typeURI) const;

    /**
     * @brief Get the processor type
     * @return The processor type enum
     */
    ProcessorType getProcessorType() const {
        return processorType_;
    }

    /**
     * @brief Get the type URI for this processor
     * @return The W3C specification URI
     */
    const std::string &getTypeURI() const {
        return typeURI_;
    }

    /**
     * @brief Set callback for incoming events
     * @param callback Function to call when events are received
     */
    void setEventCallback(EventCallback callback) {
        eventCallback_ = callback;
    }

    /**
     * @brief Check if the processor is currently running
     * @return true if running
     */
    bool isRunning() const {
        return running_;
    }

    /**
     * @brief Get processor statistics
     */
    struct Statistics {
        uint64_t eventsSent = 0;
        uint64_t eventsReceived = 0;
        uint64_t sendFailures = 0;
        uint64_t receiveFailures = 0;
        uint64_t activeRequests = 0;
    };

    /**
     * @brief Get processor statistics
     * @return Current statistics
     */
    const Statistics &getStatistics() const {
        return stats_;
    }

    /**
     * @brief Reset processor statistics
     */
    void resetStatistics();

protected:
    /**
     * @brief Fire an incoming event to the state machine
     * @param event The event to fire
     */
    void fireIncomingEvent(const std::shared_ptr<::SCXML::Events::Event> &event);

    /**
     * @brief Update statistics counters (thread-safe)
     */
    void incrementEventsSent() {
        ++stats_.eventsSent;
    }

    void incrementEventsReceived() {
        ++stats_.eventsReceived;
    }

    void incrementSendFailures() {
        ++stats_.sendFailures;
    }

    void incrementReceiveFailures() {
        ++stats_.receiveFailures;
    }

    void incrementActiveRequests() {
        ++stats_.activeRequests;
    }

    void decrementActiveRequests() {
        if (stats_.activeRequests > 0) {
            --stats_.activeRequests;
        }
    }

    /**
     * @brief Create an error event for send failures
     * @param originalSendId The send ID that failed
     * @param errorType The type of error
     * @param errorMessage Human-readable error message
     * @return Error event to send to state machine
     */
    std::shared_ptr<::SCXML::Events::Event>
    createErrorEvent(const std::string &originalSendId, const std::string &errorType, const std::string &errorMessage);

    ProcessorType processorType_;
    std::string typeURI_;
    std::atomic<bool> running_{false};
    EventCallback eventCallback_;
    mutable std::mutex statsMutex_;
    Statistics stats_;
};

/**
 * @brief I/O Processor Manager for managing multiple I/O Processors
 */
class IOProcessorManager {
public:
    /**
     * @brief Get singleton instance
     * @return The global I/O Processor Manager instance
     */
    static IOProcessorManager &getInstance();

    /**
     * @brief Register an I/O Processor
     * @param processor The processor to register
     * @return true if registered successfully
     */
    bool registerProcessor(std::shared_ptr<IOProcessor> processor);

    /**
     * @brief Unregister an I/O Processor
     * @param typeURI The type URI of the processor to unregister
     * @return true if unregistered successfully
     */
    bool unregisterProcessor(const std::string &typeURI);

    /**
     * @brief Get I/O Processor by type URI
     * @param typeURI The type URI to look for
     * @return Processor if found, nullptr otherwise
     */
    std::shared_ptr<IOProcessor> getProcessor(const std::string &typeURI);

    /**
     * @brief Send event through appropriate I/O Processor
     * @param params Send parameters
     * @return true if send was initiated successfully
     */
    bool send(const IOProcessor::SendParameters &params);

    /**
     * @brief Cancel a delayed send operation
     * @param sendId The send identifier to cancel
     * @return true if cancellation was successful
     */
    bool cancelSend(const std::string &sendId);

    /**
     * @brief Start all registered I/O Processors
     * @return true if all processors started successfully
     */
    bool startAll();

    /**
     * @brief Stop all registered I/O Processors
     * @return true if all processors stopped successfully
     */
    bool stopAll();

    /**
     * @brief Get list of registered processor types
     * @return Vector of type URIs
     */
    std::vector<std::string> getRegisteredTypes() const;

    /**
     * @brief Set global event callback for all processors
     * @param callback Function to call when events are received
     */
    void setGlobalEventCallback(IOProcessor::EventCallback callback);

private:
    IOProcessorManager() = default;

    mutable std::mutex processorsMutex_;
    std::unordered_map<std::string, std::shared_ptr<IOProcessor>> processors_;
    std::unordered_map<std::string, std::string> sendIdToProcessorType_;  // Track which processor handles each sendId
    IOProcessor::EventCallback globalEventCallback_;
};

}  // namespace Runtime
}  // namespace SCXML