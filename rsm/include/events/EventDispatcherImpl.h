#pragma once

#include "IEventDispatcher.h"
#include "IEventTarget.h"
#include <memory>

namespace RSM {

/**
 * @brief Concrete implementation of IEventDispatcher
 *
 * This implementation coordinates event dispatching by combining an event
 * scheduler for delayed events and an event target factory for creating
 * appropriate targets. It provides the main entry point for SCXML event
 * sending and cancellation operations.
 *
 * Key responsibilities:
 * - Route immediate events directly to targets
 * - Schedule delayed events through the scheduler
 * - Create appropriate event targets using the factory
 * - Manage event cancellation requests
 * - Provide unified interface for all event operations
 */
class EventDispatcherImpl : public IEventDispatcher {
public:
    /**
     * @brief Construct dispatcher with scheduler and target factory
     *
     * @param scheduler Event scheduler for delayed events
     * @param targetFactory Factory for creating event targets
     */
    EventDispatcherImpl(std::shared_ptr<IEventScheduler> scheduler, std::shared_ptr<IEventTargetFactory> targetFactory);

    /**
     * @brief Destructor
     */
    virtual ~EventDispatcherImpl() = default;

    // IEventDispatcher implementation
    std::future<SendResult> sendEvent(const EventDescriptor &event) override;
    std::future<SendResult> sendEventDelayed(const EventDescriptor &event, std::chrono::milliseconds delay) override;
    bool cancelEvent(const std::string &sendId) override;
    bool isEventPending(const std::string &sendId) const override;
    std::string getStatistics() const override;
    void shutdown() override;

private:
    /**
     * @brief Execute an event immediately without scheduling
     *
     * @param event Event to execute
     * @param target Target for event delivery
     * @return Future containing the execution result
     */
    std::future<SendResult> executeEventImmediately(const EventDescriptor &event, std::shared_ptr<IEventTarget> target);

    /**
     * @brief Execution callback for scheduled events
     *
     * This method is called by the scheduler when a delayed event is ready
     * for execution. It handles the actual event delivery to the target.
     *
     * @param event Event to execute
     * @param target Target for event delivery
     * @param sendId Send ID of the event
     * @return Future containing the execution result
     */
    std::future<SendResult> onScheduledEventExecution(const EventDescriptor &event,
                                                      std::shared_ptr<IEventTarget> target, const std::string &sendId);

    std::shared_ptr<IEventScheduler> scheduler_;
    std::shared_ptr<IEventTargetFactory> targetFactory_;
};

}  // namespace RSM