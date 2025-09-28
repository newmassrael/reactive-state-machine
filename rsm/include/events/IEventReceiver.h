#pragma once

#include "EventDescriptor.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

/**
 * @brief Interface for receiving events from external sources
 *
 * Abstracts different event receiving mechanisms (HTTP webhooks, WebSocket, MQTT, etc.)
 * following SOLID principles for extensible inbound event support.
 *
 * This complements IEventTarget (outbound) to provide bidirectional communication.
 */
class IEventReceiver {
public:
    /**
     * @brief Callback function type for handling received events
     * @param event The received event
     * @return true if event was processed successfully
     */
    using EventCallback = std::function<bool(const EventDescriptor &event)>;

    virtual ~IEventReceiver() = default;

    /**
     * @brief Start receiving events
     * @return true if receiver started successfully
     */
    virtual bool startReceiving() = 0;

    /**
     * @brief Stop receiving events
     * @return true if receiver stopped successfully
     */
    virtual bool stopReceiving() = 0;

    /**
     * @brief Check if receiver is currently active
     * @return true if actively receiving events
     */
    virtual bool isReceiving() const = 0;

    /**
     * @brief Get the endpoint where events can be sent to this receiver
     * @return Endpoint URI (e.g., "http://localhost:8080/scxml/events")
     */
    virtual std::string getReceiveEndpoint() const = 0;

    /**
     * @brief Get the receiver type identifier
     * @return Receiver type string (e.g., "http-webhook", "websocket", "mqtt")
     */
    virtual std::string getReceiverType() const = 0;

    /**
     * @brief Set the callback function for handling received events
     * @param callback Event handling callback
     */
    virtual void setEventCallback(EventCallback callback) = 0;

    /**
     * @brief Validate receiver configuration
     * @return Vector of validation errors (empty if valid)
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Get receiver-specific information for debugging
     * @return Debug information string
     */
    virtual std::string getDebugInfo() const = 0;
};

/**
 * @brief Configuration interface for event receivers
 *
 * Provides type-safe configuration management for different receiver types.
 */
class IEventReceiverConfig {
public:
    virtual ~IEventReceiverConfig() = default;

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
    virtual std::unique_ptr<IEventReceiverConfig> clone() const = 0;
};

/**
 * @brief Factory interface for creating event receivers
 *
 * Enables registration of different receiver implementations
 * and automatic receiver selection based on configuration.
 */
class IEventReceiverFactory {
public:
    virtual ~IEventReceiverFactory() = default;

    /**
     * @brief Create an event receiver with the given configuration
     * @param config Receiver configuration
     * @return Event receiver instance, or nullptr if config not supported
     */
    virtual std::unique_ptr<IEventReceiver> createReceiver(const IEventReceiverConfig &config) = 0;

    /**
     * @brief Register a receiver type with the factory
     * @param configType Configuration type identifier
     * @param creator Function to create receiver instances
     */
    virtual void
    registerReceiverType(const std::string &configType,
                         std::function<std::unique_ptr<IEventReceiver>(const IEventReceiverConfig &)> creator) = 0;

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