#pragma once

#include "IEventTarget.h"
#include "runtime/IEventRaiser.h"
#include <memory>

namespace RSM {

/**
 * @brief Event target for internal SCXML events
 *
 * Implements event delivery to the same SCXML interpreter using
 * the existing raiseEvent mechanism. This bridges the new event
 * system with the current internal event handling.
 */
class InternalEventTarget : public IEventTarget {
public:
    /**
     * @brief Construct internal event target
     * @param eventRaiser Event raiser for raising internal events
     * @param isExternal true for external queue priority, false for internal queue priority
     */
    explicit InternalEventTarget(std::shared_ptr<IEventRaiser> eventRaiser, bool isExternal = false);

    /**
     * @brief Destructor
     */
    virtual ~InternalEventTarget() = default;

    // IEventTarget implementation
    std::future<SendResult> send(const EventDescriptor &event) override;
    std::string getTargetType() const override;
    bool canHandle(const std::string &targetUri) const override;
    std::vector<std::string> validate() const override;
    std::string getDebugInfo() const override;

private:
    std::shared_ptr<IEventRaiser> eventRaiser_;
    bool isExternal_;  // W3C SCXML: true for external queue priority, false for internal

    /**
     * @brief Resolve dynamic event name from expression
     * @param event Event descriptor
     * @return Resolved event name, or original name if no expression
     */
    std::string resolveEventName(const EventDescriptor &event) const;

    /**
     * @brief Convert event descriptor to internal event data
     * @param event Event descriptor
     * @return JSON string with event data
     */
    std::string buildEventData(const EventDescriptor &event) const;
};

}  // namespace RSM