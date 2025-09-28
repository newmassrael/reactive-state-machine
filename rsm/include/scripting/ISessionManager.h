#pragma once

#include <string>
#include <vector>

namespace RSM {

// Forward declaration
class ISessionObserver;

/**
 * @brief Interface for session management operations (SOLID: Interface Segregation + Observer Pattern)
 *
 * Extended interface that provides session management with observer support
 * for decoupled notification of session lifecycle events.
 */
class ISessionManager {
public:
    virtual ~ISessionManager() = default;

    /**
     * @brief Check if a session exists
     *
     * @param sessionId Session identifier to check
     * @return true if session exists, false otherwise
     */
    virtual bool hasSession(const std::string &sessionId) const = 0;

    /**
     * @brief Create a new session
     *
     * @param sessionId New session identifier
     * @param parentSessionId Parent session identifier (optional)
     * @return true if session created successfully
     */
    virtual bool createSession(const std::string &sessionId, const std::string &parentSessionId = "") = 0;

    /**
     * @brief Destroy an existing session
     *
     * @param sessionId Session identifier to destroy
     * @return true if session destroyed successfully
     */
    virtual bool destroySession(const std::string &sessionId) = 0;

    // === Observer Pattern Support ===

    /**
     * @brief Add observer for session lifecycle events
     * @param observer Observer to be notified of session events
     */
    virtual void addObserver(ISessionObserver *observer) = 0;

    /**
     * @brief Remove observer from session lifecycle events
     * @param observer Observer to be removed
     */
    virtual void removeObserver(ISessionObserver *observer) = 0;

    // === Extended Session Management ===

    /**
     * @brief Get list of all active sessions
     * @return Vector of session identifiers
     */
    virtual std::vector<std::string> getActiveSessions() const = 0;

    /**
     * @brief Get parent session ID for a given session
     * @param sessionId Session to get parent for
     * @return Parent session ID or empty string if no parent
     */
    virtual std::string getParentSessionId(const std::string &sessionId) const = 0;
};

}  // namespace RSM