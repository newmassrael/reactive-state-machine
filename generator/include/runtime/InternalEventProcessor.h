#pragma once

#include "runtime/IOProcessor.h"
#include <condition_variable>
#include <queue>
#include <thread>
#include <unordered_map>

namespace SCXML {
// Forward declarations
class Processor;

namespace Runtime {

/**
 * @brief SCXML I/O Processor for inter-machine communication
 *
 * The SCXML I/O Processor enables communication between SCXML state machines.
 * This follows the W3C SCXML specification for SCXML Event Processor functionality.
 *
 * Key features:
 * - Direct event passing between SCXML instances
 * - Session management for persistent connections
 * - Event queuing and delivery confirmation
 * - Target resolution (by session ID, name, or URI)
 */
class InternalEventProcessor : public IOProcessor {
public:
    /**
     * @brief Configuration for SCXML I/O Processor
     */
    struct Config {
        std::string sessionId;                   // Unique session identifier for this instance
        std::string instanceName;                // Human-readable instance name
        uint32_t maxEventQueueSize = 1000;       // Max events in delivery queue
        uint32_t eventDeliveryTimeoutMs = 5000;  // Timeout for event delivery
        bool enableEventConfirmation = true;     // Enable delivery confirmation
        bool enableSessionDiscovery = true;      // Enable automatic session discovery
        uint32_t sessionHeartbeatMs = 30000;     // Session heartbeat interval
        uint32_t maxConcurrentSessions = 100;    // Max concurrent session connections
    };

    /**
     * @brief Session information for connected SCXML instances
     */
    struct SessionInfo {
        std::string sessionId;
        std::string instanceName;
        std::string targetURI;
        std::chrono::steady_clock::time_point lastHeartbeat;
        bool isActive = true;
        uint64_t eventsSent = 0;
        uint64_t eventsReceived = 0;
    };

private:
    /**
     * @brief Pending SCXML event delivery
     */
    struct PendingDelivery {
        std::string deliveryId;
        std::string targetSessionId;
        Events::EventPtr event;
        SendParameters originalParams;
        std::chrono::steady_clock::time_point scheduledTime;
        uint32_t retryCount = 0;
        bool confirmationRequired = false;
        bool cancelled = false;
    };

    /**
     * @brief Event delivery confirmation
     */
    struct DeliveryConfirmation {
        std::string deliveryId;
        bool delivered;
        std::string errorMessage;
        std::chrono::steady_clock::time_point timestamp;
    };

public:
    /**
     * @brief Constructor with default configuration
     */
    InternalEventProcessor();

    /**
     * @brief Constructor with custom configuration
     * @param config SCXML processor configuration
     */
    explicit InternalEventProcessor(const Config &config);

    /**
     * @brief Destructor
     */
    ~InternalEventProcessor() override;

    /**
     * @brief Start the SCXML I/O Processor
     * @return true if started successfully
     */
    bool start() override;

    /**
     * @brief Stop the SCXML I/O Processor
     * @return true if stopped successfully
     */
    bool stop() override;

    /**
     * @brief Send an event to another SCXML instance
     * @param params Send parameters from SCXML <send> element
     * @return true if send was initiated successfully
     */
    bool send(const SendParameters &params) override;

    /**
     * @brief Cancel a pending SCXML event delivery
     * @param sendId The send identifier to cancel
     * @return true if cancellation was successful
     */
    bool cancelSend(const std::string &sendId) override;

    /**
     * @brief Check if this processor can handle the given type URI
     * @param typeURI The type URI to check
     * @return true if this processor can handle the type
     */
    bool canHandle(const std::string &typeURI) const override;

    /**
     * @brief Register an SCXML runtime instance for direct communication
     * @param runtime The runtime instance to register
     * @return true if registered successfully
     */
    bool registerRuntime(std::shared_ptr<Processor> runtime);

    /**
     * @brief Unregister an SCXML runtime instance
     * @param sessionId The session ID of the runtime to unregister
     * @return true if unregistered successfully
     */
    bool unregisterRuntime(const std::string &sessionId);

    /**
     * @brief Get information about active sessions
     * @return Vector of session information
     */
    std::vector<SessionInfo> getActiveSessions() const;

    /**
     * @brief Get session information by session ID
     * @param sessionId The session ID to look up
     * @return Session info if found, nullptr otherwise
     */
    std::shared_ptr<SessionInfo> getSession(const std::string &sessionId) const;

    /**
     * @brief Update processor configuration (processor must be stopped)
     * @param config New configuration
     * @return true if configuration was updated
     */
    bool updateConfig(const Config &config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const Config &getConfig() const {
        return config_;
    }

    /**
     * @brief Get this processor's session ID
     * @return Session ID
     */
    const std::string &getSessionId() const {
        return config_.sessionId;
    }

private:
    /**
     * @brief Execute SCXML event delivery
     * @param params Send parameters
     */
    void executeSCXMLDelivery(const SendParameters &params);

    /**
     * @brief Deliver event to target runtime
     * @param targetSessionId Target session ID
     * @param event Event to deliver
     * @param confirmationRequired Whether delivery confirmation is needed
     * @return Delivery ID if successful, empty string otherwise
     */
    std::string deliverEventToTarget(const std::string &targetSessionId, const Events::EventPtr &event,
                                     bool confirmationRequired = false);

    /**
     * @brief Process event delivery queue
     */
    void processDeliveryQueue();

    /**
     * @brief Process session heartbeats
     */
    void processSessionHeartbeats();

    /**
     * @brief Perform heartbeat maintenance tasks
     */
    void performHeartbeatMaintenance();

    /**
     * @brief Handle delivery confirmation
     * @param confirmation Delivery confirmation details
     */
    void handleDeliveryConfirmation(const DeliveryConfirmation &confirmation);

    /**
     * @brief Resolve target from SCXML target string
     * @param target Target specification (session ID, name, or URI)
     * @return Resolved session ID, empty if not found
     */
    std::string resolveTarget(const std::string &target) const;

    /**
     * @brief Create SCXML event from send parameters
     * @param params Send parameters
     * @return Created event
     */
    Events::EventPtr createSCXMLEvent(const SendParameters &params) const;

    /**
     * @brief Generate unique delivery ID
     * @return Unique delivery identifier
     */
    std::string generateDeliveryId() const;

    /**
     * @brief Generate unique session ID
     * @return Unique session identifier
     */
    std::string generateSessionId() const;

    /**
     * @brief Update session heartbeat
     * @param sessionId Session to update
     */
    void updateSessionHeartbeat(const std::string &sessionId);

    /**
     * @brief Check if session is alive based on heartbeat
     * @param session Session to check
     * @return true if session is considered alive
     */
    bool isSessionAlive(const SessionInfo &session) const;

    Config config_;

    // Runtime management
    mutable std::mutex runtimesMutex_;
    std::unordered_map<std::string, std::weak_ptr<Processor>> registeredRuntimes_;
    std::unordered_map<std::string, std::shared_ptr<SessionInfo>> sessions_;
    std::unordered_map<std::string, std::string> nameToSessionId_;  // Name lookup

    // Event delivery management
    mutable std::mutex deliveryQueueMutex_;
    std::condition_variable deliveryQueueCondition_;
    std::queue<PendingDelivery> deliveryQueue_;
    std::unordered_map<std::string, PendingDelivery> pendingDeliveries_;

    // Delivery confirmation handling
    mutable std::mutex confirmationsMutex_;
    std::unordered_map<std::string, DeliveryConfirmation> deliveryConfirmations_;

    // Background threads
    std::thread deliveryThread_;
    std::thread heartbeatThread_;

    // Heartbeat synchronization
    mutable std::mutex heartbeatMutex_;
    std::condition_variable heartbeatCondition_;

    // Graceful shutdown synchronization
    mutable std::mutex shutdownMutex_;
    std::condition_variable shutdownCondition_;

    // ID generation
    mutable std::mutex idMutex_;
    mutable std::atomic<uint64_t> nextDeliveryId_{1};
    std::atomic<uint64_t> nextSessionId_{1};

    // Shutdown management
    std::atomic<bool> shutdownRequested_{false};
};

}  // namespace Runtime
}  // namespace SCXML