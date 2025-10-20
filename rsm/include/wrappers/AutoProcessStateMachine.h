#pragma once

namespace RSM::Wrappers {

/**
 * @brief Convenience wrapper for automatic event queue processing
 *
 * This wrapper provides a simpler API for beginners by automatically
 * processing event queues after raising external events.
 *
 * **Usage**:
 * @code
 * using SM = RSM::Generated::traffic_light::traffic_light;
 * AutoProcessStateMachine<SM> light;
 *
 * light.initialize();
 * light.processEvent(Event::Timer);  // Auto-processes event queue
 * @endcode
 *
 * **Advanced users** can still use the low-level API:
 * @code
 * light.raiseExternal(Event::Timer);  // Queue only
 * light.step();                       // Manual processing
 * @endcode
 *
 * @tparam StateMachine The generated state machine class
 */
template <typename StateMachine> class AutoProcessStateMachine : public StateMachine {
public:
    using StateMachine::StateMachine;
    using Event = typename StateMachine::Event;
    using State = typename StateMachine::State;

    /**
     * @brief Process event with automatic queue processing
     *
     * Convenience method that combines raiseExternal() and step().
     * This is the recommended API for most users.
     *
     * @param event Event to process
     */
    void processEvent(Event event) {
        this->raiseExternal(event);
        this->step();
    }

    /**
     * @brief Process event with data and automatic queue processing
     *
     * @param event Event to process
     * @param eventData Event data (JSON string)
     */
    void processEvent(Event event, const std::string &eventData) {
        this->raiseExternal(event, eventData);
        this->step();
    }

    /**
     * @brief Process event with full metadata and automatic queue processing
     *
     * @param eventWithMetadata Event with complete metadata
     */
    void processEvent(const typename StateMachine::EventWithMetadata &eventWithMetadata) {
        this->raiseExternal(eventWithMetadata);
        this->step();
    }

    // Keep low-level API accessible for advanced users
    using StateMachine::getCurrentState;
    using StateMachine::isInFinalState;
    using StateMachine::raiseExternal;
    using StateMachine::step;
};

}  // namespace RSM::Wrappers
