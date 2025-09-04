#pragma once

#include "common/Result.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace SCXML {
// Forward declarations
namespace Runtime {
class RuntimeContext;
}
class LoadedModule;

/**
 * Inter-module message structure
 */
struct InterModuleMessage {
    std::string sourceModuleId;                       // Source module identifier
    std::string targetModuleId;                       // Target module identifier (empty for broadcast)
    std::string messageType;                          // Message type (event, data, control, etc.)
    std::string eventName;                            // Event name for event-type messages
    nlohmann::json messageData;                       // Message payload
    std::chrono::steady_clock::time_point timestamp;  // Message timestamp
    std::string correlationId;                        // For request-response correlation
    bool requiresResponse;                            // True if message requires response
    int priority;                                     // Message priority (higher = more priority)

    InterModuleMessage() : timestamp(std::chrono::steady_clock::now()), requiresResponse(false), priority(0) {}

    InterModuleMessage(const std::string &source, const std::string &target, const std::string &type,
                       const std::string &event = "", const nlohmann::json &data = nlohmann::json{})
        : sourceModuleId(source), targetModuleId(target), messageType(type), eventName(event), messageData(data),
          timestamp(std::chrono::steady_clock::now()), requiresResponse(false), priority(0) {}
};

/**
 * Message handler function type
 */
using MessageHandler = std::function<SCXML::Common::Result<void>(const InterModuleMessage &message)>;

/**
 * Message response handler function type
 */
using ResponseHandler = std::function<void(const InterModuleMessage &response)>;

/**
 * Communication channel between modules
 */
struct CommunicationChannel {
    std::string channelId;
    std::string sourceModuleId;
    std::string targetModuleId;
    std::string channelType;  // direct, broadcast, multicast
    bool isActive;
    std::chrono::steady_clock::time_point creationTime;
    size_t messageCount;

    CommunicationChannel() : isActive(false), creationTime(std::chrono::steady_clock::now()), messageCount(0) {}

    CommunicationChannel(const std::string &id, const std::string &source, const std::string &target,
                         const std::string &type = "direct")
        : channelId(id), sourceModuleId(source), targetModuleId(target), channelType(type), isActive(true),
          creationTime(std::chrono::steady_clock::now()), messageCount(0) {}
};

/**
 * Message routing information
 */
struct MessageRoute {
    std::string routeId;
    std::string pattern;                                     // Message pattern to match
    std::vector<std::string> targetModules;                  // Target modules for this route
    std::function<bool(const InterModuleMessage &)> filter;  // Message filter function
    bool isActive;

    MessageRoute() : isActive(true) {}

    MessageRoute(const std::string &id, const std::string &messagePattern)
        : routeId(id), pattern(messagePattern), isActive(true) {}
};

/**
 * Interface for inter-module communication
 */
class IInterModuleCommunicator {
public:
    virtual ~IInterModuleCommunicator() = default;

    /**
     * Register a module for communication
     */
    virtual SCXML::Common::Result<void> registerModule(const std::string &moduleId,
                                                       std::shared_ptr<RuntimeContext> context) = 0;

    /**
     * Unregister a module
     */
    virtual SCXML::Common::Result<void> unregisterModule(const std::string &moduleId) = 0;

    /**
     * Send message to specific module
     */
    virtual SCXML::Common::Result<void> sendMessage(const InterModuleMessage &message) = 0;

    /**
     * Send message with response handling
     */
    virtual SCXML::Common::Result<void>
    sendMessageWithResponse(const InterModuleMessage &message, ResponseHandler responseHandler,
                            std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) = 0;

    /**
     * Broadcast message to all registered modules
     */
    virtual SCXML::Common::Result<void> broadcastMessage(const InterModuleMessage &message) = 0;

    /**
     * Register message handler for a module
     */
    virtual SCXML::Common::Result<void>
    registerMessageHandler(const std::string &moduleId, const std::string &messageType, MessageHandler handler) = 0;

    /**
     * Create communication channel between modules
     */
    virtual SCXML::Common::Result<std::string> createChannel(const std::string &sourceModuleId,
                                                             const std::string &targetModuleId,
                                                             const std::string &channelType = "direct") = 0;

    /**
     * Close communication channel
     */
    virtual SCXML::Common::Result<void> closeChannel(const std::string &channelId) = 0;

    /**
     * Add message route for pattern-based routing
     */
    virtual SCXML::Common::Result<void> addRoute(const MessageRoute &route) = 0;

    /**
     * Remove message route
     */
    virtual SCXML::Common::Result<void> removeRoute(const std::string &routeId) = 0;

    /**
     * Get communication statistics
     */
    virtual std::map<std::string, size_t> getStatistics() const = 0;

    /**
     * Start communication system
     */
    virtual SCXML::Common::Result<void> start() = 0;

    /**
     * Stop communication system
     */
    virtual SCXML::Common::Result<void> stop() = 0;
};

/**
 * Priority-based message queue for inter-module communication
 */
class InterModuleMessageQueue {
public:
    InterModuleMessageQueue();
    ~InterModuleMessageQueue();

    /**
     * Enqueue message with priority
     */
    void enqueue(const InterModuleMessage &message);

    /**
     * Dequeue highest priority message
     */
    bool dequeue(InterModuleMessage &message, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    /**
     * Get queue size
     */
    size_t size() const;

    /**
     * Check if queue is empty
     */
    bool empty() const;

    /**
     * Clear all messages
     */
    void clear();

private:
    struct MessageComparator {
        bool operator()(const InterModuleMessage &a, const InterModuleMessage &b) const {
            // Higher priority first, then older timestamp
            if (a.priority != b.priority) {
                return a.priority < b.priority;
            }
            return a.timestamp > b.timestamp;
        }
    };

    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    std::priority_queue<InterModuleMessage, std::vector<InterModuleMessage>, MessageComparator> messageQueue_;
};

/**
 * Advanced inter-module communicator implementation
 */
class InterModuleCommunicator : public IInterModuleCommunicator {
public:
    InterModuleCommunicator();
    ~InterModuleCommunicator() override;

    SCXML::Common::Result<void> registerModule(const std::string &moduleId,
                                               std::shared_ptr<RuntimeContext> context) override;

    SCXML::Common::Result<void> unregisterModule(const std::string &moduleId) override;

    SCXML::Common::Result<void> sendMessage(const InterModuleMessage &message) override;

    SCXML::Common::Result<void> sendMessageWithResponse(const InterModuleMessage &message,
                                                        ResponseHandler responseHandler,
                                                        std::chrono::milliseconds timeout) override;

    SCXML::Common::Result<void> broadcastMessage(const InterModuleMessage &message) override;

    SCXML::Common::Result<void> registerMessageHandler(const std::string &moduleId, const std::string &messageType,
                                                       MessageHandler handler) override;

    SCXML::Common::Result<std::string> createChannel(const std::string &sourceModuleId,
                                                     const std::string &targetModuleId,
                                                     const std::string &channelType) override;

    SCXML::Common::Result<void> closeChannel(const std::string &channelId) override;

    SCXML::Common::Result<void> addRoute(const MessageRoute &route) override;

    SCXML::Common::Result<void> removeRoute(const std::string &routeId) override;

    std::map<std::string, size_t> getStatistics() const override;

    SCXML::Common::Result<void> start() override;

    SCXML::Common::Result<void> stop() override;

    /**
     * Set message processing thread count
     */
    void setThreadCount(size_t threadCount);

    /**
     * Get all active channels
     */
    std::vector<CommunicationChannel> getActiveChannels() const;

    /**
     * Get message history for debugging
     */
    std::vector<InterModuleMessage> getMessageHistory(size_t maxMessages = 100) const;

private:
    mutable std::mutex modulesMutex_;
    mutable std::mutex channelsMutex_;
    mutable std::mutex routesMutex_;
    mutable std::mutex handlersMutex_;
    mutable std::mutex responseHandlersMutex_;
    mutable std::mutex statisticsMutex_;

    std::map<std::string, std::shared_ptr<RuntimeContext>> registeredModules_;
    std::map<std::string, CommunicationChannel> channels_;
    std::map<std::string, MessageRoute> routes_;
    std::map<std::string, std::map<std::string, MessageHandler>>
        messageHandlers_;                                      // moduleId -> messageType -> handler
    std::map<std::string, ResponseHandler> responseHandlers_;  // correlationId -> handler

    std::unique_ptr<InterModuleMessageQueue> messageQueue_;
    std::vector<std::thread> workerThreads_;
    std::atomic<bool> isRunning_;
    size_t threadCount_;

    // Statistics
    mutable std::map<std::string, size_t> statistics_;
    std::vector<InterModuleMessage> messageHistory_;
    size_t maxHistorySize_;

    /**
     * Worker thread function for processing messages
     */
    void messageProcessingWorker();

    /**
     * Process a single message
     */
    SCXML::Common::Result<void> processMessage(const InterModuleMessage &message);

    /**
     * Route message based on routing rules
     */
    std::vector<std::string> routeMessage(const InterModuleMessage &message);

    /**
     * Deliver message to specific module
     */
    SCXML::Common::Result<void> deliverMessage(const std::string &targetModuleId, const InterModuleMessage &message);

    /**
     * Handle response message
     */
    void handleResponse(const InterModuleMessage &response);

    /**
     * Generate unique channel ID
     */
    std::string generateChannelId(const std::string &sourceModuleId, const std::string &targetModuleId);

    /**
     * Generate unique correlation ID
     */
    std::string generateCorrelationId();

    /**
     * Update statistics
     */
    void updateStatistics(const std::string &statName, size_t increment = 1);

    /**
     * Add message to history
     */
    void addToHistory(const InterModuleMessage &message);

    /**
     * Validate message format
     */
    SCXML::Common::Result<void> validateMessage(const InterModuleMessage &message);
};

/**
 * Utility functions for inter-module communication
 */
class InterModuleUtils {
public:
    /**
     * Create event message
     */
    static InterModuleMessage createEventMessage(const std::string &sourceModuleId, const std::string &targetModuleId,
                                                 const std::string &eventName,
                                                 const nlohmann::json &eventData = nlohmann::json{});

    /**
     * Create data message
     */
    static InterModuleMessage createDataMessage(const std::string &sourceModuleId, const std::string &targetModuleId,
                                                const nlohmann::json &data);

    /**
     * Create control message
     */
    static InterModuleMessage createControlMessage(const std::string &sourceModuleId, const std::string &targetModuleId,
                                                   const std::string &controlCommand,
                                                   const nlohmann::json &parameters = nlohmann::json{});

    /**
     * Create response message
     */
    static InterModuleMessage createResponseMessage(const InterModuleMessage &originalMessage,
                                                    const nlohmann::json &responseData);

    /**
     * Extract module ID from SCXML target attribute
     */
    static std::string extractModuleIdFromTarget(const std::string &target);

    /**
     * Format message for logging
     */
    static std::string formatMessageForLog(const InterModuleMessage &message);

    /**
     * Validate module ID format
     */
    static bool isValidModuleId(const std::string &moduleId);

    /**
     * Create message filter for specific event patterns
     */
    static std::function<bool(const InterModuleMessage &)> createEventPatternFilter(const std::string &eventPattern);

    /**
     * Create message filter for specific data keys
     */
    static std::function<bool(const InterModuleMessage &)>
    createDataKeyFilter(const std::vector<std::string> &requiredKeys);
};

}  // namespace Runtime
}  // namespace SCXML