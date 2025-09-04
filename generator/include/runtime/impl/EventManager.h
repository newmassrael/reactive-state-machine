#pragma once

#include "../interfaces/IEventManager.h"
#include "common/Result.h"
#include "events/Event.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace SCXML {
namespace Runtime {

/**
 * @brief Default implementation of event management
 */
class EventManager : public IEventManager {
public:
    EventManager();
    virtual ~EventManager() = default;

    // IEventManager implementation
    void raiseEvent(Events::EventPtr event) override;
    void raiseEvent(const std::string &eventName, const std::string &data = "") override;
    void sendEvent(Events::EventPtr event, const std::string &target = "", uint64_t delayMs = 0) override;
    void sendEvent(const std::string &eventName, const std::string &target = "", uint64_t delayMs = 0,
                   const std::string &data = "") override;

    std::shared_ptr<Events::EventQueue> getInternalQueue() override;
    std::shared_ptr<Events::EventQueue> getExternalQueue() override;

    Events::EventPtr getNextInternalEvent() override;
    Events::EventPtr getNextExternalEvent() override;
    bool hasInternalEvents() const override;
    bool hasExternalEvents() const override;
    void clearInternalQueue() override;
    void clearExternalQueue() override;

    std::string scheduleEvent(Events::EventPtr event, uint64_t delayMs, const std::string &sendId = "") override;
    bool cancelScheduledEvent(const std::string &sendId) override;

    std::vector<Events::EventPtr> getEventHistory(size_t maxEvents = 100) const override;
    void clearEventHistory() override;

    void setSessionId(const std::string &sessionId) override;
    std::string getSessionId() const override;
    Events::EventPtr getCurrentEvent() const override;

    // Additional methods from original RuntimeContext
    Events::EventPtr createEventFromIOProcessor(const std::string &event,
                                                const std::map<std::string, std::string> &data);
    ::SCXML::Common::Result<void> sendInternalEvent(const Events::Event &event);
    ::SCXML::Common::Result<void> sendExternalEvent(const Events::Event &event,
                                                    const std::string &processorType = "scxml");
    ::SCXML::Common::Result<void> scheduleDelayedSend(const Events::Event &event, uint64_t delayMs,
                                                      bool isInternal = false);

private:
    mutable std::mutex eventMutex_;

    std::shared_ptr<Events::EventQueue> internalQueue_;
    std::shared_ptr<Events::EventQueue> externalQueue_;

    // Event history for debugging
    std::vector<Events::EventPtr> eventHistory_;
    size_t maxHistorySize_ = 1000;

    // Scheduled events (original logic)
    std::map<std::string, Events::EventPtr> scheduledSends_;
    static std::atomic<uint64_t> sendCounter_;

    std::string sessionId_;
    Events::EventPtr currentEvent_;

    // Additional original fields
    std::string currentEventName_;
    Events::EventData currentEventData_;

    // Helper methods
    std::string generateSendId();
    void addToHistory(Events::EventPtr event);
    Events::EventPtr createEvent(const std::string &eventName, const std::string &data = "");
};

}  // namespace Runtime
}  // namespace SCXML