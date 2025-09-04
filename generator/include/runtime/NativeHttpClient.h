#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace SCXML {
namespace Runtime {

/**
 * @brief Real HTTP client implementation using system libraries
 *
 * This implementation provides actual HTTP communication without external dependencies,
 * using system-level socket programming and HTTP protocol implementation.
 */
class NativeHttpClient {
public:
    struct Response {
        int statusCode = 0;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        std::string error;
        bool success = false;

        Response() = default;

        Response(int code, const std::string &responseBody)
            : statusCode(code), body(responseBody), success(code >= 200 && code < 300) {}
    };

    struct Request {
        std::string method = "GET";
        std::string url;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        std::chrono::milliseconds timeout{30000};  // 30 seconds default
        bool followRedirects = true;
        bool verifySSL = true;

        Request(const std::string &requestUrl) : url(requestUrl) {}
    };

    using ResponseCallback = std::function<void(const Response &)>;

private:
    struct ParsedURL {
        std::string protocol;
        std::string host;
        int port = 80;
        std::string path;
        bool isSSL = false;

        ParsedURL() = default;
    };

    struct PendingRequest {
        std::string id;
        Request request;
        ResponseCallback callback;
        std::promise<Response> promise;
        std::atomic<bool> cancelled{false};
        std::chrono::steady_clock::time_point startTime;

        PendingRequest() : request("") {}  // Default constructor with empty URL
    };

public:
    NativeHttpClient();
    ~NativeHttpClient();

    /**
     * @brief Make synchronous HTTP request
     * @param request Request configuration
     * @return HTTP response
     */
    Response execute(const Request &request);

    /**
     * @brief Make asynchronous HTTP request
     * @param request Request configuration
     * @param callback Response callback function
     * @return Request ID for cancellation
     */
    std::string executeAsync(const Request &request, ResponseCallback callback = nullptr);

    /**
     * @brief Cancel ongoing request
     * @param requestId Request identifier to cancel
     * @return true if cancellation was successful
     */
    bool cancelRequest(const std::string &requestId);

    /**
     * @brief Get pending requests count
     * @return Number of active requests
     */
    size_t getPendingRequestsCount() const;

    /**
     * @brief Set global timeout for all requests
     * @param timeoutMs Timeout in milliseconds
     */
    void setGlobalTimeout(std::chrono::milliseconds timeoutMs);

    /**
     * @brief Set maximum concurrent requests
     * @param maxRequests Maximum number of concurrent requests
     */
    void setMaxConcurrentRequests(size_t maxRequests);

private:
    /**
     * @brief Parse URL into components
     * @param url URL to parse
     * @return Parsed URL components
     */
    ParsedURL parseURL(const std::string &url);

    /**
     * @brief Execute actual HTTP request
     * @param request Request to execute
     * @return HTTP response
     */
    Response executeHTTPRequest(const Request &request);

    /**
     * @brief Create TCP connection to host
     * @param host Target host
     * @param port Target port
     * @return Socket file descriptor or -1 on failure
     */
    int createConnection(const std::string &host, int port);

    /**
     * @brief Send HTTP request data
     * @param socket Socket file descriptor
     * @param request Request to send
     * @param parsedUrl Parsed URL components
     * @return true if sent successfully
     */
    bool sendRequest(int socket, const Request &request, const ParsedURL &parsedUrl);

    /**
     * @brief Receive HTTP response
     * @param socket Socket file descriptor
     * @param request Original request (for timeout)
     * @return HTTP response
     */
    Response receiveResponse(int socket, const Request &request);

    /**
     * @brief Parse HTTP response headers
     * @param headerLines Raw header lines
     * @return Status code and headers map
     */
    std::pair<int, std::unordered_map<std::string, std::string>>
    parseHeaders(const std::vector<std::string> &headerLines);

    /**
     * @brief Build HTTP request string
     * @param request Request configuration
     * @param parsedUrl Parsed URL components
     * @return HTTP request string
     */
    std::string buildRequestString(const Request &request, const ParsedURL &parsedUrl);

    /**
     * @brief Handle redirects if enabled
     * @param response Current response
     * @param request Original request
     * @param redirectCount Current redirect count
     * @return Final response after following redirects
     */
    Response handleRedirect(const Response &response, Request request, int redirectCount = 0);

    /**
     * @brief URL encode string
     * @param value String to encode
     * @return URL encoded string
     */
    std::string urlEncode(const std::string &value);

    /**
     * @brief Generate unique request ID
     * @return Unique identifier string
     */
    std::string generateRequestId();

    /**
     * @brief Worker thread for async requests
     */
    void workerThread();

    /**
     * @brief Process pending async requests
     */
    void processPendingRequests();

    // Configuration
    std::chrono::milliseconds globalTimeout_{30000};
    size_t maxConcurrentRequests_{50};
    static constexpr int MAX_REDIRECTS = 5;

    // Async processing
    std::atomic<bool> shutdown_{false};
    std::thread workerThread_;
    mutable std::mutex pendingRequestsMutex_;
    std::queue<std::unique_ptr<PendingRequest>> pendingRequests_;
    std::unordered_map<std::string, std::unique_ptr<PendingRequest>> activeRequests_;
    std::condition_variable requestCondition_;

    // Request ID generation
    std::atomic<uint64_t> nextRequestId_{1};
    mutable std::mutex requestIdMutex_;
};

}  // namespace Runtime
}  // namespace SCXML