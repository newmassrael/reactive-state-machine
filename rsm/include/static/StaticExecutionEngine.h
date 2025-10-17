#pragma once

#include "common/Logger.h"
#include "core/EventMetadata.h"
#include "core/EventProcessingAlgorithms.h"
#include "core/EventQueueAdapters.h"
#include "core/EventQueueManager.h"
#include "core/HierarchicalStateHelper.h"
#include <cstdint>
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
    RSM::Core::EventQueueManager<Event> internalQueue_;  // W3C SCXML C.1: Internal event queue (high priority)
    RSM::Core::EventQueueManager<Event> externalQueue_;  // W3C SCXML C.1: External event queue (low priority)
    bool isRunning_ = false;
    std::function<void()> completionCallback_;  // W3C SCXML 6.4: Callback for done.invoke

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
     * Used by:
     * - <raise> element (W3C SCXML 3.14.1)
     * - <send> with target="#_internal" (W3C SCXML C.1)
     *
     * Delegates to Core::EventQueueManager for W3C compliance.
     *
     * @param event Event to raise internally
     * @param eventData Optional event data as JSON string (W3C SCXML 5.10)
     */
    void raise(Event event, const std::string &eventData = "") {
        internalQueue_.raise(event);  // W3C SCXML C.1: High priority queue

        // W3C SCXML 5.10: Store event data for _event.data access (test176)
        // Note: pendingEventData_ is only present in policies with send params
        if constexpr (requires { policy_.pendingEventData_; }) {
            if (!eventData.empty()) {
                policy_.pendingEventData_ = eventData;
            }
        }
    }

public:
    /**
     * @brief Raise an external event (W3C SCXML C.1, 6.2)
     *
     * External events are placed at the back of the external event queue.
     * They are processed after all internal events have been consumed.
     *
     * Used by:
     * - <send> without target (W3C SCXML 6.2)
     * - <send> with external targets (not #_internal)
     * - <send target="#_parent"> from child state machines (W3C SCXML 6.2)
     *
     * W3C SCXML C.1 (test189): External queue has lower priority than internal queue.
     *
     * @param event Event to raise externally
     * @param eventData Optional event data as JSON string (W3C SCXML 5.10)
     */
    void raiseExternal(Event event, const std::string &eventData = "") {
        externalQueue_.raise(event);  // W3C SCXML C.1: Low priority queue

        // W3C SCXML 5.10: Store event data for _event.data access
        if constexpr (requires { policy_.pendingEventData_; }) {
            if (!eventData.empty()) {
                policy_.pendingEventData_ = eventData;
            }
        }
    }

protected:
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
     * @brief Process both internal and external event queues (W3C SCXML D.1 Algorithm)
     *
     * Processes all queued internal and external events in priority order.
     * Internal events are processed first (high priority), then external events.
     *
     * W3C SCXML C.1 (test189): Internal queue (#_internal target) has higher
     * priority than external queue (no target or external targets).
     *
     * Uses shared EventProcessingAlgorithms for W3C-compliant processing.
     * This ensures Interpreter and JIT engines use identical logic.
     *
     * Supports both static (stateless) and non-static (stateful) policies.
     * Static methods can also be called through an instance in C++.
     */
    void processEventQueues() {
        // W3C SCXML C.1: Process internal queue first (high priority)
        RSM::Core::JITEventQueue<Event> internalAdapter(internalQueue_);
        RSM::Core::EventProcessingAlgorithms::processInternalEventQueue(internalAdapter, [this](Event event) {
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
            return true;  // Continue processing
        });

        // W3C SCXML C.1: Process external queue second (low priority)
        RSM::Core::JITEventQueue<Event> externalAdapter(externalQueue_);
        RSM::Core::EventProcessingAlgorithms::processInternalEventQueue(externalAdapter, [this](Event event) {
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
            return true;  // Continue processing
        });
    }

    /**
     * @brief Check for eventless transitions (W3C SCXML 3.13)
     *
     * Eventless transitions have no event attribute and are evaluated
     * immediately after entering a state. They are checked after all
     * internal events have been processed.
     *
     * Uses shared EventProcessingAlgorithms for W3C-compliant processing.
     * This ensures Interpreter and JIT engines use identical logic.
     *
     * Uses iteration instead of recursion to prevent stack overflow
     * and includes loop detection to prevent infinite cycles.
     */
    void checkEventlessTransitions() {
        static const int MAX_ITERATIONS = 100;  // Safety limit
        int iterations = 0;

        // W3C SCXML 3.13: Use shared algorithm (Single Source of Truth)
        // Note: Eventless transitions can raise new internal events, use internal queue
        RSM::Core::JITEventQueue<Event> adapter(internalQueue_);

        while (iterations++ < MAX_ITERATIONS) {
            State oldState = currentState_;

            // Call processTransition with default event for eventless transitions
            if (policy_.processTransition(currentState_, Event(), *this)) {
                if (oldState != currentState_) {
                    executeOnExit(oldState);
                    executeOnEntry(currentState_);

                    // W3C SCXML 3.12.1: Process any new internal events
                    RSM::Core::EventProcessingAlgorithms::processInternalEventQueue(adapter, [this](Event event) {
                        State oldEventState = currentState_;
                        if (policy_.processTransition(currentState_, event, *this)) {
                            if (oldEventState != currentState_) {
                                executeOnExit(oldEventState);
                                executeOnEntry(currentState_);
                            }
                        }
                        return true;
                    });

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
     * 1. Enter initial state (with hierarchical entry from root to leaf)
     * 2. Execute entry actions (may raise internal events)
     * 3. Process internal event queue
     * 4. Check for eventless transitions
     */
    void initialize() {
        isRunning_ = true;

        // W3C SCXML 3.3: Use HierarchicalStateHelper for correct entry order
        auto entryChain = RSM::Core::HierarchicalStateHelper<StatePolicy>::buildEntryChain(currentState_);

        // Execute entry actions from root to leaf (ancestor first)
        for (const auto &state : entryChain) {
            executeOnEntry(state);
        }

        processEventQueues();
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

        // Note: currentEventMetadata_ is only present in policies with invokes
        // If needed, it's managed by processTransition() and processEvent(Event, EventMetadata)

        State oldState = currentState_;
        // Call through policy instance (works for both static and non-static)
        if (policy_.processTransition(currentState_, event, *this)) {
            if (oldState != currentState_) {
                executeOnExit(oldState);
                executeOnEntry(currentState_);
                processEventQueues();
                checkEventlessTransitions();

                // W3C SCXML 6.4: Notify parent if reached final state
                if (isInFinalState() && completionCallback_) {
                    completionCallback_();
                }
            }
        }
    }

    /**
     * @brief Process an external event with metadata (W3C SCXML 5.10)
     *
     * External events with metadata support originSessionId for invoke finalize.
     * Used when events come from child sessions via invoke.
     *
     * @param event External event to process
     * @param metadata Event metadata (originSessionId, etc.)
     */
    void processEvent(Event event, const RSM::Core::EventMetadata &metadata) {
        if (!isRunning_) {
            return;
        }

        // Set event metadata for invoke processing
        policy_.currentEventMetadata_ = metadata;

        State oldState = currentState_;
        // Call through policy instance (works for both static and non-static)
        if (policy_.processTransition(currentState_, event, *this)) {
            if (oldState != currentState_) {
                executeOnExit(oldState);
                executeOnEntry(currentState_);
                processEventQueues();
                checkEventlessTransitions();

                // W3C SCXML 6.4: Notify parent if reached final state
                if (isInFinalState() && completionCallback_) {
                    completionCallback_();
                }
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

    /**
     * @brief Tick scheduler and process ready internal events (W3C SCXML 6.2)
     *
     * For single-threaded JIT engines with delayed send support.
     * This method polls the event scheduler and processes any ready scheduled events
     * without injecting an external event. Should be called periodically in a polling
     * loop to allow delayed sends to fire at the correct time.
     *
     * Implementation: Calls processTransition to trigger scheduler check, which
     * raises ready scheduled events to the internal queue, then processes them.
     * The dummy event parameter won't match transitions in final states.
     */
    void tick() {
        if (!isRunning_ || isInFinalState()) {
            return;
        }

        // Trigger scheduler check by calling processTransition
        // Use Event() which is typically the first enum value (e.g., Wildcard)
        // This triggers the scheduler check in processTransition, which raises
        // any ready scheduled events to the internal queue.
        State oldState = currentState_;
        if (policy_.processTransition(currentState_, Event(), *this)) {
            // Only execute state change actions if state actually changed
            if (oldState != currentState_) {
                executeOnExit(oldState);
                executeOnEntry(currentState_);
                processEventQueues();
                checkEventlessTransitions();

                // W3C SCXML 6.4: Notify parent if reached final state
                if (isInFinalState() && completionCallback_) {
                    completionCallback_();
                }
            }
        }

        // Even if no transition taken, process internal queue in case
        // scheduler raised events that should be processed
        processEventQueues();
        checkEventlessTransitions();
    }

    /**
     * @brief Set completion callback for done.invoke event generation (W3C SCXML 6.4)
     *
     * This callback is invoked when the state machine reaches a final state.
     * Used by parent to generate done.invoke.{id} events.
     *
     * @param callback Function to call on completion (nullptr to clear)
     */
    void setCompletionCallback(std::function<void()> callback) {
        completionCallback_ = callback;
    }

    /**
     * @brief Get access to policy for parameter passing (W3C SCXML 6.4)
     *
     * Used by parent state machines to pass invoke parameters to child state machines.
     * Allows setting datamodel variables before calling initialize().
     *
     * @return Reference to policy instance
     */
    StatePolicy &getPolicy() {
        return policy_;
    }
};

}  // namespace RSM::Static
