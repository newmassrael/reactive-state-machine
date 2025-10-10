#pragma once

#include <string>

namespace RSM {
namespace Log {

/**
 * @brief Sanitize string for safe logging (prevent log injection attacks)
 *
 * Converts control characters to safe representations:
 * - '\n' → "\\n" (visible escape sequence)
 * - '\r' → "\\r" (visible escape sequence)
 * - Other control chars → '?' (placeholder)
 * - Printable ASCII (32-126) → preserved as-is
 *
 * This prevents log injection vulnerabilities where malicious input containing
 * newlines could create fake log entries or hide malicious content.
 *
 * @param input Raw string that may contain control characters
 * @return Sanitized string safe for logging
 */
inline std::string sanitize(const std::string &input) {
    std::string sanitized;
    sanitized.reserve(input.length());

    for (char c : input) {
        if (c == '\n') {
            sanitized += "\\n";
        } else if (c == '\r') {
            sanitized += "\\r";
        } else if (c >= 32 && c < 127) {
            sanitized += c;
        } else {
            sanitized += '?';
        }
    }

    return sanitized;
}

}  // namespace Log
}  // namespace RSM
