#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace RSM::EventMatchingHelper {

/**
 * @brief W3C SCXML 5.9.3: Event descriptor matching algorithm
 *
 * This is the Single Source of Truth for event matching logic shared between
 * Interpreter and AOT engines.
 *
 * Event Matching Rules (W3C SCXML 5.9.3):
 * 1. Event descriptor may contain multiple tokens separated by spaces
 * 2. Each token is matched against the event name using prefix matching
 * 3. Prefix matching uses dot (.) as token separator
 * 4. Special wildcards:
 *    - "*" matches any event
 *    - "foo.*" matches any event starting with "foo."
 * 5. Token boundaries are enforced: "foo" matches "foo.bar" but NOT "foobar"
 *
 * @param eventName The actual event name (e.g., "foo.zoo", "bar")
 * @param descriptor The event descriptor from transition (e.g., "foo bar", "foo.*", "*")
 * @return true if eventName matches descriptor, false otherwise
 *
 * @example
 * matchesEventDescriptor("foo", "foo bar")      → true  (exact match)
 * matchesEventDescriptor("bar", "foo bar")      → true  (second token)
 * matchesEventDescriptor("foo.zoo", "foo bar")  → true  (prefix match)
 * matchesEventDescriptor("foos", "foo")         → false (token boundary)
 * matchesEventDescriptor("foo.zoo", "foo.*")    → true  (wildcard suffix)
 * matchesEventDescriptor("anything", "*")       → true  (universal wildcard)
 */
inline bool matchesEventDescriptor(const std::string &eventName, const std::string &descriptor) {
    // W3C SCXML 5.9.3: Split descriptor into space-separated tokens
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = descriptor.find(' ');

    while (end != std::string::npos) {
        if (end > start) {
            tokens.push_back(descriptor.substr(start, end - start));
        }
        start = end + 1;
        end = descriptor.find(' ', start);
    }
    if (start < descriptor.length()) {
        tokens.push_back(descriptor.substr(start));
    }

    // If no tokens (empty descriptor), no match
    if (tokens.empty()) {
        return false;
    }

    // W3C SCXML 5.9.3: Event matches if it matches ANY token
    for (const auto &token : tokens) {
        // W3C SCXML 5.9.3: Universal wildcard "*" matches any event
        if (token == "*") {
            return true;
        }

        // W3C SCXML 5.9.3: Wildcard suffix "foo.*" matches "foo.xxx"
        if (token.length() >= 2 && token.substr(token.length() - 2) == ".*") {
            std::string prefix = token.substr(0, token.length() - 1);  // "foo."
            if (eventName.length() >= prefix.length() && eventName.substr(0, prefix.length()) == prefix) {
                return true;
            }
        }

        // W3C SCXML 5.9.3: Exact match
        if (eventName == token) {
            return true;
        }

        // W3C SCXML 5.9.3: Prefix match with dot separator
        // "foo" matches "foo.bar" but NOT "foobar"
        if (eventName.length() > token.length() && eventName[token.length()] == '.' &&
            eventName.substr(0, token.length()) == token) {
            return true;
        }
    }

    return false;
}

}  // namespace RSM::EventMatchingHelper
