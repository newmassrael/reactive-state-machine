#pragma once

#include "common/Logger.h"
#include <string>
#include <vector>

/**
 * @file InvokeProcessingAlgorithms.h
 * @brief Single Source of Truth for W3C SCXML Invoke processing (W3C SCXML 6.4)
 *
 * Design principles (same as EventProcessingAlgorithms):
 * 1. Algorithm sharing only - data structures remain engine-specific
 * 2. Template-based zero overhead (compile-time inlining)
 * 3. Clear interface contracts via template parameters
 *
 * W3C SCXML Sections:
 * - 6.4: Invoke element and lifecycle
 * - 6.5: Finalize element execution before child event
 * - 6.4.1: Autoforward attribute for parent-to-child event forwarding
 */

namespace RSM::Core {

class InvokeProcessingAlgorithms {
public:
    /**
     * @brief W3C SCXML 6.5: Execute finalize handler before processing child event
     *
     * The finalize element allows the parent to execute actions before processing
     * an event from a child invoked session. This is useful for data transformation
     * or cleanup before event handling.
     *
     * @tparam InvokeManager Interface for invoke management
     *         Required methods:
     *         - std::string getFinalizeScript(const std::string& childSessionId)
     *
     * @tparam ActionExecutor Interface for action execution
     *         Required methods:
     *         - void executeScript(const std::string& script)
     *
     * @param originSessionId Session ID of the child that sent the event
     * @param invokeManager Invoke manager providing finalize script lookup
     * @param actionExecutor Action executor for script execution
     *
     * @example Interpreter usage:
     * @code
     * RSM::Core::InterpreterInvokeManager adapter(invokeExecutor_);
     * RSM::Core::InvokeProcessingAlgorithms::processFinalize(
     *     event.originSessionId,
     *     adapter,
     *     *actionExecutor_
     * );
     * @endcode
     *
     * @example JIT usage:
     * @code
     * RSM::Core::JITInvokeManager<Policy> adapter(policy_);
     * RSM::Core::InvokeProcessingAlgorithms::processFinalize(
     *     getOriginSessionId(event),
     *     adapter,
     *     *this  // Policy implements ActionExecutor
     * );
     * @endcode
     */
    template <typename InvokeManager, typename ActionExecutor>
    static void processFinalize(const std::string &originSessionId, InvokeManager &invokeManager,
                                ActionExecutor &actionExecutor) {
        // W3C SCXML 6.5: Skip if event not from child session
        if (originSessionId.empty()) {
            return;
        }

        // Lookup finalize script for this child session
        std::string finalizeScript = invokeManager.getFinalizeScript(originSessionId);

        // Execute finalize script if present
        if (!finalizeScript.empty()) {
            LOG_DEBUG("InvokeProcessingAlgorithms: Executing finalize for child session {}", originSessionId);
            actionExecutor.executeScript(finalizeScript);
        }
    }

    /**
     * @brief W3C SCXML 6.4.1: Autoforward events from parent to child sessions
     *
     * When autoforward="true" is set on an invoke element, all non-platform
     * events received by the parent are automatically forwarded to the child.
     * Platform events (starting with "#_") are never forwarded.
     *
     * @tparam Event Event type (engine-specific: string for Interpreter, enum for JIT)
     * @tparam InvokeManager Interface for invoke management
     *         Required methods:
     *         - std::vector<std::shared_ptr<StateMachine>>
     *           getAutoforwardSessions(const std::string& parentSessionId)
     *
     * @param event Event to potentially forward
     * @param parentSessionId Current session ID (parent for forwarding)
     * @param invokeManager Invoke manager providing autoforward session list
     *
     * @note Platform events are filtered automatically (event names starting with "#_")
     *
     * @example Interpreter usage:
     * @code
     * RSM::Core::InterpreterInvokeManager adapter(invokeExecutor_);
     * RSM::Core::InvokeProcessingAlgorithms::processAutoforward(
     *     event,
     *     sessionId_,
     *     adapter
     * );
     * @endcode
     *
     * @example JIT usage:
     * @code
     * RSM::Core::JITInvokeManager<Policy> adapter(policy_);
     * RSM::Core::InvokeProcessingAlgorithms::processAutoforward(
     *     event,
     *     sessionId_,
     *     adapter
     * );
     * @endcode
     */
    template <typename Event, typename InvokeManager>
    static void processAutoforward(const Event &event, const std::string &parentSessionId,
                                   InvokeManager &invokeManager) {
        // W3C SCXML 6.4.1: Never autoforward platform events
        if (isPlatformEvent(event)) {
            LOG_DEBUG("InvokeProcessingAlgorithms: Skipping autoforward for platform event");
            return;
        }

        // Get all child sessions with autoforward enabled
        auto childSessions = invokeManager.getAutoforwardSessions(parentSessionId);

        // Forward event to all autoforward children
        if (!childSessions.empty()) {
            LOG_DEBUG("InvokeProcessingAlgorithms: Autoforwarding event to {} child sessions", childSessions.size());
            for (auto &child : childSessions) {
                if (child) {
                    child->processEvent(event);
                }
            }
        }
    }

private:
    /**
     * @brief Check if event is a platform event (W3C SCXML 5.10.1)
     *
     * Platform events have names starting with "#_" and are internal to the
     * SCXML processor. They should never be autoforwarded to child sessions.
     *
     * @tparam Event Event type (string or engine-specific enum)
     * @param event Event to check
     * @return true if platform event, false otherwise
     */
    template <typename Event> static bool isPlatformEvent(const Event &event) {
        std::string eventName = getEventName(event);
        return !eventName.empty() && eventName.rfind("#_", 0) == 0;
    }

    /**
     * @brief Extract event name from string event (Interpreter)
     * @param event Event string
     * @return Event name
     */
    static std::string getEventName(const std::string &event) {
        return event;
    }

    /**
     * @brief Extract event name from enum event (JIT)
     *
     * For JIT engine with enum-based events, this requires a conversion
     * function that must be provided by the engine.
     *
     * @tparam Event Enum event type
     * @param event Enum event
     * @return Event name string
     */
    template <typename Event> static std::string getEventName(const Event &event) {
        // For enum-based events, rely on toString conversion
        // This will be provided by the generated JIT code
        return eventToString(event);
    }
};

}  // namespace RSM::Core
