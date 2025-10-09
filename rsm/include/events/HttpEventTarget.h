#pragma once

#include "EventDescriptor.h"
#include "IEventTarget.h"
#include <chrono>
#include <future>
#include <httplib.h>
#include <string>

namespace RSM {

/**
 * @brief HTTP/HTTPS event target implementation
 *
 * This class implements the IEventTarget interface for sending events to
 * HTTP and HTTPS endpoints. It supports W3C SCXML specification requirements
 * for external event delivery via HTTP POST requests.
 *
 * Key features:
 * - HTTP and HTTPS support with automatic protocol detection
 * - JSON payload serialization for event data
 * - Configurable timeouts and retry logic
 * - Proper error handling and status code interpretation
 * - W3C SCXML compliant event formatting
 *
 * Example usage:
 * <send target="https://api.example.com/webhook" event="user.action" data="'payload'"/>
 *
 * HTTP Request format:
 * POST /webhook HTTP/1.1
 * Host: api.example.com
 * Content-Type: application/json
 *
 * {
 *   "event": "user.action",
 *   "data": "payload",
 *   "sendid": "auto_12345",
 *   "source": "scxml"
 * }
 */
class HttpEventTarget : public IEventTarget, public std::enable_shared_from_this<HttpEventTarget> {
public:
    /**
     * @brief Construct HTTP event target
     *
     * @param targetUri Target URL (http:// or https://)
     * @param timeoutMs Request timeout in milliseconds (default: 5000)
     * @param maxRetries Maximum retry attempts on failure (default: 1)
     */
    explicit HttpEventTarget(const std::string &targetUri,
                             std::chrono::milliseconds timeoutMs = std::chrono::milliseconds(5000), int maxRetries = 1);

    /**
     * @brief Destructor
     */
    virtual ~HttpEventTarget() = default;

    // IEventTarget implementation
    std::future<SendResult> send(const EventDescriptor &event) override;
    std::string getTargetType() const override;
    bool canHandle(const std::string &targetUri) const override;
    std::vector<std::string> validate() const override;
    std::string getDebugInfo() const override;

    /**
     * @brief Set custom HTTP headers for requests
     *
     * @param headers Map of header name to header value
     */
    void setCustomHeaders(const std::map<std::string, std::string> &headers);

    /**
     * @brief Set request timeout
     *
     * @param timeoutMs Timeout in milliseconds
     */
    void setTimeout(std::chrono::milliseconds timeoutMs);

    /**
     * @brief Set maximum retry attempts
     *
     * @param maxRetries Maximum number of retry attempts
     */
    void setMaxRetries(int maxRetries);

    /**
     * @brief Enable or disable SSL certificate verification (HTTPS only)
     *
     * @param verify true to verify certificates (default), false to skip
     */
    void setSSLVerification(bool verify);

private:
    /**
     * @brief Parse target URI into components
     *
     * @return true if URI is valid and supported
     */
    bool parseTargetUri();

    /**
     * @brief Create HTTP client for the target
     *
     * @return HTTP client instance
     */
    std::unique_ptr<httplib::Client> createHttpClient() const;

    /**
     * @brief Convert event to JSON payload
     *
     * @param event Event to serialize
     * @return JSON string representation
     */
    std::string createJsonPayload(const EventDescriptor &event) const;

    /**
     * @brief Perform HTTP POST request with retry logic
     *
     * @param client HTTP client
     * @param path Request path
     * @param payload JSON payload
     * @param contentType Content-Type header (e.g., "application/json" or "text/plain")
     * @return HTTP response result
     */
    httplib::Result performRequestWithRetry(httplib::Client &client, const std::string &path,
                                            const std::string &payload, const std::string &contentType) const;

    /**
     * @brief Convert HTTP response to SendResult
     *
     * @param result HTTP response
     * @param event Original event for context
     * @return SendResult with success/failure information
     */
    SendResult convertHttpResponse(const httplib::Result &result, const EventDescriptor &event) const;

    /**
     * @brief Escape JSON string values
     *
     * @param input Raw string
     * @return JSON-escaped string
     */
    std::string escapeJsonString(const std::string &input) const;

private:
    std::string targetUri_;
    std::string scheme_;  // "http" or "https"
    std::string host_;
    int port_;
    std::string path_;

    std::chrono::milliseconds timeoutMs_;
    int maxRetries_;
    bool sslVerification_;

    std::map<std::string, std::string> customHeaders_;

    mutable std::mutex clientMutex_;  // For thread-safe HTTP client access
};

}  // namespace RSM