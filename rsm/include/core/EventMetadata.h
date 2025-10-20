#pragma once

#include <string>

namespace RSM::Core {

/**
 * @brief Event metadata container for W3C SCXML 5.10 compliance
 *
 * Single Source of Truth for event metadata shared between:
 * - Interpreter engine (StateMachine)
 * - AOT engine (StaticExecutionEngine)
 *
 * Consolidates all event-related metadata into a single structure to simplify
 * API and reduce parameter coupling. All fields are optional (empty string = not set).
 *
 * W3C SCXML 5.10 Event Object Fields:
 * - name: Event name (required)
 * - type: Event type ("internal", "platform", "external")
 * - sendid: Send ID from <send> element
 * - origin: Session ID that sent the event
 * - origintype: Event processor type that sent the event
 * - invokeid: Invoke ID from <invoke> element that generated this event
 * - data: Event payload data
 */
struct EventMetadata {
    std::string name;             ///< Event name (required)
    std::string data;             ///< Event data as JSON string
    std::string type;             ///< Event type ("internal", "platform", "external")
    std::string sendId;           ///< Send ID from <send> element
    std::string invokeId;         ///< Invoke ID from <invoke> element
    std::string originType;       ///< Origin event processor type
    std::string originSessionId;  ///< Origin session ID for _event.origin

    /**
     * @brief Construct minimal event metadata with name and data
     */
    EventMetadata(const std::string &eventName = "", const std::string &eventData = "")
        : name(eventName), data(eventData) {}

    /**
     * @brief Construct full event metadata with all fields
     */
    EventMetadata(const std::string &eventName, const std::string &eventData, const std::string &eventType,
                  const std::string &sendId = "", const std::string &invokeId = "", const std::string &originType = "",
                  const std::string &originSessionId = "")
        : name(eventName), data(eventData), type(eventType), sendId(sendId), invokeId(invokeId), originType(originType),
          originSessionId(originSessionId) {}
};

}  // namespace RSM::Core
