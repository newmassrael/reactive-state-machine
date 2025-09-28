#pragma once

#include "common/Logger.h"
#include "events/IEventDispatcher.h"
#include <future>
#include <memory>
#include <string>

namespace RSM::W3C {

/**
 * @brief W3C SCXML Test-specific EventDispatcher implementation
 *
 * SOLID Architecture Design:
 * - Single Responsibility: Handle event dispatching for W3C test environment only
 * - Open/Closed: Implements IEventDispatcher interface, extensible for W3C features
 * - Liskov Substitution: Drop-in replacement for any IEventDispatcher
 * - Interface Segregation: Only implements required IEventDispatcher methods
 * - Dependency Inversion: RSMTestExecutor depends on IEventDispatcher abstraction
 *
 * W3C Test Environment Characteristics:
 * - Immediate execution: All events execute immediately (no real scheduling)
 * - Session context: Uses sessionId for JavaScript evaluation
 * - Parameter timing: Ensures parameters evaluated at send time, not delivery time
 * - Simplified targets: W3C tests don't require complex target resolution
 */
class W3CTestEventDispatcher : public RSM::IEventDispatcher {
private:
    std::string sessionId_;

    // Store the last event parameters for W3C test access (eventName -> params map)
    mutable std::map<std::string, std::map<std::string, std::string>> lastEventParams_;

    // W3C Compliance: Internal scheduler for delayed events
    struct ScheduledTestEvent {
        EventDescriptor event;
        std::chrono::steady_clock::time_point executeAt;
        std::string sendId;
        bool cancelled = false;

        // W3C SCXML 6.2: Store evaluated parameters at send time (mandatory compliance)
        std::map<std::string, std::string> evaluatedParams;

        ScheduledTestEvent(const EventDescriptor &evt, std::chrono::steady_clock::time_point execTime,
                           const std::string &id, const std::map<std::string, std::string> &evalParams)
            : event(evt), executeAt(execTime), sendId(id), evaluatedParams(evalParams) {}
    };

    mutable std::mutex schedulerMutex_;
    std::map<std::string, std::unique_ptr<ScheduledTestEvent>> scheduledEvents_;
    std::atomic<uint64_t> sendIdCounter_{0};

    /**
     * @brief Execute event immediately for W3C test environment
     * @param event Event descriptor with all necessary information
     * @return Future containing SendResult
     */
    std::future<SendResult> executeEventImmediately(const EventDescriptor &event);

    /**
     * @brief Process any ready scheduled events
     * Called periodically to check if delayed events should execute
     */
    void processReadyEvents();

    /**
     * @brief Generate unique sendId for W3C test events
     * @return Unique send ID string
     */
    std::string generateSendId();

public:
    /**
     * @brief Constructor for W3C test event dispatcher
     * @param sessionId Session ID for JavaScript evaluation context
     */
    explicit W3CTestEventDispatcher(const std::string &sessionId);

    /**
     * @brief Virtual destructor for proper inheritance
     */
    ~W3CTestEventDispatcher() override = default;

    // IEventDispatcher interface implementation

    /**
     * @brief Send event with W3C test semantics
     * @param event Event descriptor containing all event information
     * @return Future containing SendResult with success/error information
     */
    std::future<SendResult> sendEvent(const EventDescriptor &event) override;

    /**
     * @brief Cancel event (W3C SCXML 6.2 compliance)
     * @param sendId ID of event to cancel
     * @return true if event was found and cancelled
     */
    bool cancelEvent(const std::string &sendId) override;

    /**
     * @brief Send delayed event (W3C SCXML compliance with actual delays)
     * @param event Event descriptor
     * @param delay Delay duration (respected for W3C compliance testing)
     * @return Future containing SendResult
     */
    std::future<SendResult> sendEventDelayed(const EventDescriptor &event, std::chrono::milliseconds delay) override;

    /**
     * @brief Check if event is pending (W3C SCXML compliance)
     * @param sendId ID of event to check
     * @return true if event is scheduled but not yet executed
     */
    bool isEventPending(const std::string &sendId) const override;

    /**
     * @brief Get dispatcher statistics for W3C test environment
     * @return Statistics string showing test dispatcher status
     */
    std::string getStatistics() const override;

    /**
     * @brief Shutdown dispatcher (W3C compliance: cancel all pending events)
     */
    void shutdown() override;

    /**
     * @brief Cancel all events for a specific session (W3C SCXML 6.2 compliance)
     * @param sessionId Session whose events should be cancelled
     * @return Number of events cancelled
     */
    size_t cancelEventsForSession(const std::string &sessionId);

    /**
     * @brief Get the parameters from the last dispatched event
     * @param eventName Event name to get parameters for
     * @return Map of parameter name to evaluated value
     */
    std::map<std::string, std::string> getLastEventParams(const std::string &eventName) const;
};

}  // namespace RSM::W3C