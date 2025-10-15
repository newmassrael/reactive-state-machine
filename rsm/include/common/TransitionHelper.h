#pragma once
#include <string>
#include <vector>

/**
 * @file TransitionHelper.h
 * @brief W3C SCXML 3.12 Transition Matching Helper
 *
 * Single Source of Truth for event descriptor matching logic shared between
 * Interpreter engine (runtime) and JIT engine (code generation).
 *
 * W3C SCXML 3.12: Event descriptors can be:
 * - "*" (wildcard) - matches any event
 * - "foo" - exact match or prefix match for "foo.bar"
 * - "foo.*" - explicit wildcard pattern
 *
 * ARCHITECTURE.md: Zero Duplication principle
 * - Interpreter engine: Calls matchesEventDescriptor() at runtime
 * - JIT engine: Generates code that calls matchesEventDescriptor()
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
inline bool matchesEventDescriptor(const std::string &descriptor, const std::string &eventName) {
    // Skip malformed/empty descriptors
    if (descriptor.empty()) {
        return false;
    }

    // W3C SCXML 3.12: Wildcard "*" matches any event
    if (descriptor == "*") {
        return true;
    }

    // W3C SCXML 3.12: Exact match
    if (descriptor == eventName) {
        return true;
    }

    // W3C SCXML 3.12: Wildcard pattern "foo.*" matches "foo", "foo.bar", "foo.bar.baz"
    constexpr size_t WILDCARD_SUFFIX_LEN = 2;  // Length of ".*"
    if (descriptor.ends_with(".*")) {
        std::string prefix = descriptor.substr(0, descriptor.length() - WILDCARD_SUFFIX_LEN);
        // Match if event is exactly the prefix OR starts with "prefix."
        if (eventName == prefix || eventName.starts_with(prefix + ".")) {
            return true;
        }
    } else {
        // W3C SCXML 3.12: Token-based prefix matching
        // "foo" matches "foo.bar" but NOT "foobar"
        if (eventName.starts_with(descriptor + ".")) {
            return true;
        }
    }

    return false;
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
inline bool matchesAnyEventDescriptor(const std::vector<std::string> &descriptors, const std::string &eventName) {
    for (const auto &descriptor : descriptors) {
        if (matchesEventDescriptor(descriptor, eventName)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if an event descriptor is a wildcard
 *
 * Used by JIT code generator to optimize wildcard handling.
 * Wildcards are treated as catch-all after specific event checks.
 *
 * @param descriptor Event descriptor to check
 * @return true if descriptor is "*", false otherwise
 */
inline bool isWildcardDescriptor(const std::string &descriptor) {
    return descriptor == "*";
}

}  // namespace RSM::TransitionHelper
