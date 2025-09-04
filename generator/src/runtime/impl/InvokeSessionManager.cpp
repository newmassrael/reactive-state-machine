#include "runtime/impl/InvokeSessionManager.h"
#include "common/Logger.h"
#include "model/IInvokeNode.h"
#include <shared_mutex>

namespace SCXML {
namespace Runtime {

InvokeSessionManager::InvokeSessionManager() {
    SCXML::Common::Logger::debug("InvokeSessionManager::Constructor - Creating invoke session manager");
}

// Default destructor is already declared in header

std::string InvokeSessionManager::createInvokeSession(std::shared_ptr<Model::IInvokeNode> invokeNode) {
    if (!invokeNode) {
        SCXML::Common::Logger::error("InvokeSessionManager::createInvokeSession - Invalid invoke node");
        failedInvokes_.fetch_add(1);
        return "";
    }

    std::lock_guard<std::mutex> lock(sessionMutex_);
    std::string invokeId = generateInvokeId();

    // Create new session with proper structure
    auto session = std::make_unique<InvokeSession>();
    session->invokeId = invokeId;
    session->invokeNode = invokeNode;
    session->isActive = true;
    session->isPaused = false;

    sessions_[invokeId] = std::move(session);

    // Update statistics
    totalInvokes_.fetch_add(1);
    successfulInvokes_.fetch_add(1);

    SCXML::Common::Logger::info("InvokeSessionManager::createInvokeSession - Created session: " + invokeId +
                 " for invoke type: " + (invokeNode->getType().empty() ? "default" : invokeNode->getType()));
    return invokeId;
}

bool InvokeSessionManager::terminateInvokeSession(const std::string &sessionId) {
    std::unique_lock<std::mutex> lock(sessionMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        // Mark session as inactive
        it->second->isActive = false;

        // Perform cleanup for the invoke session
        if (it->second->invokeNode) {
            SCXML::Common::Logger::info("InvokeSessionManager::terminateInvokeSession - Cleaning up session: " + sessionId);
        }

        // Remove session
        sessions_.erase(it);
        
        SCXML::Common::Logger::debug("InvokeSessionManager::terminateInvokeSession - Terminated session: " + sessionId);
        return true;
    }
    SCXML::Common::Logger::warning("InvokeSessionManager::terminateInvokeSession - Session not found: " + sessionId);
    return false;
}

void InvokeSessionManager::terminateAllSessions() {
    std::unique_lock<std::mutex> lock(sessionMutex_);
    size_t count = sessions_.size();

    if (count > 0) {
        SCXML::Common::Logger::info("InvokeSessionManager::terminateAllSessions - Terminating " + std::to_string(count) +
                     " active sessions");

        // Cleanup each session properly
        for (auto &pair : sessions_) {
            SCXML::Common::Logger::debug("InvokeSessionManager::terminateAllSessions - Cleaning up session: " + pair.first);
            pair.second->isActive = false;
        }

        sessions_.clear();

        SCXML::Common::Logger::info("InvokeSessionManager::terminateAllSessions - All sessions terminated successfully");
    }
}

// Removed duplicate terminateInvokeSession method

bool InvokeSessionManager::hasInvokeSession(const std::string &invokeId) const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto it = sessions_.find(invokeId);
    return it != sessions_.end() && it->second->isActive;
}

void InvokeSessionManager::forwardEventToInvoke(const std::string &invokeId, Events::EventPtr event) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    if (hasInvokeSession(invokeId)) {
        SCXML::Common::Logger::debug("InvokeSessionManager::forwardEventToInvoke - Forwarded event to invoke: " + invokeId);
        // Would forward to actual invoke implementation
        notifyEventCallback(invokeId, event);
    } else {
        SCXML::Common::Logger::warning("InvokeSessionManager::forwardEventToInvoke - Invoke not found: " + invokeId);
    }
}

void InvokeSessionManager::forwardEventFromInvoke(const std::string &invokeId, Events::EventPtr event) {
    SCXML::Common::Logger::debug("InvokeSessionManager::forwardEventFromInvoke - Received event from invoke: " + invokeId);
    notifyEventCallback(invokeId, event);
}

std::vector<std::string> InvokeSessionManager::getActiveInvokeSessions() const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    std::vector<std::string> activeIds;
    for (const auto &pair : sessions_) {
        if (pair.second->isActive && !pair.second->isPaused) {
            activeIds.push_back(pair.first);
        }
    }
    return activeIds;
}

size_t InvokeSessionManager::getActiveSessionCount() const {
    return getActiveInvokeSessions().size();
}

void InvokeSessionManager::pauseSession(const std::string &invokeId) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto it = sessions_.find(invokeId);
    if (it != sessions_.end()) {
        it->second->isPaused = true;
        SCXML::Common::Logger::info("InvokeSessionManager::pauseSession - Paused session: " + invokeId);
    }
}

void InvokeSessionManager::resumeSession(const std::string &invokeId) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto it = sessions_.find(invokeId);
    if (it != sessions_.end()) {
        it->second->isPaused = false;
        SCXML::Common::Logger::info("InvokeSessionManager::resumeSession - Resumed session: " + invokeId);
    }
}

std::shared_ptr<Model::IInvokeNode> InvokeSessionManager::getInvokeNode(const std::string &invokeId) const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto it = sessions_.find(invokeId);
    return (it != sessions_.end()) ? it->second->invokeNode : nullptr;
}

size_t InvokeSessionManager::getTotalInvokes() const {
    return totalInvokes_.load();
}

size_t InvokeSessionManager::getSuccessfulInvokes() const {
    return successfulInvokes_.load();
}

size_t InvokeSessionManager::getFailedInvokes() const {
    return failedInvokes_.load();
}

void InvokeSessionManager::setInvokeEventCallback(InvokeEventCallback callback) {
    eventCallback_ = callback;
}

std::string InvokeSessionManager::generateInvokeId() {
    return "invoke_" + std::to_string(sessionIdCounter_.fetch_add(1));
}

void InvokeSessionManager::notifyEventCallback(const std::string &invokeId, Events::EventPtr event) {
    if (eventCallback_) {
        eventCallback_(invokeId, event);
    }
}

}  // namespace Runtime
}  // namespace SCXML