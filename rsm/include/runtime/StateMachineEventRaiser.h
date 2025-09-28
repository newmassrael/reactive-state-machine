#pragma once

#include "IEventRaiser.h"
#include <functional>
#include <memory>

namespace RSM {

// Forward declaration
class StateMachine;

/**
 * @brief Event raiser that delegates to StateMachine.processEvent()
 *
 * This implementation follows SOLID principles by:
 * - Single Responsibility: Only handles event raising for StateMachine
 * - Dependency Inversion: Depends on StateMachine abstraction via callback
 * - Interface Segregation: Implements only IEventRaiser interface
 */
class StateMachineEventRaiser : public IEventRaiser {
public:
    /**
     * @brief Constructor with StateMachine event processor callback
     * @param eventProcessor Function that processes events (typically StateMachine::processEvent)
     */
    explicit StateMachineEventRaiser(std::function<bool(const std::string &, const std::string &)> eventProcessor);

    /**
     * @brief Destructor
     */
    virtual ~StateMachineEventRaiser() = default;

    // IEventRaiser interface
    bool raiseEvent(const std::string &eventName, const std::string &eventData = "") override;
    bool isReady() const override;
    void setImmediateMode(bool immediate) override;
    void processQueuedEvents() override;

private:
    std::function<bool(const std::string &, const std::string &)> eventProcessor_;
};

}  // namespace RSM