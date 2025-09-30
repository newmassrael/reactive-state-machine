#include "events/HttpEventCoordinator.h"
#include "common/Logger.h"
#include <algorithm>

namespace RSM {

HttpEventCoordinator::HttpEventCoordinator(const HttpCoordinatorConfig &config)
    : config_(config), typeRegistry_(TypeRegistry::getInstance()) {
    // Create bridge
    bridge_ = std::make_unique<HttpEventBridge>(config_.bridgeConfig);

    // Create receiver
    receiver_ = std::make_unique<HttpEventReceiver>(config_.receiverConfig);

    // Set up receiver callback to handle incoming events
    receiver_->setEventCallback([this](const EventDescriptor &event) -> bool { return handleIncomingEvent(event); });

    LOG_DEBUG("HttpEventCoordinator: Created with webhook endpoint: {}", receiver_->getReceiveEndpoint());
}

HttpEventCoordinator::~HttpEventCoordinator() {
    stop();
}

bool HttpEventCoordinator::start() {
    if (running_) {
        LOG_WARN("HttpEventCoordinator: Already running");
        return false;
    }

    // Validate configuration
    auto validationErrors = validate();
    if (!validationErrors.empty()) {
        LOG_ERROR("HttpEventCoordinator: Configuration validation failed:");
        for (const auto &error : validationErrors) {
            LOG_ERROR("  - {}", error);
        }
        return false;
    }

    LOG_INFO("HttpEventCoordinator: Starting HTTP event coordination");

    // Start receiver if auto-start is enabled
    if (config_.autoStartReceiver) {
        if (!receiver_->startReceiving()) {
            LOG_ERROR("HttpEventCoordinator: Failed to start HTTP event receiver");
            return false;
        }
        LOG_INFO("HttpEventCoordinator: HTTP event receiver started at: {}", receiver_->getReceiveEndpoint());
    }

    shutdownRequested_ = false;
    running_ = true;

    LOG_INFO("HttpEventCoordinator: HTTP event coordination started successfully");
    LOG_INFO("HttpEventCoordinator: W3C compliance: {}", config_.enableW3CCompliance ? "enabled" : "disabled");
    LOG_INFO("HttpEventCoordinator: Event loopback: {}", config_.enableEventLoopback ? "enabled" : "disabled");

    return true;
}

bool HttpEventCoordinator::stop() {
    if (!running_) {
        return true;
    }

    LOG_INFO("HttpEventCoordinator: Stopping HTTP event coordination");

    shutdownRequested_ = true;
    running_ = false;

    // Stop receiver
    if (receiver_) {
        receiver_->stopReceiving();
    }

    LOG_INFO("HttpEventCoordinator: HTTP event coordination stopped");
    return true;
}

bool HttpEventCoordinator::isRunning() const {
    return running_.load();
}

void HttpEventCoordinator::setEventCallback(EventCallback callback) {
    eventCallback_ = std::move(callback);
}

std::future<SendResult> HttpEventCoordinator::sendEvent(const EventDescriptor &event, const std::string &targetUrl) {
    eventsSent_++;

    return std::async(std::launch::async, [this, event, targetUrl]() -> SendResult {
        try {
            if (!running_) {
                return SendResult::error("Coordinator not running", SendResult::ErrorType::INTERNAL_ERROR);
            }

            LOG_DEBUG("HttpEventCoordinator: Sending event '{}' to '{}'", event.eventName, targetUrl);

            // Convert SCXML event to HTTP request using bridge
            HttpRequest httpRequest = bridge_->scxmlToHttpRequest(event, targetUrl);
            if (httpRequest.url.empty()) {
                return SendResult::error("Failed to convert SCXML event to HTTP request",
                                         SendResult::ErrorType::INTERNAL_ERROR);
            }

            // Create HTTP target and send
            auto httpTarget = createHttpTarget(targetUrl);
            if (!httpTarget) {
                return SendResult::error("Failed to create HTTP target for: " + targetUrl,
                                         SendResult::ErrorType::INTERNAL_ERROR);
            }

            // Convert HttpRequest to EventDescriptor for HttpEventTarget
            EventDescriptor httpEvent;
            httpEvent.eventName = event.eventName;
            httpEvent.data = httpRequest.body;
            httpEvent.sendId = event.sendId;
            httpEvent.target = targetUrl;

            // HTTP metadata is handled by the HttpEventBridge

            // Send via HTTP target
            auto sendFuture = httpTarget->send(httpEvent);
            auto result = sendFuture.get();

            LOG_DEBUG("HttpEventCoordinator: HTTP send result: success={}, sendId='{}'", result.isSuccess,
                      result.sendId);

            return result;

        } catch (const std::exception &e) {
            processingErrors_++;
            LOG_ERROR("HttpEventCoordinator: Exception sending event: {}", e.what());
            return SendResult::error("Exception during send: " + std::string(e.what()),
                                     SendResult::ErrorType::INTERNAL_ERROR);
        }
    });
}

bool HttpEventCoordinator::canHandleType(const std::string &typeUri) const {
    if (typeUri.empty()) {
        return false;
    }

    // Check against TypeRegistry
    if (typeRegistry_.isBasicHttpEventProcessor(typeUri)) {
        return true;
    }

    // Check for HTTP-based types
    if (typeRegistry_.isHttpType(typeUri)) {
        return true;
    }

    // W3C compliance validation
    if (config_.enableW3CCompliance && config_.validateEventProcessorType) {
        return validateTypeUri(typeUri);
    }

    return false;
}

std::string HttpEventCoordinator::getWebhookUrl() const {
    if (receiver_) {
        return receiver_->getReceiveEndpoint();
    }
    return "";
}

std::unordered_map<std::string, std::string> HttpEventCoordinator::getStatistics() const {
    std::unordered_map<std::string, std::string> stats;

    stats["running"] = running_ ? "true" : "false";
    stats["events_received"] = std::to_string(eventsReceived_.load());
    stats["events_sent"] = std::to_string(eventsSent_.load());
    stats["events_processed"] = std::to_string(eventsProcessed_.load());
    stats["events_filtered"] = std::to_string(eventsFiltered_.load());
    stats["processing_errors"] = std::to_string(processingErrors_.load());
    stats["webhook_url"] = getWebhookUrl();
    stats["w3c_compliance"] = config_.enableW3CCompliance ? "enabled" : "disabled";
    stats["event_loopback"] = config_.enableEventLoopback ? "enabled" : "disabled";

    // Add receiver statistics
    if (receiver_) {
        auto receiverStats = receiver_->getStatistics();
        for (const auto &stat : receiverStats) {
            stats["receiver_" + stat.first] = stat.second;
        }
    }

    // Add bridge statistics
    if (bridge_) {
        auto bridgeStats = bridge_->getStatistics();
        for (const auto &stat : bridgeStats) {
            stats["bridge_" + stat.first] = stat.second;
        }
    }

    return stats;
}

std::string HttpEventCoordinator::getDebugInfo() const {
    std::ostringstream info;

    info << "HttpEventCoordinator{"
         << "running=" << (running_ ? "true" : "false") << ", webhook='" << getWebhookUrl() << "'"
         << ", events_received=" << eventsReceived_.load() << ", events_sent=" << eventsSent_.load()
         << ", events_processed=" << eventsProcessed_.load() << ", processing_errors=" << processingErrors_.load()
         << ", w3c_compliance=" << (config_.enableW3CCompliance ? "enabled" : "disabled")
         << ", receiver=" << (receiver_ ? receiver_->getDebugInfo() : "null")
         << ", bridge=" << (bridge_ ? bridge_->getDebugInfo() : "null") << "}";

    return info.str();
}

bool HttpEventCoordinator::updateConfig(const HttpCoordinatorConfig &config) {
    if (running_) {
        LOG_ERROR("HttpEventCoordinator: Cannot update configuration while running");
        return false;
    }

    config_ = config;

    // Update receiver configuration
    if (receiver_) {
        receiver_->updateConfig(config_.receiverConfig);
    }

    // Update bridge configuration
    if (bridge_) {
        bridge_->updateConfig(config_.bridgeConfig);
    }

    LOG_DEBUG("HttpEventCoordinator: Configuration updated");
    return true;
}

std::vector<std::string> HttpEventCoordinator::validate() const {
    std::vector<std::string> errors;

    // Validate receiver configuration
    if (receiver_) {
        auto receiverErrors = receiver_->validate();
        for (const auto &error : receiverErrors) {
            errors.push_back("Receiver: " + error);
        }
    }

    // Validate bridge configuration
    if (bridge_) {
        auto bridgeErrors = bridge_->validate();
        for (const auto &error : bridgeErrors) {
            errors.push_back("Bridge: " + error);
        }
    }

    // Validate coordinator-specific settings
    if (config_.maxConcurrentEvents == 0) {
        errors.push_back("Max concurrent events must be greater than 0");
    }

    if (config_.eventTimeout.count() <= 0) {
        errors.push_back("Event timeout must be positive");
    }

    return errors;
}

void HttpEventCoordinator::setEventLoopback(bool enabled, const std::string &eventPrefix) {
    config_.enableEventLoopback = enabled;
    config_.loopbackEventPrefix = eventPrefix;

    LOG_DEBUG("HttpEventCoordinator: Event loopback {} with prefix '{}'", enabled ? "enabled" : "disabled",
              eventPrefix);
}

bool HttpEventCoordinator::handleIncomingEvent(const EventDescriptor &event) {
    eventsReceived_++;

    try {
        LOG_DEBUG("HttpEventCoordinator: Handling incoming event: '{}'", event.eventName);

        // Check if we should process this event
        if (!shouldProcessEvent(event)) {
            eventsFiltered_++;
            LOG_DEBUG("HttpEventCoordinator: Event '{}' filtered out", event.eventName);
            return true;  // Filtered events are considered "handled"
        }

        // Process the event
        bool success = processEvent(event);

        // Log processing result
        logEventProcessing(event, success);

        if (success) {
            eventsProcessed_++;
        } else {
            processingErrors_++;
        }

        return success;

    } catch (const std::exception &e) {
        processingErrors_++;
        LOG_ERROR("HttpEventCoordinator: Exception handling incoming event '{}': {}", event.eventName, e.what());
        return false;
    }
}

bool HttpEventCoordinator::processEvent(const EventDescriptor &event) {
    // Handle event loopback for testing
    if (config_.enableEventLoopback && event.eventName.starts_with(config_.loopbackEventPrefix)) {
        LOG_DEBUG("HttpEventCoordinator: Processing loopback event '{}'", event.eventName);

        // For loopback events, we can simulate responses or just log them
        if (eventCallback_) {
            return eventCallback_(event);
        }
        return true;
    }

    // Process normal events through callback
    if (eventCallback_) {
        return eventCallback_(event);
    }

    // No callback configured - this might be an error in production
    LOG_WARN("HttpEventCoordinator: No event callback configured, cannot process event '{}'", event.eventName);
    return false;
}

bool HttpEventCoordinator::shouldProcessEvent(const EventDescriptor &event) const {
    // Apply event filter if configured
    if (config_.eventFilter) {
        return config_.eventFilter(event);
    }

    // Default: process all events
    return true;
}

void HttpEventCoordinator::logEventProcessing(const EventDescriptor &event, bool success) const {
    // Apply event logger if configured
    if (config_.eventLogger) {
        config_.eventLogger(event);
    }

    // Default logging
    LOG_DEBUG("HttpEventCoordinator: Event '{}' processed: {}", event.eventName, success ? "success" : "failed");
}

bool HttpEventCoordinator::validateTypeUri(const std::string &typeUri) const {
    if (!config_.enableW3CCompliance) {
        return true;  // Skip validation if compliance not required
    }

    // Must be registered in TypeRegistry as event processor
    return typeRegistry_.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, typeUri);
}

std::shared_ptr<HttpEventTarget> HttpEventCoordinator::createHttpTarget(const std::string &targetUrl) const {
    try {
        // Create HTTP target with default configuration
        // In production, this might use a factory or pool
        return std::make_shared<HttpEventTarget>(targetUrl);

    } catch (const std::exception &e) {
        LOG_ERROR("HttpEventCoordinator: Failed to create HTTP target for '{}': {}", targetUrl, e.what());
        return nullptr;
    }
}

}  // namespace RSM