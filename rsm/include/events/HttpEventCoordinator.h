#pragma once

#include "HttpEventBridge.h"
#include "HttpEventReceiver.h"
#include "HttpEventTarget.h"
#include "common/TypeRegistry.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace RSM {

/**
 * @brief Configuration for HTTP Event Coordinator
 */
struct HttpCoordinatorConfig {
    // Server configuration
    HttpReceiverConfig receiverConfig;

    // Bridge configuration
    HttpBridgeConfig bridgeConfig;

    // Coordinator settings
    bool autoStartReceiver = true;              // Auto-start receiver when coordinator starts
    bool enableEventLoopback = true;            // Enable event loopback for testing
    std::string loopbackEventPrefix = "test.";  // Prefix for loopback events

    // Event routing
    std::function<bool(const EventDescriptor &)> eventFilter;  // Filter incoming events
    std::function<void(const EventDescriptor &)> eventLogger;  // Log processed events

    // Performance settings
    size_t maxConcurrentEvents = 100;                                           // Max concurrent event processing
    std::chrono::milliseconds eventTimeout = std::chrono::milliseconds(30000);  // Event processing timeout

    // W3C compliance
    bool enableW3CCompliance = true;         // Enable strict W3C BasicHTTPEventProcessor compliance
    bool validateEventProcessorType = true;  // Validate type attribute against TypeRegistry
};

/**
 * @brief HTTP Event Coordinator
 *
 * Coordinates HTTP event processing between clients and servers,
 * implementing W3C SCXML BasicHTTPEventProcessor specification.
 *
 * Responsibilities:
 * - Manage HTTP event receiver lifecycle
 * - Coordinate event bridging between HTTP and SCXML
 * - Route events between internal and external systems
 * - Provide unified interface for HTTP event processing
 */
class HttpEventCoordinator {
public:
    /**
     * @brief Event processing callback type
     * @param event Processed SCXML event
     * @return true if event was handled successfully
     */
    using EventCallback = std::function<bool(const EventDescriptor &event)>;

    /**
     * @brief Constructor with configuration
     * @param config Coordinator configuration
     */
    explicit HttpEventCoordinator(const HttpCoordinatorConfig &config);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~HttpEventCoordinator();

    /**
     * @brief Start the coordinator
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the coordinator
     * @return true if stopped successfully
     */
    bool stop();

    /**
     * @brief Check if coordinator is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Set callback for handling processed SCXML events
     * @param callback Event handling callback
     */
    void setEventCallback(EventCallback callback);

    /**
     * @brief Send SCXML event via HTTP (outbound)
     * @param event Event to send
     * @param targetUrl Target HTTP URL
     * @return Future with send result
     */
    std::future<SendResult> sendEvent(const EventDescriptor &event, const std::string &targetUrl);

    /**
     * @brief Check if type URI is supported for HTTP event processing
     * @param typeUri Type URI to check (e.g., "http://www.w3.org/TR/scxml/#BasicHTTPEventProcessor")
     * @return true if supported
     */
    bool canHandleType(const std::string &typeUri) const;

    /**
     * @brief Get the webhook endpoint URL for receiving events
     * @return Webhook URL where HTTP events can be sent
     */
    std::string getWebhookUrl() const;

    /**
     * @brief Get coordinator statistics
     * @return Statistics as key-value pairs
     */
    std::unordered_map<std::string, std::string> getStatistics() const;

    /**
     * @brief Get debug information
     * @return Debug information string
     */
    std::string getDebugInfo() const;

    /**
     * @brief Update configuration (coordinator must be stopped)
     * @param config New configuration
     * @return true if configuration was updated
     */
    bool updateConfig(const HttpCoordinatorConfig &config);

    /**
     * @brief Validate configuration
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<std::string> validate() const;

    /**
     * @brief Enable/disable event loopback for testing
     * @param enabled Enable loopback
     * @param eventPrefix Prefix for loopback events
     */
    void setEventLoopback(bool enabled, const std::string &eventPrefix = "test.");

private:
    /**
     * @brief Handle incoming HTTP event from receiver
     * @param event HTTP-derived SCXML event
     * @return true if event was processed successfully
     */
    bool handleIncomingEvent(const EventDescriptor &event);

    /**
     * @brief Process event through bridge and routing
     * @param event Event to process
     * @return true if processing succeeded
     */
    bool processEvent(const EventDescriptor &event);

    /**
     * @brief Check if event should be processed based on filters
     * @param event Event to check
     * @return true if event should be processed
     */
    bool shouldProcessEvent(const EventDescriptor &event) const;

    /**
     * @brief Log event processing
     * @param event Event being processed
     * @param success Whether processing succeeded
     */
    void logEventProcessing(const EventDescriptor &event, bool success) const;

    /**
     * @brief Validate type URI against W3C compliance rules
     * @param typeUri Type URI to validate
     * @return true if valid for HTTP processing
     */
    bool validateTypeUri(const std::string &typeUri) const;

    /**
     * @brief Create HTTP target for outbound requests
     * @param targetUrl Target URL
     * @return HTTP target instance
     */
    std::shared_ptr<HttpEventTarget> createHttpTarget(const std::string &targetUrl) const;

    HttpCoordinatorConfig config_;
    EventCallback eventCallback_;

    // Core components
    std::unique_ptr<HttpEventReceiver> receiver_;
    std::unique_ptr<HttpEventBridge> bridge_;

    // Runtime state
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdownRequested_{false};

    // Statistics
    mutable std::atomic<uint64_t> eventsReceived_{0};
    mutable std::atomic<uint64_t> eventsSent_{0};
    mutable std::atomic<uint64_t> eventsProcessed_{0};
    mutable std::atomic<uint64_t> eventsFiltered_{0};
    mutable std::atomic<uint64_t> processingErrors_{0};

    // Type registry reference
    TypeRegistry &typeRegistry_;
};

}  // namespace RSM