#include <functional>
#pragma once

#include "../interfaces/IInvokeSessionManager.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace SCXML {
namespace Runtime {

// Type definition for invoke event callback
using InvokeEventCallback = std::function<void(const std::string&, Events::EventPtr)>;

/**
 * @brief Default implementation of invoke session management
 */
class InvokeSessionManager : public IInvokeSessionManager {
public:
    InvokeSessionManager();
    virtual ~InvokeSessionManager() = default;

    // IInvokeSessionManager interface implementation
    std::string createInvokeSession(std::shared_ptr<Model::IInvokeNode> invokeNode) override;
    std::shared_ptr<Model::IInvokeNode> getInvokeNode(const std::string &invokeId) const override;
    void terminateAllSessions() override;
    size_t getActiveSessionCount() const override;

    // Additional implementation methods (not in interface)
    bool terminateInvokeSession(const std::string &invokeId);
    bool hasInvokeSession(const std::string &invokeId) const;
    void forwardEventToInvoke(const std::string &invokeId, Events::EventPtr event);
    void forwardEventFromInvoke(const std::string &invokeId, Events::EventPtr event);
    std::vector<std::string> getActiveInvokeSessions() const;
    void pauseSession(const std::string &invokeId);
    void resumeSession(const std::string &invokeId);

    size_t getTotalInvokes() const;
    size_t getSuccessfulInvokes() const;
    size_t getFailedInvokes() const;

    void setInvokeEventCallback(InvokeEventCallback callback);

private:
    mutable std::mutex sessionMutex_;

    struct InvokeSession {
        std::string invokeId;
        std::shared_ptr<Model::IInvokeNode> invokeNode;
        bool isActive = true;
        bool isPaused = false;
    };

    std::unordered_map<std::string, std::unique_ptr<InvokeSession>> sessions_;
    std::atomic<uint64_t> sessionIdCounter_{0};

    // Statistics
    std::atomic<size_t> totalInvokes_{0};
    std::atomic<size_t> successfulInvokes_{0};
    std::atomic<size_t> failedInvokes_{0};

    InvokeEventCallback eventCallback_;

    // Helper methods
    std::string generateInvokeId();
    void notifyEventCallback(const std::string &invokeId, Events::EventPtr event);
};

}  // namespace Runtime
}  // namespace SCXML