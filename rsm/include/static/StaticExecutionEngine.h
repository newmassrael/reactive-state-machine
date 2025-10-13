#pragma once

#include "common/Logger.h"
#include "core/EventQueueManager.h"
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

namespace RSM::Static {

/**
 * @brief Template-based SCXML execution engine for static code generation
 *
 * This engine implements the core SCXML execution semantics (event queue management,
 * entry/exit actions, transitions) while delegating state-specific logic to the
 * StatePolicy template parameter.
 *
 * Key SCXML standards implemented:
 * - Internal event queue with FIFO ordering (W3C SCXML 3.12.1)
 * - Entry/exit action execution (W3C SCXML 3.7, 3.8)
 * - Event processing loop (W3C SCXML D.1)
 *
 * @tparam StatePolicy Policy class providing state-specific implementations
 *         Must provide: State, Event enums, transition logic, action execution
 */
template <typename StatePolicy> class StaticExecutionEngine {
    friend StatePolicy;

public:
    using State = typename StatePolicy::State;
    using Event = typename StatePolicy::Event;

private:
    State currentState_;
    RSM::Core::EventQueueManager<Event> eventQueue_;  // Shared core component
    bool isRunning_ = false;

protected:
    StatePolicy policy_;  // Policy instance for stateful policies

protected:
    /**
     * @brief Raise an internal event (W3C SCXML 3.14.1)
     *
     * Internal events are placed at the back of the internal event queue.
     * They are processed before external events but after currently queued
     * internal events (FIFO ordering).
     *
     * Delegates to Core::EventQueueManager for W3C compliance.
     *
     * @param event Event to raise internally
     */
    void raise(Event event) {
        eventQueue_.raise(event);  // Use shared core implementation
    }

    /**
     * @brief Execute entry actions for a state (W3C SCXML 3.7)
     *
     * Entry actions are executable content that runs when entering a state.
     * This includes <onentry> blocks which may contain <raise>, <assign>, etc.
     *
     * Supports both static (stateless) and non-static (stateful) policies.
     * Static methods can also be called through an instance in C++.
     *
     * @param state State being entered
     */
    void executeOnEntry(State state) {
        // Call through policy instance (works for both static and non-static)
        policy_.executeEntryActions(state, *this);
    }

    /**
     * @brief Execute exit actions for a state (W3C SCXML 3.8)
     *
     * Exit actions are executable content that runs when exiting a state.
     * This includes <onexit> blocks.
     *
     * Supports both static (stateless) and non-static (stateful) policies.
     * Static methods can also be called through an instance in C++.
     *
     * @param state State being exited
     */
    void executeOnExit(State state) {
        // Call through policy instance (works for both static and non-static)
        policy_.executeExitActions(state, *this);
    }

    /**
     * @brief Process internal event queue (W3C SCXML D.1 Algorithm)
     *
     * Processes all queued internal events in FIFO order. This implements
     * the macrostep completion logic where all internal events generated
     * during state entry are processed before external events.
     *
     * Delegates to Core::EventQueueManager for W3C-compliant queue processing.
     *
     * Supports both static (stateless) and non-static (stateful) policies.
     * Static methods can also be called through an instance in C++.
     */
    void processInternalQueue() {
        eventQueue_.processAll([this](Event event) {
            // Process event through transition logic
            State oldState = currentState_;
            // Call through policy instance (works for both static and non-static)
            if (policy_.processTransition(currentState_, event, *this)) {
                // Transition occurred: execute exit/entry actions
                if (oldState != currentState_) {
                    executeOnExit(oldState);
                    executeOnEntry(currentState_);
                }
            }
            return false;  // Return value not used in current implementation
        });
    }

    /**
     * @brief Check for eventless transitions (W3C SCXML 3.13)
     *
     * Eventless transitions have no event attribute and are evaluated
     * immediately after entering a state. They are checked after all
     * internal events have been processed.
     *
     * Uses iteration instead of recursion to prevent stack overflow
     * and includes loop detection to prevent infinite cycles.
     */
    void checkEventlessTransitions() {
        static const int MAX_ITERATIONS = 100;  // Safety limit
        int iterations = 0;

        while (iterations++ < MAX_ITERATIONS) {
            State oldState = currentState_;
            // Call processTransition with default event for eventless transitions
            if (policy_.processTransition(currentState_, Event(), *this)) {
                if (oldState != currentState_) {
                    executeOnExit(oldState);
                    executeOnEntry(currentState_);
                    // Process any new internal events
                    processInternalQueue();
                    // Continue loop to check for more eventless transitions
                } else {
                    // Transition taken but state didn't change - stop
                    break;
                }
            } else {
                // No eventless transition available - stop
                break;
            }
        }

        if (iterations >= MAX_ITERATIONS) {
            // Eventless transition loop detected
            LOG_ERROR("StaticExecutionEngine: Eventless transition loop detected after {} iterations - stopping state "
                      "machine",
                      MAX_ITERATIONS);
            stop();
        }
    }

public:
    StaticExecutionEngine() : currentState_(StatePolicy::initialState()) {}

    /**
     * @brief Initialize state machine (W3C SCXML 3.2)
     *
     * Performs the initial configuration:
     * 1. Enter initial state
     * 2. Execute entry actions (may raise internal events)
     * 3. Process internal event queue
     * 4. Check for eventless transitions
     */
    void initialize() {
        isRunning_ = true;
        executeOnEntry(currentState_);
        processInternalQueue();
        checkEventlessTransitions();
    }

    /**
     * @brief Process an external event (W3C SCXML 3.12)
     *
     * External events are processed after all internal events have been
     * consumed. Each external event triggers a macrostep.
     *
     * Supports both static (stateless) and non-static (stateful) policies.
     * Static methods can also be called through an instance in C++.
     *
     * @param event External event to process
     */
    void processEvent(Event event) {
        if (!isRunning_) {
            return;
        }

        State oldState = currentState_;
        // Call through policy instance (works for both static and non-static)
        if (policy_.processTransition(currentState_, event, *this)) {
            if (oldState != currentState_) {
                executeOnExit(oldState);
                executeOnEntry(currentState_);
                processInternalQueue();
                checkEventlessTransitions();
            }
        }
    }

    /**
     * @brief Get current state
     * @return Current active state
     */
    State getCurrentState() const {
        return currentState_;
    }

    /**
     * @brief Check if in a final state (W3C SCXML 3.3)
     * @return true if current state is final
     */
    bool isInFinalState() const {
        return StatePolicy::isFinalState(currentState_);
    }

    /**
     * @brief Check if state machine is running
     * @return true if running (not stopped or completed)
     */
    bool isRunning() const {
        return isRunning_;
    }

    /**
     * @brief Stop state machine execution
     */
    void stop() {
        isRunning_ = false;
    }
};

}  // namespace RSM::Static
