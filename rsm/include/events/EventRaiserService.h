#pragma once

#include "events/IEventRaiserRegistry.h"
#include "scripting/ISessionManager.h"
#include <memory>
#include <string>

namespace RSM {

class IEventRaiser;

/**
 * @brief Centralized EventRaiser management service with dependency injection support
 *
 * This service replaces the static EventRaiserRegistry creation in JSEngine
 * and provides proper dependency injection for better testability and flexibility.
 *
 * Key improvements:
 * - Dependency injection support for testing
 * - Mock registry injection capability
 * - Thread-safe singleton with initialization control
 * - Eliminates dangerous dynamic_pointer_cast operations
 */
class EventRaiserService {
public:
    /**
     * @brief Initialize the service with injected dependencies
     *
     * Must be called before getInstance(). Allows dependency injection
     * for testing with mock implementations.
     *
     * @param registry EventRaiser registry implementation
     * @param sessionManager Session manager implementation
     */
    static void initialize(std::shared_ptr<IEventRaiserRegistry> registry,
                           std::shared_ptr<ISessionManager> sessionManager);

    /**
     * @brief Get the singleton service instance
     *
     * @return Reference to the initialized service
     * @throws std::runtime_error if not initialized
     */
    static EventRaiserService &getInstance();

    /**
     * @brief Reset the service (for testing)
     *
     * Clears the singleton instance to allow re-initialization
     * with different dependencies.
     */
    static void reset();

    // Non-copyable, non-movable
    EventRaiserService(const EventRaiserService &) = delete;
    EventRaiserService &operator=(const EventRaiserService &) = delete;
    EventRaiserService(EventRaiserService &&) = delete;
    EventRaiserService &operator=(EventRaiserService &&) = delete;

    /**
     * @brief Register EventRaiser for a session with validation
     *
     * @param sessionId Target session identifier
     * @param eventRaiser EventRaiser instance to register
     * @return true if registration successful or already registered
     */
    bool registerEventRaiser(const std::string &sessionId, std::shared_ptr<IEventRaiser> eventRaiser);

    /**
     * @brief Get EventRaiser for a session
     *
     * @param sessionId Target session identifier
     * @return EventRaiser instance or nullptr if not found
     */
    std::shared_ptr<IEventRaiser> getEventRaiser(const std::string &sessionId) const;

    /**
     * @brief Unregister EventRaiser for a session
     *
     * @param sessionId Target session identifier
     * @return true if unregistration successful
     */
    bool unregisterEventRaiser(const std::string &sessionId);

    /**
     * @brief Get the underlying registry (for advanced use cases)
     *
     * @return Shared pointer to the registry
     */
    std::shared_ptr<IEventRaiserRegistry> getRegistry() const;

    /**
     * @brief Clear all registrations (for testing)
     *
     * Safe method that works with any registry implementation
     */
    void clearAll();

private:
    explicit EventRaiserService(std::shared_ptr<IEventRaiserRegistry> registry,
                                std::shared_ptr<ISessionManager> sessionManager);

    std::shared_ptr<IEventRaiserRegistry> registry_;
    std::shared_ptr<ISessionManager> sessionManager_;

    static std::unique_ptr<EventRaiserService> instance_;
    static std::mutex initMutex_;
};

}  // namespace RSM