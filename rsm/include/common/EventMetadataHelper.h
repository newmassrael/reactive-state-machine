#pragma once

#include "SCXMLTypes.h"
#include <string>

// Forward declaration to avoid circular dependency
namespace RSM::Static {
template <typename StatePolicy> class StaticExecutionEngine;
}

namespace RSM::Common {

/**
 * @brief Helper for W3C SCXML 5.10 event metadata management
 *
 * Provides Single Source of Truth for event metadata operations across
 * Interpreter and AOT (Static) engines, following ARCHITECTURE.md Zero Duplication Principle.
 *
 * W3C SCXML 5.10: The System Variables (_event object)
 * W3C SCXML 5.10.1: Event Descriptor fields (name, data, type, sendid, origin, origintype, invokeid)
 *
 * Related Helpers:
 * - SendHelper: W3C SCXML 6.2 send action support
 * - ForeachHelper: W3C SCXML 5.9 foreach iteration
 * - GuardHelper: W3C SCXML 3.12.1 conditional guard evaluation
 *
 * @example Interpreter Engine Usage
 * @code
 * auto event = std::make_shared<Event>("foo", "external");
 * EventMetadataHelper::setEventMetadata(
 *     *event,
 *     "sm_session_123",           // origin
 *     "http://www.w3.org/...",    // originType
 *     "send_456",                  // sendId
 *     "invoke_789"                 // invokeId
 * );
 * @endcode
 *
 * @example AOT Engine Usage
 * @code
 * EventWithMetadata eventWrapper(Event::Foo, "data", "origin_session", "send_id", "external");
 * EventMetadataHelper::populatePolicyFromMetadata(policy_, eventWrapper);
 * // Now policy_.pendingEventOrigin_ = "origin_session", etc.
 * @endcode
 */
class EventMetadataHelper {
public:
    /**
     * @brief Set all W3C SCXML 5.10.1 event metadata fields on Event object
     *
     * Used by Interpreter engine to populate Event Descriptor fields.
     * Follows W3C SCXML 5.10.1 specification for event metadata structure.
     *
     * @param event Event object to populate (must be valid reference)
     * @param origin W3C SCXML 5.10.1: URL for bidirectional communication (test336)
     * @param originType W3C SCXML 5.10.1: Event processor type (e.g.,
     * "http://www.w3.org/TR/scxml/#SCXMLEventProcessor")
     * @param sendId W3C SCXML 5.10.1: Send action identifier (test332)
     * @param invokeId W3C SCXML 5.10.1: Invoke element identifier
     *
     * @note Empty strings are allowed for optional fields
     * @note This is a Single Source of Truth for metadata setting across engines
     */
    static void setEventMetadata(Event &event, const std::string &origin = "", const std::string &originType = "",
                                 const std::string &sendId = "", const std::string &invokeId = "") {
        // W3C SCXML 5.10.1: Set origin if provided (test336)
        if (!origin.empty()) {
            event.setOrigin(origin);
        }

        // W3C SCXML 5.10.1: Set originType if provided
        if (!originType.empty()) {
            event.setOriginType(originType);
        }

        // W3C SCXML 5.10.1: Set sendId if provided (test332)
        if (!sendId.empty()) {
            event.setSendId(sendId);
        }

        // W3C SCXML 5.10.1: Set invokeId if provided
        if (!invokeId.empty()) {
            event.setInvokeId(invokeId);
        }
    }

    /**
     * @brief Populate AOT engine policy from EventWithMetadata wrapper
     *
     * Used by AOT (Static) engine to extract metadata from queue and store in policy
     * for _event variable binding. Follows W3C SCXML 5.10 event descriptor semantics.
     *
     * This method uses C++20 concepts to check if policy has the required fields,
     * allowing it to work with policies that may not have all metadata fields.
     *
     * @tparam Policy AOT engine policy type (must have pendingEvent* fields)
     * @param policy Policy instance to populate
     * @param metadata EventWithMetadata wrapper from event queue
     *
     * @note Uses if constexpr to conditionally populate only existing fields
     * @note Preserves metadata across event processing cycles (INCOMING event properties)
     *
     * @example
     * @code
     * // In StaticExecutionEngine::processEventQueues()
     * EventWithMetadata eventWithMeta = queue.pop();
     * EventMetadataHelper::populatePolicyFromMetadata(policy_, eventWithMeta);
     * // Now policy_ has all metadata fields set for _event binding
     * @endcode
     */
    template <typename Policy, typename EventType>
    static void
    populatePolicyFromMetadata(Policy &policy,
                               const typename RSM::Static::StaticExecutionEngine<Policy>::EventWithMetadata &metadata) {
        // W3C SCXML 5.10: Set pending event data for _event.data access (test176)
        if constexpr (requires { policy.pendingEventData_; }) {
            policy.pendingEventData_ = metadata.data;
        }

        // W3C SCXML 5.10.1: Set pending event origin for _event.origin access (test336)
        if constexpr (requires { policy.pendingEventOrigin_; }) {
            policy.pendingEventOrigin_ = metadata.origin;
        }

        // W3C SCXML 5.10.1: Set pending event sendId for _event.sendid access (test332)
        if constexpr (requires { policy.pendingEventSendId_; }) {
            policy.pendingEventSendId_ = metadata.sendId;
        }

        // W3C SCXML 5.10.1: Set pending event type for _event.type access (test331)
        if constexpr (requires { policy.pendingEventType_; }) {
            policy.pendingEventType_ = metadata.type;
        }

        // W3C SCXML 5.10.1: Set pending event originType for _event.origintype access
        if constexpr (requires { policy.pendingEventOriginType_; }) {
            policy.pendingEventOriginType_ = metadata.originType;
        }

        // W3C SCXML 5.10.1: Set pending event invokeId for _event.invokeid access
        if constexpr (requires { policy.pendingEventInvokeId_; }) {
            policy.pendingEventInvokeId_ = metadata.invokeId;
        }
    }

    /**
     * @brief Clear all metadata fields in policy (W3C SCXML 5.10)
     *
     * Called at the end of processTransition to clear _event binding for next cycle.
     * Follows W3C SCXML 5.10 semantics: _event is bound only during transition processing.
     *
     * @tparam Policy AOT engine policy type
     * @param policy Policy instance to clear
     *
     * @note Uses if constexpr to conditionally clear only existing fields
     *
     * @example
     * @code
     * // In generated processTransition():
     * bool transitionTaken = processTransition(...);
     * EventMetadataHelper::clearPolicyMetadata(policy_);  // Clear for next cycle
     * @endcode
     */
    template <typename Policy> static void clearPolicyMetadata(Policy &policy) {
        // W3C SCXML 5.10: Clear event name for next cycle
        if constexpr (requires { policy.pendingEventName_; }) {
            policy.pendingEventName_.clear();
        }

        // W3C SCXML 5.10: Clear event data for next cycle
        if constexpr (requires { policy.pendingEventData_; }) {
            policy.pendingEventData_.clear();
        }

        // W3C SCXML 5.10.1: Clear event type for next cycle (test331)
        if constexpr (requires { policy.pendingEventType_; }) {
            policy.pendingEventType_.clear();
        }

        // W3C SCXML 5.10.1: Clear event sendId for next cycle (test332)
        if constexpr (requires { policy.pendingEventSendId_; }) {
            policy.pendingEventSendId_.clear();
        }

        // W3C SCXML 5.10.1: Clear event origin for next cycle (test336)
        if constexpr (requires { policy.pendingEventOrigin_; }) {
            policy.pendingEventOrigin_.clear();
        }

        // W3C SCXML 5.10.1: Clear event originType for next cycle
        if constexpr (requires { policy.pendingEventOriginType_; }) {
            policy.pendingEventOriginType_.clear();
        }

        // W3C SCXML 5.10.1: Clear event invokeId for next cycle
        if constexpr (requires { policy.pendingEventInvokeId_; }) {
            policy.pendingEventInvokeId_.clear();
        }
    }
};

}  // namespace RSM::Common
