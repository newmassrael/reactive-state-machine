#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
namespace SCXML {

namespace Runtime {
class RuntimeContext;
}

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {

/**
 * @brief Central manager for SCXML invoke operations
 *
 * The InvokeManager coordinates all invoke operations in an SCXML state machine,
 * managing multiple invoke processors and handling the lifecycle of invoked services.
 * It provides the main interface for states to invoke external services and
 * manages communication between parent and child sessions.
 */
class InvokeManager {
public:
    /**
     * @brief Invoke operation result
     */
    struct InvokeResult {
        bool success = false;
        std::string sessionId;
        std::string errorMessage;
        std::string processorType;
    };

    /**
     * @brief Active invoke session information
     */
    struct ActiveInvoke {
        std::string sessionId;
        std::string invokeId;
        std::string stateId;  // State that created this invoke
        std::string processorType;
        std::shared_ptr<SCXML::Model::IInvokeNode> invokeNode;
        std::chrono::steady_clock::time_point startTime;
        bool autoForward;

        ActiveInvoke(const std::string &sid, const std::string &iid, const std::string &state)
            : sessionId(sid), invokeId(iid), stateId(state), autoForward(false) {
            startTime = std::chrono::steady_clock::now();
        }
    };

    /**
     * @brief Event filter for auto-forwarding
     */
    using EventFilter = std::function<bool(SCXML::Events::EventPtr event, const std::string &sessionId)>;

    /**
     * @brief Callback for invoke lifecycle events
     */
    using InvokeEventCallback =
        std::function<void(const std::string &invokeId, const std::string &eventType, const std::string &data)>;

public:
    /**
     * @brief Constructor
     */
    InvokeManager();

    /**
     * @brief Destructor
     */
    virtual ~InvokeManager();

    /**
     * @brief Initialize invoke manager with runtime context
     * @param context Runtime context for parent state machine
     */
    void initialize(SCXML::Runtime::RuntimeContext *context);

    /**
     * @brief Register an invoke processor
     * @param processor Invoke processor to register
     * @return true if registration successful
     */
    bool registerProcessor(std::shared_ptr<SCXML::Model::InvokeProcessor> processor);

    /**
     * @brief Unregister an invoke processor
     * @param processorType Type of processor to unregister
     * @return true if unregistration successful
     */
    bool unregisterProcessor(const std::string &processorType);

    /**
     * @brief Start an invoke operation
     * @param invokeNode Invoke node containing parameters
     * @param stateId ID of the state initiating the invoke
     * @return Invoke result with session information
     */
    InvokeResult startInvoke(std::shared_ptr<SCXML::Model::IInvokeNode> invokeNode, const std::string &stateId);

    /**
     * @brief Send event to an invoked service
     * @param invokeId Invoke ID or session ID
     * @param event Event to send
     * @return true if event was sent successfully
     */
    bool sendEventToInvoke(const std::string &invokeId, SCXML::Events::EventPtr event);

    /**
     * @brief Terminate an invoke operation
     * @param invokeId Invoke ID to terminate
     * @return true if termination was successful
     */
    bool terminateInvoke(const std::string &invokeId);

    /**
     * @brief Terminate all invokes for a specific state
     * @param stateId State ID
     * @return Number of invokes terminated
     */
    size_t terminateInvokesForState(const std::string &stateId);

    /**
     * @brief Get active invoke information
     * @param invokeId Invoke ID to query
     * @return Active invoke info if found, nullptr otherwise
     */
    std::shared_ptr<ActiveInvoke> getActiveInvoke(const std::string &invokeId) const;

    /**
     * @brief Get all active invokes
     * @return Map of invoke ID to active invoke info
     */
    std::unordered_map<std::string, std::shared_ptr<ActiveInvoke>> getAllActiveInvokes() const;

    /**
     * @brief Get active invokes for a specific state
     * @param stateId State ID to query
     * @return Vector of active invokes for the state
     */
    std::vector<std::shared_ptr<ActiveInvoke>> getInvokesForState(const std::string &stateId) const;

    /**
     * @brief Check if an invoke is active
     * @param invokeId Invoke ID to check
     * @return true if invoke is active
     */
    bool isInvokeActive(const std::string &invokeId) const;

    /**
     * @brief Set event filter for auto-forwarding
     * @param filter Function to filter events for auto-forward
     */
    void setEventFilter(EventFilter filter) {
        eventFilter_ = filter;
    }

    /**
     * @brief Set invoke event callback
     * @param callback Function to call for invoke lifecycle events
     */
    void setInvokeEventCallback(InvokeEventCallback callback) {
        invokeEventCallback_ = callback;
    }

    /**
     * @brief Process parent event for auto-forwarding
     * @param event Event from parent state machine
     * @return Number of child sessions the event was forwarded to
     */
    size_t processParentEvent(SCXML::Events::EventPtr event);

    /**
     * @brief Get invoke manager statistics
     */
    struct Statistics {
        size_t totalInvokes = 0;
        size_t activeInvokes = 0;
        size_t successfulInvokes = 0;
        size_t failedInvokes = 0;
        size_t eventsForwarded = 0;
        size_t eventsReceived = 0;
        std::unordered_map<std::string, size_t> processorUsage;
    };

    /**
     * @brief Get current statistics
     * @return Invoke manager statistics
     */
    const Statistics &getStatistics() const {
        return statistics_;
    }

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

    /**
     * @brief Start all registered processors
     * @return true if all processors started successfully
     */
    bool startAllProcessors();

    /**
     * @brief Stop all processors and terminate all invokes
     * @return true if cleanup completed successfully
     */
    bool stopAllProcessors();

    /**
     * @brief Get list of registered processor types
     * @return Vector of processor type names
     */
    std::vector<std::string> getRegisteredProcessorTypes() const;

protected:
    /**
     * @brief Find appropriate processor for invoke type
     * @param type Invoke type string
     * @return Processor if found, nullptr otherwise
     */
    std::shared_ptr<SCXML::Model::InvokeProcessor> findProcessorForType(const std::string &type) const;

    /**
     * @brief Handle event from child session
     * @param sessionId Source session ID
     * @param event Event from child
     */
    void handleChildEvent(const std::string &sessionId, SCXML::Events::EventPtr event);

    /**
     * @brief Handle session state change
     * @param sessionId Session that changed state
     * @param oldState Previous state
     * @param newState New state
     */
    void handleSessionStateChange(const std::string &sessionId, int oldState, int newState);

    /**
     * @brief Generate unique invoke ID
     * @param stateId State creating the invoke
     * @return Unique invoke ID
     */
    std::string generateInvokeId(const std::string &stateId);

    /**
     * @brief Create done.invoke event for completed invokes
     * @param invokeId Completed invoke ID
     * @param data Completion data
     * @return Done event
     */
    SCXML::Events::EventPtr createDoneInvokeEvent(const std::string &invokeId, const std::string &data = "");

    /**
     * @brief Create error.invoke event for failed invokes
     * @param invokeId Failed invoke ID
     * @param errorMessage Error description
     * @return Error event
     */
    SCXML::Events::EventPtr createErrorInvokeEvent(const std::string &invokeId, const std::string &errorMessage);

private:
    // Core components
    SCXML::Runtime::RuntimeContext *context_;

    // Processor management
    std::unordered_map<std::string, std::shared_ptr<SCXML::Model::InvokeProcessor>> processors_;
    mutable std::mutex processorsMutex_;

    // Active invoke tracking
    std::unordered_map<std::string, std::shared_ptr<ActiveInvoke>> activeInvokes_;
    std::unordered_map<std::string, std::string> sessionToInvokeMap_;  // session ID -> invoke ID
    mutable std::mutex invokesMutex_;

    // Event handling
    EventFilter eventFilter_;
    InvokeEventCallback invokeEventCallback_;

    // Statistics
    mutable Statistics statistics_;
    mutable std::mutex statisticsMutex_;

    // ID generation
    std::atomic<uint64_t> invokeCounter_{0};

    /**
     * @brief Setup processor callbacks
     * @param processor Processor to setup callbacks for
     */
    void setupProcessorCallbacks(std::shared_ptr<SCXML::Model::InvokeProcessor> processor);

    /**
     * @brief Update statistics
     */
    void incrementTotalInvokes() {
        statistics_.totalInvokes++;
    }

    void incrementActiveInvokes() {
        statistics_.activeInvokes++;
    }

    void decrementActiveInvokes() {
        if (statistics_.activeInvokes > 0) {
            statistics_.activeInvokes--;
        }
    }

    void incrementSuccessfulInvokes() {
        statistics_.successfulInvokes++;
    }

    void incrementFailedInvokes() {
        statistics_.failedInvokes++;
    }

    void incrementEventsForwarded() {
        statistics_.eventsForwarded++;
    }

    void incrementEventsReceived() {
        statistics_.eventsReceived++;
    }
};
}  // namespace Runtime
}  // namespace SCXML
