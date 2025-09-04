#pragma once

#include <memory>
#include <string>

namespace SCXML {

namespace Model {
class IInvokeNode;
}

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {

// Using declarations for Model interfaces
using SCXML::Model::IInvokeNode;

/**
 * @brief Interface for invoke session management operations
 */
class IInvokeSessionManager {
public:
    virtual ~IInvokeSessionManager() = default;

    // Core interface methods
    virtual std::string createInvokeSession(std::shared_ptr<IInvokeNode> invokeNode) = 0;
    virtual std::shared_ptr<IInvokeNode> getInvokeNode(const std::string &invokeId) const = 0;

    // Additional methods needed by RuntimeContext
    virtual void terminateAllSessions() = 0;
    virtual size_t getActiveSessionCount() const = 0;
};

}  // namespace Runtime
}  // namespace SCXML
