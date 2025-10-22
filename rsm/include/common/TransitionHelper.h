#pragma once
#include "common/EventMatchingHelper.h"
#include <string>
#include <vector>

/**
 * @file TransitionHelper.h
 * @brief W3C SCXML 5.9.3 Transition Matching Helper
 *
 * Provides convenience wrappers for event descriptor matching logic.
 * ARCHITECTURE.md: Zero Duplication - delegates to EventMatchingHelper (Single Source of Truth)
 *
 * W3C SCXML 5.9.3: Event descriptors can be:
 * - "*" (wildcard) - matches any event
 * - "foo" - exact match or prefix match for "foo.bar"
 * - "foo.*" - explicit wildcard pattern
 * - "foo bar" - multiple space-separated events
 *
 * Implementation:
 * - matchesEventDescriptor() → delegates to EventMatchingHelper::matchesEventDescriptor()
 * - matchesAnyEventDescriptor() → loops over descriptors using EventMatchingHelper
 * - Both Interpreter and AOT engines use same underlying logic (Zero Duplication)
 */

namespace RSM::TransitionHelper {

/**
 * @brief Check if an event descriptor matches an event name
 *
 * W3C SCXML 3.12 compliant event descriptor matching.
 *
 * @param descriptor Event descriptor from transition (e.g., "*", "foo", "foo.*")
 * @param eventName Event name to match (e.g., "foo", "foo.bar")
 * @return true if descriptor matches eventName, false otherwise
 *
 * @example
 * matchesEventDescriptor("*", "foo") → true (wildcard matches all)
 * matchesEventDescriptor("foo", "foo") → true (exact match)
 * matchesEventDescriptor("foo", "foo.bar") → true (prefix match)
 * matchesEventDescriptor("foo.*", "foo.bar") → true (wildcard pattern)
 * matchesEventDescriptor("bar", "foo") → false (no match)
 */
/**
 * @brief Check if an event descriptor matches an event name
 *
 * W3C SCXML 5.9.3 compliant event descriptor matching.
 * ARCHITECTURE.md: Zero Duplication - delegates to EventMatchingHelper (Single Source of Truth)
 *
 * @param descriptor Event descriptor from transition (e.g., "*", "foo", "foo.*", "foo bar")
 * @param eventName Event name to match (e.g., "foo", "foo.bar")
 * @return true if descriptor matches eventName, false otherwise
 *
 * @example
 * matchesEventDescriptor("*", "foo") → true (wildcard matches all)
 * matchesEventDescriptor("foo", "foo") → true (exact match)
 * matchesEventDescriptor("foo", "foo.bar") → true (prefix match)
 * matchesEventDescriptor("foo.*", "foo.bar") → true (wildcard pattern)
 * matchesEventDescriptor("foo bar", "foo") → true (multiple events)
 * matchesEventDescriptor("bar", "foo") → false (no match)
 */
inline bool matchesEventDescriptor(const std::string &descriptor, const std::string &eventName) {
    // W3C SCXML 5.9.3: Delegate to EventMatchingHelper (Single Source of Truth)
    // ARCHITECTURE.md: Zero Duplication principle - both Interpreter and AOT use same logic
    return EventMatchingHelper::matchesEventDescriptor(eventName, descriptor);
}

/**
 * @brief Check if any event descriptor in a list matches an event name
 *
 * W3C SCXML 3.12: A transition can have multiple event descriptors.
 * The transition matches if at least one descriptor matches.
 *
 * @param descriptors List of event descriptors (e.g., ["foo", "bar.*"])
 * @param eventName Event name to match
 * @return true if any descriptor matches, false otherwise
 *
 * @example
 * matchesAnyEventDescriptor({"foo", "bar"}, "foo") → true
 * matchesAnyEventDescriptor({"foo", "bar"}, "baz") → false
 */
/**
 * @brief Check if any event descriptor in a list matches an event name
 *
 * W3C SCXML 5.9.3: A transition can have multiple event descriptors.
 * The transition matches if at least one descriptor matches.
 * ARCHITECTURE.md: Zero Duplication - uses EventMatchingHelper for all matching
 *
 * @param descriptors List of event descriptors (e.g., ["foo", "bar.*"])
 * @param eventName Event name to match
 * @return true if any descriptor matches, false otherwise
 *
 * @example
 * matchesAnyEventDescriptor({"foo", "bar"}, "foo") → true
 * matchesAnyEventDescriptor({"foo", "bar"}, "baz") → false
 */
inline bool matchesAnyEventDescriptor(const std::vector<std::string> &descriptors, const std::string &eventName) {
    // W3C SCXML 5.9.3: Check if ANY descriptor matches
    // ARCHITECTURE.md: Zero Duplication - uses EventMatchingHelper (Single Source of Truth)
    for (const auto &descriptor : descriptors) {
        if (EventMatchingHelper::matchesEventDescriptor(eventName, descriptor)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if an event descriptor is a wildcard
 *
 * Used by AOT code generator to optimize wildcard handling.
 * Wildcards are treated as catch-all after specific event checks.
 *
 * @param descriptor Event descriptor to check
 * @return true if descriptor is "*", false otherwise
 */
inline bool isWildcardDescriptor(const std::string &descriptor) {
    return descriptor == "*";
}

}  // namespace RSM::TransitionHelper
