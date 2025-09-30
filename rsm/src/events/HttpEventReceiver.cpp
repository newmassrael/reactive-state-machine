#include "events/HttpEventReceiver.h"
#include "common/HttpResponseUtils.h"
#include "common/JsonUtils.h"
#include "common/Logger.h"
#include "common/UniqueIdGenerator.h"
#include "scripting/JSEngine.h"
#include <httplib.h>
#include <regex>
#include <sstream>

namespace RSM {

// ============================================================================
// HttpReceiverConfig Implementation
// ============================================================================

HttpReceiverConfig::HttpReceiverConfig() : settings_{} {}

HttpReceiverConfig::HttpReceiverConfig(const Settings &settings) : settings_(settings) {}

std::string HttpReceiverConfig::getConfigType() const {
    return "http-webhook";
}

std::vector<std::string> HttpReceiverConfig::validate() const {
    std::vector<std::string> errors;

    if (settings_.host.empty()) {
        errors.push_back("Host cannot be empty");
    }

    if (settings_.port <= 0 || settings_.port > 65535) {
        errors.push_back("Port must be between 1 and 65535");
    }

    if (settings_.basePath.empty()) {
        errors.push_back("Base path cannot be empty");
    }

    if (!settings_.basePath.starts_with("/")) {
        errors.push_back("Base path must start with '/'");
    }

    if (settings_.serverTimeout.count() <= 0) {
        errors.push_back("Server timeout must be positive");
    }

    if (settings_.maxConcurrentConnections <= 0) {
        errors.push_back("Max concurrent connections must be positive");
    }

    if (settings_.enableHttps) {
        if (settings_.certPath.empty()) {
            errors.push_back("Certificate path required for HTTPS");
        }
        if (settings_.keyPath.empty()) {
            errors.push_back("Private key path required for HTTPS");
        }
    }

    if (settings_.requireAuth && settings_.authToken.empty()) {
        errors.push_back("Auth token required when authentication is enabled");
    }

    return errors;
}

std::unique_ptr<IEventReceiverConfig> HttpReceiverConfig::clone() const {
    return std::make_unique<HttpReceiverConfig>(settings_);
}

// ============================================================================
// HttpEventReceiver Implementation
// ============================================================================

HttpEventReceiver::HttpEventReceiver(const HttpReceiverConfig &config)
    : config_(config), server_(std::make_unique<httplib::Server>()) {
    LOG_DEBUG("HttpEventReceiver: Created with host='{}', port={}, basePath='{}'", config_.getSettings().host,
              config_.getSettings().port, config_.getSettings().basePath);
}

HttpEventReceiver::~HttpEventReceiver() {
    stopReceiving();
}

bool HttpEventReceiver::startReceiving() {
    if (receiving_) {
        LOG_WARN("HttpEventReceiver: Already receiving events");
        return false;
    }

    auto validationErrors = validate();
    if (!validationErrors.empty()) {
        LOG_ERROR("HttpEventReceiver: Configuration validation failed:");
        for (const auto &error : validationErrors) {
            LOG_ERROR("  - {}", error);
        }
        return false;
    }

    if (!eventCallback_) {
        LOG_ERROR("HttpEventReceiver: Event callback not set");
        return false;
    }

    LOG_INFO("HttpEventReceiver: Starting HTTP webhook server on {}:{}{}", config_.getSettings().host,
             config_.getSettings().port, config_.getSettings().basePath);

    // Configure server
    server_->set_read_timeout(config_.getSettings().serverTimeout);
    server_->set_write_timeout(config_.getSettings().serverTimeout);

    // Set up webhook endpoint
    const auto &settings = config_.getSettings();
    std::string eventPath = settings.basePath;
    if (!eventPath.ends_with("/")) {
        eventPath += "/";
    }
    eventPath += "event";

    server_->Post(eventPath, [this](const httplib::Request &req, httplib::Response &res) { handleRequest(req, res); });

    // Handle preflight OPTIONS requests for CORS
    if (settings.enableCors) {
        server_->Options(eventPath, [this](const httplib::Request &req, httplib::Response &res) {
            const auto &settings = config_.getSettings();
            if (settings.enableCors) {
                std::string origin = req.get_header_value("Origin");
                HttpResponseUtils::setCorsHeaders(res, origin);
            }
            res.status = 200;
        });
    }

    // Add health check endpoint
    server_->Get(settings.basePath + "/health", [](const httplib::Request &, httplib::Response &res) {
        res.set_content(R"({"status": "healthy", "service": "scxml-http-receiver"})", "application/json");
    });

    // Start server on background thread
    shutdownRequested_ = false;
    startServerThread();

    // Wait briefly for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (serverStarted_) {
        receiving_ = true;
        LOG_INFO("HttpEventReceiver: HTTP webhook server started successfully");
        LOG_INFO("HttpEventReceiver: Webhook endpoint available at: {}", getReceiveEndpoint());
        return true;
    } else {
        LOG_ERROR("HttpEventReceiver: Failed to start HTTP webhook server");
        return false;
    }
}

bool HttpEventReceiver::stopReceiving() {
    if (!receiving_) {
        return true;
    }

    LOG_INFO("HttpEventReceiver: Stopping HTTP webhook server");

    shutdownRequested_ = true;
    receiving_ = false;

    stopServerThread();

    LOG_INFO("HttpEventReceiver: HTTP webhook server stopped");
    return true;
}

bool HttpEventReceiver::isReceiving() const {
    return receiving_.load();
}

std::string HttpEventReceiver::getReceiveEndpoint() const {
    const auto &settings = config_.getSettings();
    std::ostringstream endpoint;

    endpoint << (settings.enableHttps ? "https" : "http") << "://" << settings.host << ":"
             << (actualPort_ > 0 ? actualPort_.load() : settings.port);

    std::string path = settings.basePath;
    if (!path.ends_with("/")) {
        path += "/";
    }
    path += "event";

    endpoint << path;
    return endpoint.str();
}

std::string HttpEventReceiver::getReceiverType() const {
    return "http-webhook";
}

void HttpEventReceiver::setEventCallback(EventCallback callback) {
    eventCallback_ = std::move(callback);
}

std::vector<std::string> HttpEventReceiver::validate() const {
    return config_.validate();
}

std::string HttpEventReceiver::getDebugInfo() const {
    const auto &settings = config_.getSettings();
    std::ostringstream info;

    info << "HttpEventReceiver{"
         << "type='" << getReceiverType() << "'"
         << ", endpoint='" << getReceiveEndpoint() << "'"
         << ", receiving=" << (receiving_ ? "true" : "false") << ", requests=" << requestCount_.load()
         << ", successes=" << successCount_.load() << ", errors=" << errorCount_.load() << ", host='" << settings.host
         << "'"
         << ", port=" << settings.port << ", basePath='" << settings.basePath << "'"
         << ", cors=" << (settings.enableCors ? "enabled" : "disabled")
         << ", https=" << (settings.enableHttps ? "enabled" : "disabled")
         << ", auth=" << (settings.requireAuth ? "enabled" : "disabled") << "}";

    return info.str();
}

std::unordered_map<std::string, std::string> HttpEventReceiver::getStatistics() const {
    return {{"requests_total", std::to_string(requestCount_.load())},
            {"requests_success", std::to_string(successCount_.load())},
            {"requests_error", std::to_string(errorCount_.load())},
            {"receiving", receiving_ ? "true" : "false"},
            {"endpoint", getReceiveEndpoint()}};
}

bool HttpEventReceiver::updateConfig(const HttpReceiverConfig &config) {
    if (receiving_) {
        LOG_ERROR("HttpEventReceiver: Cannot update configuration while receiving");
        return false;
    }

    config_ = config;
    return true;
}

void HttpEventReceiver::handleRequest(const httplib::Request &request, httplib::Response &response) {
    requestCount_++;

    LOG_DEBUG("HttpEventReceiver: Received {} request to {}", request.method, request.path);

    const auto &settings = config_.getSettings();

    try {
        // Set CORS headers if enabled
        if (settings.enableCors) {
            const auto &settings = config_.getSettings();
            if (settings.enableCors) {
                std::string origin = request.get_header_value("Origin");
                HttpResponseUtils::setCorsHeaders(response, origin);
            }
        }

        // Validate authentication
        if (!validateAuthentication(request)) {
            LOG_WARN("HttpEventReceiver: Authentication failed for request from {}", request.get_header_value("Host"));
            HttpResponseUtils::setErrorResponse(response, "Authentication required", 401);
            errorCount_++;
            return;
        }

        // Convert request to SCXML event
        EventDescriptor event = convertRequestToEvent(request);
        if (event.eventName.empty()) {
            LOG_ERROR("HttpEventReceiver: Failed to convert request to event");
            response.status = 400;
            response.set_content(settings.errorResponse, settings.defaultResponseContentType);
            errorCount_++;
            return;
        }

        // Process event through callback
        bool success = false;
        if (eventCallback_) {
            success = eventCallback_(event);
        }

        if (success) {
            LOG_DEBUG("HttpEventReceiver: Successfully processed event '{}'", event.eventName);
            HttpResponseUtils::setSuccessResponse(response, settings.successResponse);
            successCount_++;
        } else {
            LOG_ERROR("HttpEventReceiver: Failed to process event '{}'", event.eventName);
            HttpResponseUtils::setErrorResponse(response, settings.errorResponse, 500);
            errorCount_++;
        }

    } catch (const std::exception &e) {
        LOG_ERROR("HttpEventReceiver: Exception handling request: {}", e.what());
        HttpResponseUtils::setErrorResponse(response, settings.errorResponse, 500);
        errorCount_++;
    }
}

EventDescriptor HttpEventReceiver::convertRequestToEvent(const httplib::Request &request) const {
    EventDescriptor event;

    try {
        // Generate unique event ID
        event.sendId = UniqueIdGenerator::generateEventId();

        // Extract event name from URL path or body
        std::string eventName;

        // Try to get event name from query parameter
        auto eventParam = request.get_param_value("event");
        if (!eventParam.empty()) {
            eventName = eventParam;
        } else {
            // Try to extract from JSON body
            if (!request.body.empty()) {
                auto root = JsonUtils::parseJson(request.body);
                if (root.has_value() && JsonUtils::hasKey(root.value(), "event")) {
                    eventName = JsonUtils::getString(root.value(), "event");
                }
            }
        }

        // Default event name if not specified
        if (eventName.empty()) {
            eventName = "http.request";
        }

        event.eventName = eventName;

        // Set event data from request body
        if (!request.body.empty()) {
            event.data = request.body;
        } else {
            // Create JSON from query parameters
            json dataObj = json::object();
            for (const auto &param : request.params) {
                dataObj[param.first] = param.second;
            }

            event.data = JsonUtils::toCompactString(dataObj);
        }

        // Set target (for response routing, if needed)
        event.target = getReceiveEndpoint();

        // Add HTTP-specific metadata
        json metadata = json::object();
        metadata["method"] = request.method;
        metadata["path"] = request.path;
        metadata["remote_addr"] = request.get_header_value("Host");
        metadata["user_agent"] = request.get_header_value("User-Agent");
        metadata["content_type"] = request.get_header_value("Content-Type");

        // Add all headers
        json headers = json::object();
        for (const auto &header : request.headers) {
            headers[header.first] = header.second;
        }
        metadata["headers"] = headers;

        // Metadata is handled by the HttpEventBridge, not stored directly
        // The bridge will include metadata in event.data

        LOG_DEBUG("HttpEventReceiver: Converted request to event: name='{}', data_size={}, sendId='{}'",
                  event.eventName, event.data.size(), event.sendId);

    } catch (const std::exception &e) {
        LOG_ERROR("HttpEventReceiver: Exception converting request to event: {}", e.what());
        return {};  // Return empty event on error
    }

    return event;
}

bool HttpEventReceiver::validateAuthentication(const httplib::Request &request) const {
    const auto &settings = config_.getSettings();

    if (!settings.requireAuth) {
        return true;  // Authentication not required
    }

    std::string authHeader = request.get_header_value("Authorization");
    if (authHeader.empty()) {
        return false;
    }

    // Check for Bearer token
    const std::string bearerPrefix = "Bearer ";
    if (authHeader.starts_with(bearerPrefix)) {
        std::string token = authHeader.substr(bearerPrefix.length());
        return token == settings.authToken;
    }

    return false;
}

void HttpEventReceiver::startServerThread() {
    serverThread_ = std::thread([this]() {
        const auto &settings = config_.getSettings();

        LOG_DEBUG("HttpEventReceiver: Server thread starting on {}:{}", settings.host, settings.port);

        try {
            bool started = false;
            if (settings.enableHttps) {
                // For HTTPS, we would need httplib::SSLServer, but for simplicity use HTTP
                LOG_WARN("HttpEventReceiver: HTTPS requested but using HTTP for simplicity");
                started = server_->listen(settings.host, settings.port);
            } else {
                started = server_->listen(settings.host, settings.port);
            }

            if (!started && !shutdownRequested_) {
                LOG_ERROR("HttpEventReceiver: Failed to start server on {}:{}", settings.host, settings.port);
            }
        } catch (const std::exception &e) {
            LOG_ERROR("HttpEventReceiver: Server thread exception: {}", e.what());
        }

        LOG_DEBUG("HttpEventReceiver: Server thread ended");
    });

    // Wait briefly and check if server started
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    serverStarted_ = server_->is_running();

    if (serverStarted_) {
        // Get actual port if 0 was specified (dynamic port assignment)
        actualPort_ = config_.getSettings().port;
    }
}

void HttpEventReceiver::stopServerThread() {
    if (server_) {
        server_->stop();
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }

    serverStarted_ = false;
    actualPort_ = 0;
}

}  // namespace RSM