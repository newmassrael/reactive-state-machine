#pragma once

#include <string>

namespace RSM {

/**
 * @brief Observer interface for session lifecycle events
 *
 * SOLID Architecture: Observer pattern to decouple session management
 * from JavaScript context management while maintaining synchronization.
 */
class ISessionObserver {
public:
    virtual ~ISessionObserver() = default;

    /**
     * @brief Called when a new session is created
     * @param sessionId The identifier of the created session
     * @param parentSessionId Optional parent session identifier
     */
    virtual void onSessionCreated(const std::string &sessionId, const std::string &parentSessionId = "") = 0;

    /**
     * @brief Called when a session is being destroyed
     * @param sessionId The identifier of the session being destroyed
     */
    virtual void onSessionDestroyed(const std::string &sessionId) = 0;

    /**
     * @brief Called when session system variables are updated
     * @param sessionId The identifier of the session
     * @param sessionName Human-readable session name
     * @param ioProcessors List of available I/O processors
     */
    virtual void onSessionSystemVariablesUpdated(const std::string &sessionId, const std::string &sessionName,
                                                 const std::vector<std::string> &ioProcessors) = 0;
};

}  // namespace RSM