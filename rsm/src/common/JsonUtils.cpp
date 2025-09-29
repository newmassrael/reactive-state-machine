#include "common/JsonUtils.h"
#include "common/Logger.h"
#include <chrono>
#include <sstream>

namespace RSM {

std::optional<Json::Value> JsonUtils::parseJson(const std::string &jsonString, std::string *errorOut) {
    if (jsonString.empty()) {
        if (errorOut) {
            *errorOut = "Empty JSON string";
        }
        return std::nullopt;
    }

    Json::Value root;
    Json::CharReaderBuilder readerBuilder = createReaderBuilder();
    std::string parseErrors;
    std::istringstream jsonStream(jsonString);

    bool success = Json::parseFromStream(readerBuilder, jsonStream, &root, &parseErrors);
    if (!success) {
        if (errorOut) {
            *errorOut = parseErrors;
        }
        LOG_DEBUG("JsonUtils: Failed to parse JSON: {}", parseErrors);
        return std::nullopt;
    }

    return root;
}

std::string JsonUtils::toCompactString(const Json::Value &value) {
    Json::StreamWriterBuilder writerBuilder = createCompactWriterBuilder();
    return Json::writeString(writerBuilder, value);
}

std::string JsonUtils::toPrettyString(const Json::Value &value) {
    Json::StreamWriterBuilder writerBuilder = createPrettyWriterBuilder();
    return Json::writeString(writerBuilder, value);
}

std::string JsonUtils::getString(const Json::Value &object, const std::string &key, const std::string &defaultValue) {
    if (!object.isObject() || !object.isMember(key)) {
        return defaultValue;
    }

    const Json::Value &value = object[key];
    if (!value.isString()) {
        return defaultValue;
    }

    return value.asString();
}

int JsonUtils::getInt(const Json::Value &object, const std::string &key, int defaultValue) {
    if (!object.isObject() || !object.isMember(key)) {
        return defaultValue;
    }

    const Json::Value &value = object[key];
    if (!value.isInt()) {
        return defaultValue;
    }

    return value.asInt();
}

bool JsonUtils::hasKey(const Json::Value &object, const std::string &key) {
    return object.isObject() && object.isMember(key) && !object[key].isNull();
}

Json::Value JsonUtils::createTimestampedObject() {
    Json::Value object(Json::objectValue);
    object["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    return object;
}

Json::StreamWriterBuilder JsonUtils::createCompactWriterBuilder() {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";  // Compact JSON
    return builder;
}

Json::StreamWriterBuilder JsonUtils::createPrettyWriterBuilder() {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";  // Pretty-formatted JSON
    return builder;
}

Json::CharReaderBuilder JsonUtils::createReaderBuilder() {
    Json::CharReaderBuilder builder;
    // Use default settings for robust parsing
    return builder;
}

}  // namespace RSM