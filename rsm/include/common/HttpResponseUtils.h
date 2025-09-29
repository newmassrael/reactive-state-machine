#pragma once

#include "events/IEventBridge.h"  // For HttpResponse struct
#include <httplib.h>
#include <string>
#include <unordered_map>

namespace RSM {

/**
 * @brief Centralized HTTP response and header utilities
 *
 * Eliminates duplicate HTTP response/header processing logic across components.
 * Provides consistent header management and CORS handling.
 */
class HttpResponseUtils {
public:
    /**
     * @brief Set standard JSON response headers
     * @param response HTTP response to modify
     */
    static void setJsonHeaders(HttpResponse &response);

    /**
     * @brief Set standard JSON response headers for httplib::Response
     * @param response httplib::Response to modify
     */
    static void setJsonHeaders(httplib::Response &response);

    /**
     * @brief Set CORS headers for cross-origin requests
     * @param response httplib::Response to modify
     * @param origin Origin header value (empty for wildcard)
     * @param allowedMethods Allowed HTTP methods
     * @param allowedHeaders Allowed request headers
     */
    static void setCorsHeaders(httplib::Response &response, const std::string &origin = "",
                               const std::string &allowedMethods = "POST, GET, OPTIONS",
                               const std::string &allowedHeaders = "Content-Type, Authorization");

    /**
     * @brief Create success JSON response
     * @param data Response data as JSON string
     * @return HttpResponse with success status and JSON headers
     */
    static HttpResponse createSuccessResponse(const std::string &data);

    /**
     * @brief Create error JSON response
     * @param errorMessage Error message
     * @param statusCode HTTP status code (default: 400)
     * @return HttpResponse with error status and JSON headers
     */
    static HttpResponse createErrorResponse(const std::string &errorMessage, int statusCode = 400);

    /**
     * @brief Set httplib::Response for success with JSON data
     * @param response httplib::Response to configure
     * @param data JSON data string
     */
    static void setSuccessResponse(httplib::Response &response, const std::string &data);

    /**
     * @brief Set httplib::Response for error with message
     * @param response httplib::Response to configure
     * @param errorMessage Error message
     * @param statusCode HTTP status code (default: 400)
     */
    static void setErrorResponse(httplib::Response &response, const std::string &errorMessage, int statusCode = 400);

    /**
     * @brief Set cache control headers for no-cache
     * @param response HTTP response to modify
     */
    static void setNoCacheHeaders(HttpResponse &response);

    /**
     * @brief Set cache control headers for no-cache (httplib version)
     * @param response httplib::Response to modify
     */
    static void setNoCacheHeaders(httplib::Response &response);

private:
    static const std::string JSON_CONTENT_TYPE;
    static const std::string NO_CACHE_CONTROL;
};

}  // namespace RSM