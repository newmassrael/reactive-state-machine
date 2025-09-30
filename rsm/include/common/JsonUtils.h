#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace RSM {

// Type alias for compatibility
using json = nlohmann::json;

/**
 * @brief Centralized JSON processing utilities using nlohmann/json
 *
 * Eliminates duplicate JSON parsing/serialization logic across components.
 * Provides consistent error handling and formatting for all JSON operations.
 */
class JsonUtils {
public:
    /**
     * @brief Parse JSON string into json object with error handling
     * @param jsonString Input JSON string
     * @param errorOut Optional error message output
     * @return Parsed json object or nullopt on failure
     */
    static std::optional<json> parseJson(const std::string &jsonString, std::string *errorOut = nullptr);

    /**
     * @brief Serialize json object to compact JSON string
     * @param value json object to serialize
     * @return Compact JSON string
     */
    static std::string toCompactString(const json &value);

    /**
     * @brief Serialize json object to pretty-formatted JSON string
     * @param value json object to serialize
     * @return Pretty-formatted JSON string
     */
    static std::string toPrettyString(const json &value);

    /**
     * @brief Safely get string value from JSON object
     * @param object JSON object
     * @param key Key to lookup
     * @param defaultValue Default value if key doesn't exist
     * @return String value or default
     */
    static std::string getString(const json &object, const std::string &key, const std::string &defaultValue = "");

    /**
     * @brief Safely get integer value from JSON object
     * @param object JSON object
     * @param key Key to lookup
     * @param defaultValue Default value if key doesn't exist
     * @return Integer value or default
     */
    static int getInt(const json &object, const std::string &key, int defaultValue = 0);

    /**
     * @brief Check if JSON object has key and it's not null
     * @param object JSON object
     * @param key Key to check
     * @return true if key exists and is not null
     */
    static bool hasKey(const json &object, const std::string &key);

    /**
     * @brief Create JSON object with timestamp
     * @return JSON object with current timestamp
     */
    static json createTimestampedObject();
};

}  // namespace RSM