#pragma once

#include "EventDescriptor.h"
#include <future>
#include <memory>
#include <string>

namespace RSM {

/**
 * @brief Interface for event delivery targets
 *
 * Abstracts different event delivery mechanisms (internal, HTTP, WebSocket, etc.)
 * following the Strategy pattern for extensible target support.
 */
class IEventTarget {
public:
    virtual ~IEventTarget() = default;

    /**
     * @brief Send an event to this target
     * @param event Event descriptor containing all event information
     * @return Future with send result
     */
    virtual std::future<SendResult> send(const EventDescriptor &event) = 0;

    /**
     * @brief Get the target type identifier
     * @return Target type string (e.g., "internal", "http", "websocket")
     */
    virtual std::string getTargetType() const = 0;

    /**
     * @brief Check if this target can handle the given URI
     * @param targetUri Target URI to check
     * @return true if this target can handle the URI
     */
    virtual bool canHandle(const std::string &targetUri) const = 0;

    /**
     * @brief Validate target configuration
     * @return Vector of validation errors (empty if valid)
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Get target-specific information for debugging
     * @return Debug information string
     */
    virtual std::string getDebugInfo() const = 0;
};

/**
 * @brief Factory interface for creating event targets
 *
 * Enables registration of different target implementations
 * and automatic target selection based on URI schemes.
 */
class IEventTargetFactory {
public:
    virtual ~IEventTargetFactory() = default;

    /**
     * @brief Create an event target for the given URI
     * @param targetUri Target URI (e.g., "#_internal", "http://example.com")
     * @return Event target instance, or nullptr if URI not supported
     */
    virtual std::shared_ptr<IEventTarget> createTarget(const std::string &targetUri) = 0;

    /**
     * @brief Register a target type with the factory
     * @param scheme URI scheme (e.g., "http", "ws")
     * @param creator Function to create target instances
     */
    virtual void registerTargetType(const std::string &scheme,
                                    std::function<std::shared_ptr<IEventTarget>(const std::string &)> creator) = 0;

    /**
     * @brief Check if a URI scheme is supported
     * @param scheme URI scheme to check
     * @return true if supported
     */
    virtual bool isSchemeSupported(const std::string &scheme) const = 0;

    /**
     * @brief Get all supported URI schemes
     * @return Vector of supported schemes
     */
    virtual std::vector<std::string> getSupportedSchemes() const = 0;
};

}  // namespace RSM