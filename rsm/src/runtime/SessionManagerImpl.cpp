#include "runtime/SessionManagerImpl.h"
#include "common/Logger.h"
#include "runtime/ISessionObserver.h"
#include <algorithm>

namespace RSM {

bool SessionManagerImpl::hasSession(const std::string &sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.find(sessionId) != sessions_.end();
}

bool SessionManagerImpl::createSession(const std::string &sessionId, const std::string &parentSessionId) {
    // Validate input parameters
    if (!isValidSessionId(sessionId)) {
        LOG_ERROR("SessionManagerImpl: Invalid session ID: '{}'", sessionId);
        return false;
    }

    if (!isValidParentSession(parentSessionId)) {
        LOG_ERROR("SessionManagerImpl: Invalid parent session ID: '{}'", parentSessionId);
        return false;
    }

    std::lock_guard<std::mutex> lock(sessionsMutex_);

    // Check if session already exists
    if (sessions_.find(sessionId) != sessions_.end()) {
        LOG_DEBUG("SessionManagerImpl: Session '{}' already exists", sessionId);
        return false;
    }

    // Create session info
    SessionInfo sessionInfo(sessionId, parentSessionId);
    sessions_[sessionId] = sessionInfo;

    LOG_DEBUG("SessionManagerImpl: Created session '{}' with parent '{}' (total sessions: {})", sessionId,
              parentSessionId.empty() ? "none" : parentSessionId, sessions_.size());

    // Notify observers after successful creation
    notifySessionCreated(sessionId, parentSessionId);

    return true;
}

bool SessionManagerImpl::destroySession(const std::string &sessionId) {
    if (sessionId.empty()) {
        LOG_ERROR("SessionManagerImpl: Cannot destroy session with empty ID");
        return false;
    }

    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        LOG_DEBUG("SessionManagerImpl: Session '{}' does not exist for destruction", sessionId);
        return false;
    }

    // Store session info before deletion for notification
    SessionInfo sessionInfo = it->second;
    sessions_.erase(it);

    LOG_DEBUG("SessionManagerImpl: Destroyed session '{}' (remaining sessions: {})", sessionId, sessions_.size());

    // Notify observers after successful destruction
    notifySessionDestroyed(sessionId);

    return true;
}

void SessionManagerImpl::addObserver(ISessionObserver *observer) {
    if (!observer) {
        LOG_ERROR("SessionManagerImpl: Cannot add null observer");
        return;
    }

    std::lock_guard<std::mutex> lock(observersMutex_);
    observers_.insert(observer);
    LOG_DEBUG("SessionManagerImpl: Added session observer (total observers: {})", observers_.size());
}

void SessionManagerImpl::removeObserver(ISessionObserver *observer) {
    if (!observer) {
        LOG_ERROR("SessionManagerImpl: Cannot remove null observer");
        return;
    }

    std::lock_guard<std::mutex> lock(observersMutex_);
    auto removed = observers_.erase(observer);
    if (removed > 0) {
        LOG_DEBUG("SessionManagerImpl: Removed session observer (remaining observers: {})", observers_.size());
    } else {
        LOG_DEBUG("SessionManagerImpl: Observer not found for removal");
    }
}

std::vector<std::string> SessionManagerImpl::getActiveSessions() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    std::vector<std::string> activeSessions;
    activeSessions.reserve(sessions_.size());

    for (const auto &pair : sessions_) {
        activeSessions.push_back(pair.first);
    }

    return activeSessions;
}

std::string SessionManagerImpl::getParentSessionId(const std::string &sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second.parentSessionId;
    }

    return "";  // Session not found
}

bool SessionManagerImpl::updateSessionSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                                      const std::vector<std::string> &ioProcessors) {
    if (sessionId.empty()) {
        LOG_ERROR("SessionManagerImpl: Cannot update system variables for empty session ID");
        return false;
    }

    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        LOG_ERROR("SessionManagerImpl: Cannot update system variables for non-existent session: '{}'", sessionId);
        return false;
    }

    // Update session information
    it->second.sessionName = sessionName;
    it->second.ioProcessors = ioProcessors;

    LOG_DEBUG("SessionManagerImpl: Updated system variables for session '{}': name='{}', {} I/O processors", sessionId,
              sessionName, ioProcessors.size());

    // Notify observers after successful update
    notifySessionSystemVariablesUpdated(sessionId, sessionName, ioProcessors);

    return true;
}

std::string SessionManagerImpl::getSessionName(const std::string &sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second.sessionName;
    }

    return "";  // Session not found
}

std::vector<std::string> SessionManagerImpl::getSessionIOProcessors(const std::string &sessionId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second.ioProcessors;
    }

    return {};  // Session not found
}

// === Private Helper Methods ===

void SessionManagerImpl::notifySessionCreated(const std::string &sessionId, const std::string &parentSessionId) {
    std::lock_guard<std::mutex> lock(observersMutex_);

    for (auto observer : observers_) {
        try {
            observer->onSessionCreated(sessionId, parentSessionId);
        } catch (const std::exception &e) {
            LOG_ERROR("SessionManagerImpl: Observer exception during session creation notification: {}", e.what());
        } catch (...) {
            LOG_ERROR("SessionManagerImpl: Unknown observer exception during session creation notification");
        }
    }
}

void SessionManagerImpl::notifySessionDestroyed(const std::string &sessionId) {
    std::lock_guard<std::mutex> lock(observersMutex_);

    for (auto observer : observers_) {
        try {
            observer->onSessionDestroyed(sessionId);
        } catch (const std::exception &e) {
            LOG_ERROR("SessionManagerImpl: Observer exception during session destruction notification: {}", e.what());
        } catch (...) {
            LOG_ERROR("SessionManagerImpl: Unknown observer exception during session destruction notification");
        }
    }
}

void SessionManagerImpl::notifySessionSystemVariablesUpdated(const std::string &sessionId,
                                                             const std::string &sessionName,
                                                             const std::vector<std::string> &ioProcessors) {
    std::lock_guard<std::mutex> lock(observersMutex_);

    for (auto observer : observers_) {
        try {
            observer->onSessionSystemVariablesUpdated(sessionId, sessionName, ioProcessors);
        } catch (const std::exception &e) {
            LOG_ERROR("SessionManagerImpl: Observer exception during system variables update notification: {}",
                      e.what());
        } catch (...) {
            LOG_ERROR("SessionManagerImpl: Unknown observer exception during system variables update notification");
        }
    }
}

bool SessionManagerImpl::isValidSessionId(const std::string &sessionId) const {
    // Session ID must not be empty and should have reasonable length
    return !sessionId.empty() && sessionId.length() <= 256;
}

bool SessionManagerImpl::isValidParentSession(const std::string &parentSessionId) const {
    // Empty parent session is valid (no parent)
    if (parentSessionId.empty()) {
        return true;
    }

    // If specified, parent session must exist
    return sessions_.find(parentSessionId) != sessions_.end();
}

}  // namespace RSM