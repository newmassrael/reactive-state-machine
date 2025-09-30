#include "common/JsonUtils.h"
#include "common/Logger.h"
#include <chrono>

namespace RSM {

std::optional<json> JsonUtils::parseJson(const std::string &jsonString, std::string *errorOut) {
    if (jsonString.empty()) {
        if (errorOut) {
            *errorOut = "Empty JSON string";
        }
        return std::nullopt;
    }

    try {
        return json::parse(jsonString);
    } catch (const json::parse_error &e) {
        if (errorOut) {
            *errorOut = e.what();
        }
        LOG_DEBUG("JsonUtils: Failed to parse JSON: {}", e.what());
        return std::nullopt;
    }
}

std::string JsonUtils::toCompactString(const json &value) {
    return value.dump();
}

std::string JsonUtils::toPrettyString(const json &value) {
    return value.dump(2);  // indent with 2 spaces
}

std::string JsonUtils::getString(const json &object, const std::string &key, const std::string &defaultValue) {
    if (!object.is_object() || !object.contains(key)) {
        return defaultValue;
    }

    const auto &value = object[key];
    if (!value.is_string()) {
        return defaultValue;
    }

    return value.get<std::string>();
}

int JsonUtils::getInt(const json &object, const std::string &key, int defaultValue) {
    if (!object.is_object() || !object.contains(key)) {
        return defaultValue;
    }

    const auto &value = object[key];
    if (!value.is_number_integer()) {
        return defaultValue;
    }

    return value.get<int>();
}

bool JsonUtils::hasKey(const json &object, const std::string &key) {
    return object.is_object() && object.contains(key) && !object[key].is_null();
}

json JsonUtils::createTimestampedObject() {
    json object = json::object();
    object["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    return object;
}

}  // namespace RSM