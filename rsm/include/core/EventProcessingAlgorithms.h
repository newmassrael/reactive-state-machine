#pragma once

#include "common/Logger.h"
#include <functional>

namespace RSM::Core {

/**
 * @brief W3C SCXML event processing algorithms (Single Source of Truth)
 *
 * Share all event processing logic for Interpreter and AOT engines based on templates.
 *
 * Design principles:
 * 1. Share algorithms only, maintain per-engine data structure optimization
 * 2. Template-based zero overhead (inline expansion)
 * 3. Ensure type safety with clear interfaces
 *
 * @note All methods in this class are static template functions,
 *       inlined at compile time with no runtime overhead.
 */
class EventProcessingAlgorithms {
public:
    /**
     * @brief W3C SCXML 3.12.1: Process internal event queue (FIFO)
     *
     * Exhaust all internal events in FIFO order when macrostep completes.
     * Both Interpreter and AOT engines use the same algorithm.
     *
     * @tparam EventQueue Event queue type
     *   Required methods: bool hasEvents() const, EventType popNext()
     * @tparam EventHandler Event handler callback type
     *   Signature: bool handler(EventType event)
     *
     * @param queue Internal event queue (AOTEventQueue or InterpreterEventQueue)
     * @param handler Event processing function (stops processing if returns false)
     *
     * @example AOT engine:
     * @code
     * AOTEventQueue aotQueue(eventQueue_);
     * processInternalEventQueue(aotQueue, [this](Event e) {
     *     return processInternalEvent(e);
     * });
     * @endcode
     *
     * @example Interpreter engine:
     * @code
     * InterpreterEventQueue interpQueue(eventRaiser_);
     * processInternalEventQueue(interpQueue, [this](auto) {
     *     return true;  // EventRaiser handles internally
     * });
     * @endcode
     */
    template <typename EventQueue, typename EventHandler>
    static void processInternalEventQueue(EventQueue &queue, EventHandler &&handler) {
        // W3C SCXML 3.12.1: Process all internal events in FIFO order
        while (queue.hasEvents()) {
            auto event = queue.popNext();

            // Stop if event processing fails
            if (!handler(event)) {
                LOG_DEBUG("EventProcessingAlgorithms: Event handler returned false, stopping queue processing");
                break;
            }
        }
    }

    /**
     * @brief W3C SCXML 3.13: Check eventless transitions
     *
     * Check transitions that execute automatically without events after state entry.
     * Includes maximum iteration limit to prevent infinite loops.
     *
     * @tparam StateMachine State machine type
     *   Required methods:
     *   - StateType getCurrentState() const
     *   - bool processEventlessTransition()
     *   - void executeOnExit(StateType)
     *   - void executeOnEntry(StateType)
     * @tparam EventQueue Internal event queue type
     * @tparam InternalEventProcessor Internal event processing function type
     *
     * @param sm State machine instance
     * @param queue Internal event queue
     * @param processInternalEvent Internal event processing function
     * @param maxIterations Maximum iteration count (default 100)
     * @return true if any eventless transition occurred, false otherwise
     */
    template <typename StateMachine, typename EventQueue, typename InternalEventProcessor>
    static bool checkEventlessTransitions(StateMachine &sm, EventQueue &queue,
                                          InternalEventProcessor &&processInternalEvent, int maxIterations = 100) {
        bool anyTransition = false;
        int iterations = 0;

        while (iterations++ < maxIterations) {
            auto oldState = sm.getCurrentState();

            // W3C SCXML 3.13: Attempt eventless transition
            if (sm.processEventlessTransition()) {
                auto newState = sm.getCurrentState();

                if (oldState != newState) {
                    anyTransition = true;
                    sm.executeOnExit(oldState);
                    sm.executeOnEntry(newState);

                    // Process internal events after entering new state
                    processInternalEventQueue(queue, processInternalEvent);

                    // Continue checking eventless transitions
                } else {
                    // No state change - stop
                    break;
                }
            } else {
                // No eventless transition - stop
                break;
            }
        }

        if (iterations >= maxIterations) {
            LOG_ERROR("EventProcessingAlgorithms: Eventless transition loop detected after {} iterations",
                      maxIterations);
            return false;
        }

        return anyTransition;
    }

    /**
     * @brief W3C SCXML 3.3 / D.1: Process complete macrostep
     *
     * External event processing → Exhaust internal events → Eventless transitions.
     * Core event processing pattern for Interpreter and AOT engines.
     *
     * @tparam StateMachine State machine type
     * @tparam Event Event type
     * @tparam EventQueue Internal event queue type
     * @tparam InternalEventProcessor Internal event processing function type
     *
     * @param sm State machine instance
     * @param event External event
     * @param queue Internal event queue
     * @param processInternalEvent Internal event processing function
     * @param checkEventless Whether to check eventless transitions (default true)
     */
    template <typename StateMachine, typename Event, typename EventQueue, typename InternalEventProcessor>
    static void processMacrostep(StateMachine &sm, const Event &event, EventQueue &queue,
                                 InternalEventProcessor &&processInternalEvent, bool checkEventless = true) {
        auto oldState = sm.getCurrentState();

        // 1. W3C SCXML 3.12: Attempt transition with external event
        if (sm.processTransition(event)) {
            auto newState = sm.getCurrentState();

            // 2. On state change: execute exit/entry
            if (oldState != newState) {
                sm.executeOnExit(oldState);
                sm.executeOnEntry(newState);

                // 3. W3C SCXML 3.12.1: Process all internal events
                processInternalEventQueue(queue, processInternalEvent);

                // 4. W3C SCXML 3.13: Eventless transitions
                if (checkEventless) {
                    checkEventlessTransitions(sm, queue, processInternalEvent);
                }
            }
        }
    }
};

}  // namespace RSM::Core
