#pragma once

#include <json/json.h>
#include <optional>
#include <string>

namespace RSM {

/**
 * @brief Centralized JSON processing utilities
 *
 * Eliminates duplicate JSON parsing/serialization logic across components.
 * Provides consistent error handling and formatting for all JSON operations.
 */
class JsonUtils {
public:
    /**
     * @brief Parse JSON string into Json::Value with error handling
     * @param jsonString Input JSON string
     * @param errorOut Optional error message output
     * @return Parsed Json::Value or nullopt on failure
     */
    static std::optional<Json::Value> parseJson(const std::string &jsonString, std::string *errorOut = nullptr);

    /**
     * @brief Serialize Json::Value to compact JSON string
     * @param value Json::Value to serialize
     * @return Compact JSON string
     */
    static std::string toCompactString(const Json::Value &value);

    /**
     * @brief Serialize Json::Value to pretty-formatted JSON string
     * @param value Json::Value to serialize
     * @return Pretty-formatted JSON string
     */
    static std::string toPrettyString(const Json::Value &value);

    /**
     * @brief Safely get string value from JSON object
     * @param object JSON object
     * @param key Key to lookup
     * @param defaultValue Default value if key doesn't exist
     * @return String value or default
     */
    static std::string getString(const Json::Value &object, const std::string &key,
                                 const std::string &defaultValue = "");

    /**
     * @brief Safely get integer value from JSON object
     * @param object JSON object
     * @param key Key to lookup
     * @param defaultValue Default value if key doesn't exist
     * @return Integer value or default
     */
    static int getInt(const Json::Value &object, const std::string &key, int defaultValue = 0);

    /**
     * @brief Check if JSON object has key and it's not null
     * @param object JSON object
     * @param key Key to check
     * @return true if key exists and is not null
     */
    static bool hasKey(const Json::Value &object, const std::string &key);

    /**
     * @brief Create JSON object with timestamp
     * @return JSON object with current timestamp
     */
    static Json::Value createTimestampedObject();

private:
    static Json::StreamWriterBuilder createCompactWriterBuilder();
    static Json::StreamWriterBuilder createPrettyWriterBuilder();
    static Json::CharReaderBuilder createReaderBuilder();
};

}  // namespace RSM