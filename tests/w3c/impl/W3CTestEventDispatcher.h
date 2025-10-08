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
    // W3C SCXML: Support duplicate param names - each param can have multiple values (Test 178)
    mutable std::map<std::string, std::map<std::string, std::vector<std::string>>> lastEventParams_;

    // REFACTOR: Use shared EventScheduler instead of duplicate implementation
    std::shared_ptr<IEventScheduler> scheduler_;

    /**
     * @brief Execute event immediately for W3C test environment
     * @param event Event descriptor with all necessary information
     * @return Future containing SendResult
     */
    std::future<SendResult> executeEventImmediately(const EventDescriptor &event);

public:
    /**
     * @brief Constructor for W3C test event dispatcher
     * @param sessionId Session ID for JavaScript evaluation context
     * @param scheduler Event scheduler instance (will create one if null)
     */
    explicit W3CTestEventDispatcher(const std::string &sessionId, std::shared_ptr<IEventScheduler> scheduler = nullptr);

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
    bool cancelEvent(const std::string &sendId, const std::string &sessionId = "") override;

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
     * @return Map of parameter name to evaluated values (W3C SCXML: supports duplicate param names - Test 178)
     */
    std::map<std::string, std::vector<std::string>> getLastEventParams(const std::string &eventName) const;
};

}  // namespace RSM::W3C