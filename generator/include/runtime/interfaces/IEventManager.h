#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SCXML {

namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
class EventQueue;
class EventDispatcher;
}  // namespace Events

namespace Runtime {

/**
 * @brief Interface for event management operations
 */
class IEventManager {
public:
    virtual ~IEventManager() = default;

    // Event operations
    virtual void raiseEvent(Events::EventPtr event) = 0;
    virtual void raiseEvent(const std::string &eventName, const std::string &data = "") = 0;
    virtual void sendEvent(Events::EventPtr event, const std::string &target = "", uint64_t delayMs = 0) = 0;
    virtual void sendEvent(const std::string &eventName, const std::string &target = "", uint64_t delayMs = 0,
                           const std::string &data = "") = 0;

    // Event queues
    virtual std::shared_ptr<Events::EventQueue> getInternalQueue() = 0;
    virtual std::shared_ptr<Events::EventQueue> getExternalQueue() = 0;

    // Event processing
    virtual Events::EventPtr getNextInternalEvent() = 0;
    virtual Events::EventPtr getNextExternalEvent() = 0;
    virtual bool hasInternalEvents() const = 0;
    virtual bool hasExternalEvents() const = 0;
    virtual void clearInternalQueue() = 0;
    virtual void clearExternalQueue() = 0;

    // Event scheduling
    virtual std::string scheduleEvent(Events::EventPtr event, uint64_t delayMs, const std::string &sendId = "") = 0;
    virtual bool cancelScheduledEvent(const std::string &sendId) = 0;

    // Event history
    virtual std::vector<Events::EventPtr> getEventHistory(size_t maxEvents = 100) const = 0;
    virtual void clearEventHistory() = 0;

    // Session management
    virtual void setSessionId(const std::string &sessionId) = 0;
    virtual std::string getSessionId() const = 0;

    // Additional methods for compatibility
    virtual Events::EventPtr getCurrentEvent() const = 0;
};

}  // namespace Runtime
}  // namespace SCXML