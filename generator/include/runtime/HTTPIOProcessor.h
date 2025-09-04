#pragma once

#include "runtime/IOProcessor.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <queue>
#include <thread>

// Forward declarations for HTTP client library
namespace httplib {
class Client;
class Server;
struct Request;
struct Response;
}  // namespace httplib

namespace SCXML {
namespace Runtime {

/**
 * @brief HTTP I/O Processor implementing W3C SCXML HTTP Event Processor
 *
 * Supports full HTTP communication including:
 * - Outgoing HTTP requests (GET, POST, PUT, DELETE, etc.)
 * - Incoming HTTP webhook server
 * - Async request/response handling
 * - Request/response transformation to SCXML events
 */
class HTTPIOProcessor : public IOProcessor {
public:
    /**
     * @brief Configuration for HTTP I/O Processor
     */
    struct Config {
        std::string serverHost = "localhost";                  // Server host for incoming webhooks
        int serverPort = 8080;                                 // Server port for incoming webhooks
        std::string serverPath = "/scxml/events";              // Base path for incoming events
        bool enableServer = true;                              // Enable webhook server
        uint32_t requestTimeoutMs = 30000;                     // HTTP request timeout (30 seconds)
        uint32_t maxConcurrentRequests = 100;                  // Max concurrent outgoing requests
        uint32_t maxRetries = 3;                               // Max retry attempts for failed requests
        bool followRedirects = true;                           // Follow HTTP redirects
        std::string userAgent = "SCXML-HTTP-IOProcessor/1.0";  // User agent string

        // Security settings
        bool validateCertificates = true;  // Validate SSL certificates
        std::string clientCertPath;        // Client certificate path
        std::string clientKeyPath;         // Client private key path
        std::string caCertPath;            // CA certificate path for validation

        // Headers to include in all outgoing requests
        std::unordered_map<std::string, std::string> defaultHeaders;
    };

    /**
     * @brief HTTP request methods
     */
    enum class HttpMethod { GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS };

private:
    /**
     * @brief Pending HTTP request information
     */
    struct PendingRequest {
        std::string sendId;
        std::string event;
        std::string target;
        std::chrono::steady_clock::time_point startTime;
        std::future<void> future;
        std::atomic<bool> cancelled{false};
        uint32_t retryCount = 0;
    };

    /**
     * @brief Delayed send operation
     */
    struct DelayedSend {
        SendParameters params;
        std::chrono::steady_clock::time_point executeTime;
        bool cancelled = false;

        // Make it movable
        DelayedSend() = default;
        DelayedSend(DelayedSend &&) = default;
        DelayedSend &operator=(DelayedSend &&) = default;
        DelayedSend(const DelayedSend &) = delete;
        DelayedSend &operator=(const DelayedSend &) = delete;
    };

public:
    /**
     * @brief Constructor with default configuration
     */
    HTTPIOProcessor();

    /**
     * @brief Constructor with custom configuration
     * @param config HTTP processor configuration
     */
    explicit HTTPIOProcessor(const Config &config);

    /**
     * @brief Destructor
     */
    ~HTTPIOProcessor() override;

    /**
     * @brief Start the HTTP I/O Processor
     * @return true if started successfully
     */
    bool start() override;

    /**
     * @brief Stop the HTTP I/O Processor
     * @return true if stopped successfully
     */
    bool stop() override;

    /**
     * @brief Send an HTTP request
     * @param params Send parameters from SCXML <send> element
     * @return true if send was initiated successfully
     */
    bool send(const SendParameters &params) override;

    /**
     * @brief Cancel a delayed or ongoing HTTP request
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
     * @brief Get the webhook server URL
     * @return Full URL where webhooks can be sent
     */
    std::string getWebhookURL() const;

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

private:
    /**
     * @brief Parse HTTP method from string
     * @param method Method string (case-insensitive)
     * @return HTTP method enum
     */
    HttpMethod parseHttpMethod(const std::string &method) const;

    /**
     * @brief Convert HTTP method enum to string
     * @param method HTTP method enum
     * @return Method string
     */
    std::string httpMethodToString(HttpMethod method) const;

    /**
     * @brief Execute HTTP request asynchronously
     * @param params Send parameters
     */
    void executeHttpRequest(const SendParameters &params);

    /**
     * @brief Execute HTTP request with retries
     * @param params Send parameters
     * @param method HTTP method
     * @param url Target URL
     * @param headers Request headers
     * @param body Request body
     */
    void executeHttpRequestWithRetries(const SendParameters &params, HttpMethod method, const std::string &url,
                                       const std::unordered_map<std::string, std::string> &headers,
                                       const std::string &body);

    /**
     * @brief Handle HTTP response and create SCXML event
     * @param params Original send parameters
     * @param statusCode HTTP status code
     * @param headers Response headers
     * @param body Response body
     */
    void handleHttpResponse(const SendParameters &params, int statusCode,
                            const std::unordered_map<std::string, std::string> &headers, const std::string &body);

    /**
     * @brief Handle HTTP request failure
     * @param params Original send parameters
     * @param errorMessage Error description
     */
    void handleHttpError(const SendParameters &params, const std::string &errorMessage);

    /**
     * @brief Start HTTP webhook server
     * @return true if server started successfully
     */
    bool startWebhookServer();

    /**
     * @brief Stop HTTP webhook server
     * @return true if server stopped successfully
     */
    bool stopWebhookServer();

    /**
     * @brief Handle incoming webhook request
     * @param request HTTP request
     * @param response HTTP response to populate
     */
    void handleWebhookRequest(const httplib::Request &request, httplib::Response &response);

    /**
     * @brief Process delayed send operations
     */
    void processDelayedSends();

    /**
     * @brief Clean up completed requests
     */
    void cleanupRequests();

    /**
     * @brief Extract parameters from SCXML send params
     * @param params SCXML send parameters
     * @param method HTTP method (output)
     * @param headers HTTP headers (output)
     * @param body HTTP body (output)
     */
    void extractHttpParameters(const SendParameters &params, HttpMethod &method,
                               std::unordered_map<std::string, std::string> &headers, std::string &body) const;

    /**
     * @brief Generate unique request ID
     * @return Unique request identifier
     */
    std::string generateRequestId() const;

    Config config_;

    // HTTP client and server
    std::unique_ptr<httplib::Client> httpClient_;
    std::unique_ptr<httplib::Server> webhookServer_;
    std::thread serverThread_;

    // Request management
    mutable std::mutex pendingRequestsMutex_;
    std::unordered_map<std::string, std::unique_ptr<PendingRequest>> pendingRequests_;

    // Delayed sends
    mutable std::mutex delayedSendsMutex_;
    std::condition_variable delayedSendCondition_;
    std::queue<DelayedSend> delayedSends_;
    std::thread delayedSendThread_;

    // Runtime state
    std::atomic<bool> running_{false};

    // Request ID generation
    mutable std::mutex requestIdMutex_;
    mutable std::atomic<uint64_t> nextRequestId_{1};

    // Shutdown management
    std::atomic<bool> shutdownRequested_{false};
};

}  // namespace Runtime
}  // namespace SCXML