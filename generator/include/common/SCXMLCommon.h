#pragma once

/**
 * @file SCXMLCommon.h
 * @brief Common definitions and utilities for the SCXML framework
 *
 * This header includes all common utilities and definitions used throughout
 * the SCXML reactive state machine framework.
 */

// Core result and error handling
#include "Result.h"

// Performance utilities
#include "PerformanceUtils.h"

// Standard library includes commonly used throughout the framework
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SCXML {
namespace Common {

// Common type aliases for convenience
using StringMap = std::unordered_map<std::string, std::string>;
using StringSet = std::unordered_set<std::string>;
using StringVector = std::vector<std::string>;

// Note: Result template is defined in Result.h

// Forward declarations for commonly used types
class RuntimeContext;
class DataModelEngine;
class ExpressionEvaluator;

// Common constants
namespace Constants {
constexpr const char *SCXML_NAMESPACE = "http://www.w3.org/2005/07/scxml";
constexpr const char *DEFAULT_DATAMODEL = "ecmascript";
constexpr int DEFAULT_TIMEOUT_MS = 5000;
constexpr size_t MAX_EVENT_QUEUE_SIZE = 1000;
constexpr size_t MAX_RECURSION_DEPTH = 100;
}  // namespace Constants

// Common utility functions
namespace Utils {
/**
 * @brief Trim whitespace from both ends of a string
 * @param str String to trim
 * @return Trimmed string
 */
inline std::string trim(const std::string &str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }

    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

/**
 * @brief Check if a string is empty or contains only whitespace
 * @param str String to check
 * @return True if empty or whitespace only
 */
inline bool isEmpty(const std::string &str) {
    return trim(str).empty();
}

/**
 * @brief Split a string by delimiter
 * @param str String to split
 * @param delimiter Delimiter character
 * @return Vector of split strings
 */
inline std::vector<std::string> split(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    for (char c : str) {
        if (c == delimiter) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * @brief Join strings with a delimiter
 * @param strings Vector of strings to join
 * @param delimiter Delimiter string
 * @return Joined string
 */
inline std::string join(const std::vector<std::string> &strings, const std::string &delimiter) {
    if (strings.empty()) {
        return "";
    }

    std::string result = strings[0];
    for (size_t i = 1; i < strings.size(); ++i) {
        result += delimiter + strings[i];
    }
    return result;
}

/**
 * @brief Convert string to lowercase
 * @param str String to convert
 * @return Lowercase string
 */
inline std::string toLowerCase(const std::string &str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

/**
 * @brief Check if string starts with prefix
 * @param str String to check
 * @param prefix Prefix to look for
 * @return True if string starts with prefix
 */
inline bool startsWith(const std::string &str, const std::string &prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

/**
 * @brief Check if string ends with suffix
 * @param str String to check
 * @param suffix Suffix to look for
 * @return True if string ends with suffix
 */
inline bool endsWith(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}
}  // namespace Utils

}  // namespace Common
}  // namespace SCXML