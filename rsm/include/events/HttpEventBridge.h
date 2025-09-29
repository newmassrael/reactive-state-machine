#pragma once

#include "IEventBridge.h"
#include <atomic>
#include <regex>

namespace RSM {

/**
 * @brief Configuration for HTTP Event Bridge
 *
 * Implements W3C SCXML BasicHTTPEventProcessor specification.
 */
class HttpBridgeConfig : public IEventBridgeConfig {
public:
    struct Settings {
        // Event name extraction
        std::string defaultEventName = "http.request";  // Default event name if not specified
        bool extractEventFromUrl = true;                // Extract event name from URL path
        bool extractEventFromQuery = true;              // Extract event name from ?event= parameter
        bool extractEventFromBody = true;               // Extract event name from JSON body
        std::string eventQueryParam = "event";          // Query parameter name for event
        std::string eventBodyField = "event";           // JSON body field name for event

        // Data handling
        bool includeHttpMetadata = true;    // Include HTTP headers, method, etc. in event data
        bool preserveOriginalBody = true;   // Preserve original body content
        std::string dataEncoding = "json";  // Data encoding: "json", "form", "raw"

        // Response generation
        std::string successResponseTemplate = R"({"status": "success", "message": "Event received"})";
        std::string errorResponseTemplate = R"({"status": "error", "message": "Event processing failed"})";
        std::string defaultContentType = "application/json";

        // W3C BasicHTTP compliance
        bool enableW3CCompliance = true;  // Enable strict W3C BasicHTTPEventProcessor compliance
        std::string w3cEventProcessorType = "http://www.w3.org/TR/scxml/#BasicHTTPEventProcessor";

        // Error handling
        bool generateErrorEvents = true;                           // Generate error.* events for HTTP errors
        std::string httpErrorEventPrefix = "error.communication";  // Prefix for HTTP error events

        // Security
        std::vector<std::string> allowedContentTypes = {// Allowed content types for incoming requests
                                                        "application/json", "application/x-www-form-urlencoded",
                                                        "text/plain"};
        size_t maxBodySize = 1024 * 1024;  // Max request body size (1MB)
    };

    HttpBridgeConfig();
    explicit HttpBridgeConfig(const Settings &settings);

    // IEventBridgeConfig implementation
    std::string getConfigType() const override;
    std::vector<std::string> validate() const override;
    std::unique_ptr<IEventBridgeConfig> clone() const override;

    // Getter for settings
    const Settings &getSettings() const {
        return settings_;
    }

private:
    Settings settings_;
};

/**
 * @brief HTTP Event Bridge
 *
 * Implements W3C SCXML BasicHTTPEventProcessor event conversion.
 * Handles bidirectional conversion between HTTP and SCXML events.
 */
class HttpEventBridge : public IEventBridge {
public:
    /**
     * @brief Constructor with configuration
     * @param config Bridge configuration
     */
    explicit HttpEventBridge(const HttpBridgeConfig &config);

    // IEventBridge implementation
    EventDescriptor httpToScxmlEvent(const HttpRequest &request) override;
    HttpResponse scxmlToHttpResponse(const EventDescriptor &event) override;
    HttpRequest scxmlToHttpRequest(const EventDescriptor &event, const std::string &targetUrl) override;
    EventDescriptor httpToScxmlResponse(const HttpResponse &response, const std::string &originalSendId) override;
    std::string getBridgeType() const override;
    std::vector<std::string> validate() const override;
    std::string getDebugInfo() const override;

    /**
     * @brief Get processing statistics
     * @return Statistics as key-value pairs
     */
    std::unordered_map<std::string, std::string> getStatistics() const;

    /**
     * @brief Update configuration
     * @param config New configuration
     * @return true if configuration was updated
     */
    bool updateConfig(const HttpBridgeConfig &config);

private:
    /**
     * @brief Extract event name from HTTP request
     * @param request HTTP request
     * @return Event name, or empty string if not found
     */
    std::string extractEventName(const HttpRequest &request) const;

    /**
     * @brief Extract event data from HTTP request
     * @param request HTTP request
     * @return Event data as string
     */
    std::string extractEventData(const HttpRequest &request) const;

    /**
     * @brief Create HTTP metadata for SCXML event
     * @param request HTTP request
     * @return Metadata as JSON string
     */
    std::string createHttpMetadata(const HttpRequest &request) const;

    /**
     * @brief Parse JSON from string safely
     * @param jsonStr JSON string to parse
     * @return Parsed JSON value, or null value if parsing fails
     */
    std::string parseDataSafely(const std::string &dataStr) const;

    /**
     * @brief Validate HTTP request content type
     * @param contentType Content type to validate
     * @return true if content type is allowed
     */
    bool isContentTypeAllowed(const std::string &contentType) const;

    /**
     * @brief Create error event for HTTP processing failures
     * @param errorType Error type identifier
     * @param errorMessage Error description
     * @param originalRequest Optional original request (for context)
     * @return Error event descriptor
     */
    EventDescriptor createErrorEvent(const std::string &errorType, const std::string &errorMessage,
                                     const HttpRequest *originalRequest = nullptr) const;

    /**
     * @brief Convert form-encoded data to JSON
     * @param formData Form-encoded data string
     * @return JSON representation
     */
    std::string formDataToJson(const std::string &formData) const;

    /**
     * @brief Parse URL to extract components
     * @param url URL to parse
     * @param path Output path component
     * @param queryParams Output query parameters
     * @return true if parsing succeeded
     */
    bool parseUrl(const std::string &url, std::string &path,
                  std::unordered_map<std::string, std::string> &queryParams) const;

    /**
     * @brief URL decode a string (convert %XX to characters)
     * @param encoded URL-encoded string
     * @return Decoded string
     */
    std::string urlDecode(const std::string &encoded) const;

    HttpBridgeConfig config_;

    // Statistics
    mutable std::atomic<uint64_t> requestsProcessed_{0};
    mutable std::atomic<uint64_t> responsesGenerated_{0};
    mutable std::atomic<uint64_t> errorsEncountered_{0};
    mutable std::atomic<uint64_t> nextEventId_{1};
};

}  // namespace RSM