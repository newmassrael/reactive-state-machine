#pragma once

#include "common/UniqueIdGenerator.h"
#include <string>

namespace RSM {

/**
 * @brief Helper functions for W3C SCXML <send> element processing
 *
 * Single Source of Truth for send action validation logic shared between:
 * - Interpreter engine (ActionExecutorImpl)
 * - AOT engine (StaticCodeGenerator)
 *
 * W3C SCXML References:
 * - 6.2: Send element semantics
 * - 5.10: Error handling for send
 */
class SendHelper {
public:
    /**
     * @brief Check if target validation failed (for error.execution detection)
     *
     * Single Source of Truth for target validation logic.
     * Used by AOT engine to determine if error.execution should be raised,
     * which stops execution of subsequent executable content per W3C SCXML 5.10.
     *
     * W3C SCXML 6.2: Target values starting with "!" are invalid.
     *
     * @param target Target to check
     * @return true if target is invalid (starts with '!')
     */
    static bool isInvalidTarget(const std::string &target) {
        // W3C SCXML 6.2: Target values starting with "!" are invalid
        return !target.empty() && target[0] == '!';
    }

    /**
     * @brief Check if target should use internal event queue (W3C SCXML C.1)
     *
     * Single Source of Truth for internal queue routing logic.
     * ARCHITECTURE.md: Zero Duplication - used by both Interpreter and AOT engines.
     *
     * Usage:
     * - Interpreter: EventTargetFactoryImpl::createTarget() (rsm/src/events/EventTargetFactoryImpl.cpp)
     * - AOT: StaticCodeGenerator send.jinja2 template (tools/codegen/templates/actions/send.jinja2)
     *
     * W3C SCXML C.1 (test189, test495): Events with target="#_internal" must go to
     * the internal event queue, which has higher priority than external queue.
     *
     * @param target Target to check
     * @return true if target is "#_internal" (internal queue), false otherwise (external queue)
     */
    static bool isInternalTarget(const std::string &target) {
        // W3C SCXML C.1: #_internal indicates internal event queue
        return target == "#_internal";
    }

    /**
     * @brief Validate send target according to W3C SCXML 6.2
     *
     * W3C SCXML 6.2 (tests 159, 194): Invalid target values (e.g., starting with "!")
     * must raise error.execution and stop subsequent executable content.
     *
     * This function reuses isInvalidTarget() to avoid code duplication.
     *
     * @param target Target string to validate
     * @param errorMsg Output parameter for error message if validation fails
     * @return true if valid, false if invalid (error.execution should be raised)
     */
    static bool validateTarget(const std::string &target, std::string &errorMsg) {
        if (isInvalidTarget(target)) {
            errorMsg = "Invalid target value: " + target;
            return false;
        }
        return true;
    }

    /**
     * @brief Generate unique sendid (Single Source of Truth)
     *
     * Used by both Interpreter and AOT engines to ensure consistent sendid format.
     * Delegates to centralized UniqueIdGenerator for thread-safe, collision-free IDs.
     *
     * W3C SCXML 6.2: Each send action must have a unique sendid for tracking.
     *
     * @return Unique sendid string (format: "send_timestamp_counter")
     */
    static std::string generateSendId() {
        return UniqueIdGenerator::generateSendId();
    }

    /**
     * @brief Send event to parent state machine (Single Source of Truth)
     *
     * W3C SCXML 6.2: Handles <send target="#_parent"> semantics for child-to-parent
     * event communication in invoked state machines.
     *
     * Single Source of Truth for #_parent event routing shared between:
     * - Interpreter engine (ParentEventTarget)
     * - AOT engine (StaticCodeGenerator generated code)
     *
     * This template function enables compile-time type-safe parent event routing
     * in statically generated code while maintaining zero-overhead abstraction.
     *
     * @tparam ParentStateMachine Parent state machine type (CRTP)
     * @tparam EventType Parent's Event enum type
     * @param parent Pointer to parent state machine (nullptr-safe)
     * @param event Event to send to parent
     * @return true if event was sent successfully, false if parent is null
     */
    template <typename ParentStateMachine, typename EventType>
    static bool sendToParent(ParentStateMachine *parent, EventType event) {
        if (parent) {
            // W3C SCXML 6.2: Send to parent's external event queue
            parent->raiseExternal(event);
            return true;
        }
        return false;
    }

    /**
     * @brief Send event to parent with invokeid metadata (W3C SCXML 6.4.1)
     *
     * W3C SCXML 6.4.1 (test338): When a child sends an event to its parent,
     * the _event.invokeid field must be set to the invokeid of the invoke
     * that created the child.
     *
     * @param parent Parent state machine pointer
     * @param event Event to send
     * @param invokeId Invokeid of the child (from parent's invoke element)
     * @return true if event was sent successfully, false if parent is null
     */
    template <typename ParentStateMachine, typename EventType>
    static bool sendToParent(ParentStateMachine *parent, EventType event, const std::string &invokeId) {
        if (parent) {
            // W3C SCXML 6.4.1: Create event with invokeid metadata
            typename ParentStateMachine::EventWithMetadata eventWithMetadata(event);
            eventWithMetadata.invokeId = invokeId;

            // W3C SCXML 6.2: Send to parent's external event queue
            parent->raiseExternal(eventWithMetadata);
            return true;
        }
        return false;
    }

    /**
     * @brief Store sendid in idlocation variable (Single Source of Truth)
     *
     * W3C SCXML 6.2.4 (test 183): The idlocation attribute specifies a variable
     * where the generated sendid should be stored for later reference.
     *
     * This method encapsulates the idlocation storage logic shared between:
     * - Interpreter engine (ActionExecutorImpl::executeSendAction)
     * - AOT engine (StaticCodeGenerator::generateActionCode for SEND)
     *
     * @param jsEngine JSEngine instance for variable operations
     * @param sessionId Session identifier
     * @param idLocation Variable name to store sendid (empty = no storage)
     * @param sendId Generated sendid value to store
     */
    template <typename JSEngineType>
    static void storeInIdLocation(JSEngineType &jsEngine, const std::string &sessionId, const std::string &idLocation,
                                  const std::string &sendId) {
        if (!idLocation.empty()) {
            jsEngine.setVariable(sessionId, idLocation, sendId);
        }
    }
};

}  // namespace RSM
