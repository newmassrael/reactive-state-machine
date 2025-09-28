#pragma once

#include "EventDescriptor.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RSM {

/**
 * @brief HTTP request representation for event bridge
 */
struct HttpRequest {
    std::string method = "POST";                               // HTTP method
    std::string url;                                           // Request URL
    std::string path;                                          // URL path
    std::unordered_map<std::string, std::string> headers;      // HTTP headers
    std::unordered_map<std::string, std::string> queryParams;  // Query parameters
    std::string body;                                          // Request body
    std::string remoteAddress;                                 // Client IP address
    std::string userAgent;                                     // User agent string
};

/**
 * @brief HTTP response representation for event bridge
 */
struct HttpResponse {
    int statusCode = 200;                                  // HTTP status code
    std::unordered_map<std::string, std::string> headers;  // Response headers
    std::string body;                                      // Response body
    std::string contentType = "application/json";          // Content type
};

/**
 * @brief Interface for converting between HTTP and SCXML events
 *
 * Abstracts the conversion logic between different protocols and SCXML events,
 * following SOLID principles for extensible protocol support.
 */
class IEventBridge {
public:
    virtual ~IEventBridge() = default;

    /**
     * @brief Convert HTTP request to SCXML event
     * @param request HTTP request to convert
     * @return SCXML event descriptor
     */
    virtual EventDescriptor httpToScxmlEvent(const HttpRequest &request) = 0;

    /**
     * @brief Convert SCXML event to HTTP response
     * @param event SCXML event to convert
     * @return HTTP response
     */
    virtual HttpResponse scxmlToHttpResponse(const EventDescriptor &event) = 0;

    /**
     * @brief Convert SCXML event to HTTP request (for outbound)
     * @param event SCXML event to convert
     * @param targetUrl Target URL for the request
     * @return HTTP request
     */
    virtual HttpRequest scxmlToHttpRequest(const EventDescriptor &event, const std::string &targetUrl) = 0;

    /**
     * @brief Convert HTTP response to SCXML event (for inbound responses)
     * @param response HTTP response to convert
     * @param originalSendId Original send ID that triggered the request
     * @return SCXML event descriptor
     */
    virtual EventDescriptor httpToScxmlResponse(const HttpResponse &response, const std::string &originalSendId) = 0;

    /**
     * @brief Get bridge type identifier
     * @return Bridge type string (e.g., "basic-http", "rest-api", "soap")
     */
    virtual std::string getBridgeType() const = 0;

    /**
     * @brief Validate bridge configuration
     * @return Vector of validation errors (empty if valid)
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Get bridge-specific information for debugging
     * @return Debug information string
     */
    virtual std::string getDebugInfo() const = 0;
};

/**
 * @brief Configuration interface for event bridges
 */
class IEventBridgeConfig {
public:
    virtual ~IEventBridgeConfig() = default;

    /**
     * @brief Get configuration type identifier
     * @return Configuration type string
     */
    virtual std::string getConfigType() const = 0;

    /**
     * @brief Validate configuration
     * @return Vector of validation errors (empty if valid)
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Clone this configuration
     * @return Copy of this configuration
     */
    virtual std::unique_ptr<IEventBridgeConfig> clone() const = 0;
};

/**
 * @brief Factory interface for creating event bridges
 */
class IEventBridgeFactory {
public:
    virtual ~IEventBridgeFactory() = default;

    /**
     * @brief Create an event bridge with the given configuration
     * @param config Bridge configuration
     * @return Event bridge instance, or nullptr if config not supported
     */
    virtual std::unique_ptr<IEventBridge> createBridge(const IEventBridgeConfig &config) = 0;

    /**
     * @brief Register a bridge type with the factory
     * @param configType Configuration type identifier
     * @param creator Function to create bridge instances
     */
    virtual void
    registerBridgeType(const std::string &configType,
                       std::function<std::unique_ptr<IEventBridge>(const IEventBridgeConfig &)> creator) = 0;

    /**
     * @brief Check if a configuration type is supported
     * @param configType Configuration type to check
     * @return true if supported
     */
    virtual bool isConfigTypeSupported(const std::string &configType) const = 0;

    /**
     * @brief Get all supported configuration types
     * @return Vector of supported configuration types
     */
    virtual std::vector<std::string> getSupportedConfigTypes() const = 0;
};

}  // namespace RSM