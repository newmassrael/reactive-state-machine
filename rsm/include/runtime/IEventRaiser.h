#pragma once

#include <string>

namespace RSM {

/**
 * @brief Interface for raising events in the SCXML system
 *
 * This interface implements the SCXML "fire and forget" event model as specified
 * by W3C SCXML standard. Events are processed asynchronously to prevent deadlocks
 * and ensure proper event ordering. The interface separates event raising from
 * action execution, following the Single Responsibility Principle.
 */
class IEventRaiser {
public:
    virtual ~IEventRaiser() = default;

    /**
     * @brief Raise an event with the given name and data (SCXML "fire and forget")
     *
     * Events are queued for asynchronous processing and this method returns immediately.
     * This implements the SCXML "fire and forget" model to prevent deadlocks and ensure
     * proper event ordering as specified by W3C SCXML standard.
     *
     * @param eventName Name of the event to raise
     * @param eventData Data associated with the event
     * @return true if the event was successfully queued, false if the raiser is not ready
     */
    virtual bool raiseEvent(const std::string &eventName, const std::string &eventData) = 0;

    /**
     * @brief Check if the event raiser is ready to raise events
     * @return true if ready, false otherwise
     */
    virtual bool isReady() const = 0;
};

}  // namespace RSM