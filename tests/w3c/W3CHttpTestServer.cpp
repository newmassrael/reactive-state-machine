#include "W3CHttpTestServer.h"
#include "common/JsonUtils.h"
#include "common/Logger.h"
#include "events/HttpEventBridge.h"
#include <chrono>
#include <httplib.h>
#include <sstream>
#include <thread>

namespace RSM {
namespace W3C {

W3CHttpTestServer::W3CHttpTestServer(int port, const std::string &path)
    : port_(port), path_(path), server_(std::make_unique<httplib::Server>()) {
    // Set up POST handler for W3C HTTP event processing
    server_->Post(path_, [this](const httplib::Request &req, httplib::Response &res) { handlePost(req, res); });

    LOG_DEBUG("W3CHttpTestServer: Created server for {}:{}{}", "localhost", port_, path_);
}

W3CHttpTestServer::~W3CHttpTestServer() {
    stop();
}

bool W3CHttpTestServer::start() {
    if (running_.load()) {
        LOG_WARN("W3CHttpTestServer: Server already running");
        return false;
    }

    shutdownRequested_ = false;

    // Start server in background thread
    serverThread_ = std::thread([this]() {
        LOG_INFO("W3CHttpTestServer: Starting HTTP server on localhost:{}{}", port_, path_);

        running_ = true;
        bool success = server_->listen("localhost", port_);

        if (!success && !shutdownRequested_.load()) {
            LOG_ERROR("W3CHttpTestServer: Failed to start server on port {}", port_);
        }

        running_ = false;
        LOG_DEBUG("W3CHttpTestServer: Server thread ended");
    });

    // Wait a bit for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!running_.load()) {
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        return false;
    }

    LOG_INFO("W3CHttpTestServer: HTTP server started successfully on localhost:{}{}", port_, path_);
    return true;
}

void W3CHttpTestServer::stop() {
    if (!running_.load()) {
        return;
    }

    LOG_INFO("W3CHttpTestServer: Stopping HTTP server");

    shutdownRequested_ = true;

    if (server_) {
        server_->stop();
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }

    LOG_INFO("W3CHttpTestServer: HTTP server stopped");
}

void W3CHttpTestServer::handlePost(const httplib::Request &req, httplib::Response &res) {
    LOG_DEBUG("W3CHttpTestServer: Received POST request to {}", req.path);
    LOG_DEBUG("W3CHttpTestServer: Request body: {}", req.body);

    try {
        // Parse incoming HTTP request according to W3C SCXML BasicHTTPEventProcessor spec
        std::string eventName = "event1";  // W3C default event name
        std::string eventData = req.body;  // Use raw body as default

        // Simple string-based event extraction for W3C compliance
        if (!req.body.empty()) {
            // Look for "event" field in JSON using string parsing to avoid jsoncpp string_view issues
            size_t eventPos = req.body.find("\"event\"");
            if (eventPos != std::string::npos) {
                size_t colonPos = req.body.find(":", eventPos);
                if (colonPos != std::string::npos) {
                    size_t quoteStart = req.body.find("\"", colonPos);
                    if (quoteStart != std::string::npos) {
                        size_t quoteEnd = req.body.find("\"", quoteStart + 1);
                        if (quoteEnd != std::string::npos) {
                            eventName = req.body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                            LOG_DEBUG("W3CHttpTestServer: Extracted event name from JSON: {}", eventName);
                        }
                    }
                }
            }
        }

        // Default event name if not specified (W3C test compatibility)
        if (eventName.empty()) {
            eventName = "event1";  // Common W3C test default
        }

        // Generate unique sendId for W3C compliance
        std::string sendId = "w3c_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        LOG_INFO("W3CHttpTestServer: Processing event '{}' with sendId '{}'", eventName, sendId);

        // Forward event to SCXML system via callback
        if (eventCallback_) {
            eventCallback_(eventName, eventData);
        }

        // Send W3C compliant HTTP response using nlohmann/json
        json response = json::object();
        response["status"] = "success";
        response["event"] = eventName;
        response["sendId"] = sendId;
        response["timestamp"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();

        std::string responseBody = JsonUtils::toCompactString(response);

        res.set_content(responseBody, "application/json");
        res.status = 200;
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Access-Control-Allow-Origin", "*");  // W3C test compatibility
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        LOG_DEBUG("W3CHttpTestServer: Sent response: {}", responseBody);

    } catch (const std::exception &e) {
        LOG_ERROR("W3CHttpTestServer: Exception handling POST request: {}", e.what());
        res.status = 500;
        res.set_content(R"({"status": "error", "message": "Internal server error"})", "application/json");
    }
}

}  // namespace W3C
}  // namespace RSM