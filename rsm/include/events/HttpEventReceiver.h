#pragma once

#include "IEventReceiver.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

// Forward declarations to avoid dependency on httplib in header
namespace httplib {
class Server;
struct Request;
struct Response;
}  // namespace httplib

namespace RSM {

/**
 * @brief Configuration for HTTP webhook receiver
 *
 * Implements IEventReceiverConfig for HTTP-specific settings.
 */
class HttpReceiverConfig : public IEventReceiverConfig {
public:
    struct Settings {
        std::string host = "localhost";                                              // Server bind address
        int port = 8080;                                                             // Server port
        std::string basePath = "/scxml/events";                                      // Base path for webhook endpoints
        std::chrono::milliseconds serverTimeout = std::chrono::milliseconds(30000);  // Request timeout
        int maxConcurrentConnections = 100;                                          // Max simultaneous connections
        bool enableCors = true;                                                      // Enable CORS headers
        bool enableHttps = false;  // Enable HTTPS (requires certificates)
        std::string certPath;      // SSL certificate path (if HTTPS enabled)
        std::string keyPath;       // SSL private key path (if HTTPS enabled)

        // Security settings
        bool requireAuth = false;                                     // Require authentication
        std::string authToken;                                        // Bearer token for authentication
        std::unordered_map<std::string, std::string> allowedOrigins;  // CORS allowed origins

        // Response settings
        std::string defaultResponseContentType = "application/json";
        std::string successResponse = R"({"status": "success", "message": "Event received"})";
        std::string errorResponse = R"({"status": "error", "message": "Event processing failed"})";
    };

    HttpReceiverConfig();
    explicit HttpReceiverConfig(const Settings &settings);

    // IEventReceiverConfig implementation
    std::string getConfigType() const override;
    std::vector<std::string> validate() const override;
    std::unique_ptr<IEventReceiverConfig> clone() const override;

    // Getter for settings
    const Settings &getSettings() const {
        return settings_;
    }

private:
    Settings settings_;
};

/**
 * @brief HTTP webhook event receiver
 *
 * Receives HTTP POST requests and converts them to SCXML events.
 * Supports the W3C SCXML BasicHTTPEventProcessor specification.
 */
class HttpEventReceiver : public IEventReceiver {
public:
    /**
     * @brief Constructor with configuration
     * @param config HTTP receiver configuration
     */
    explicit HttpEventReceiver(const HttpReceiverConfig &config);

    /**
     * @brief Destructor - ensures server is stopped
     */
    ~HttpEventReceiver() override;

    // IEventReceiver implementation
    bool startReceiving() override;
    bool stopReceiving() override;
    bool isReceiving() const override;
    std::string getReceiveEndpoint() const override;
    std::string getReceiverType() const override;
    void setEventCallback(EventCallback callback) override;
    std::vector<std::string> validate() const override;
    std::string getDebugInfo() const override;

    /**
     * @brief Get current server statistics
     * @return Statistics as key-value pairs
     */
    std::unordered_map<std::string, std::string> getStatistics() const;

    /**
     * @brief Update configuration (server must be stopped)
     * @param config New configuration
     * @return true if configuration was updated
     */
    bool updateConfig(const HttpReceiverConfig &config);

private:
    /**
     * @brief Handle incoming HTTP request
     * @param request HTTP request
     * @param response HTTP response to populate
     */
    void handleRequest(const httplib::Request &request, httplib::Response &response);

    /**
     * @brief Convert HTTP request to SCXML event
     * @param request HTTP request
     * @return Event descriptor, or empty event if conversion fails
     */
    EventDescriptor convertRequestToEvent(const httplib::Request &request) const;

    /**
     * @brief Validate authentication (if enabled)
     * @param request HTTP request
     * @return true if authentication is valid or not required
     */
    bool validateAuthentication(const httplib::Request &request) const;

    /**
     * @brief Set CORS headers on response
     * @param response HTTP response
     * @param request HTTP request (for origin checking)
     */
    void setCorsHeaders(httplib::Response &response, const httplib::Request &request) const;

    /**
     * @brief Generate unique event ID
     * @return Unique event identifier
     */
    std::string generateEventId() const;

    /**
     * @brief Start HTTP server on background thread
     */
    void startServerThread();

    /**
     * @brief Stop HTTP server and join thread
     */
    void stopServerThread();

    HttpReceiverConfig config_;
    EventCallback eventCallback_;

    // HTTP server infrastructure
    std::unique_ptr<httplib::Server> server_;
    std::thread serverThread_;

    // Runtime state
    std::atomic<bool> receiving_{false};
    std::atomic<bool> shutdownRequested_{false};

    // Statistics
    mutable std::atomic<uint64_t> requestCount_{0};
    mutable std::atomic<uint64_t> successCount_{0};
    mutable std::atomic<uint64_t> errorCount_{0};
    mutable std::atomic<uint64_t> nextEventId_{1};

    // Server status
    std::atomic<bool> serverStarted_{false};
    std::atomic<int> actualPort_{0};  // Actual port if 0 was specified
};

}  // namespace RSM