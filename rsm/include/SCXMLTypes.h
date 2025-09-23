#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// Platform-specific API export macros
#ifdef _WIN32
#ifdef SCXML_ENGINE_EXPORTS
#define SCXML_API __declspec(dllexport)
#else
#define SCXML_API __declspec(dllimport)
#endif
#else
#ifdef SCXML_ENGINE_EXPORTS
#define SCXML_API __attribute__((visibility("default")))
#else
#define SCXML_API
#endif
#endif

/**
 * @brief Forward declarations for complex types
 */
struct ScriptArray;
struct ScriptObject;

/**
 * @brief JavaScript null type for SCXML W3C compliance
 */
struct ScriptNull {};

/**
 * @brief JavaScript undefined type for SCXML W3C compliance
 */
struct ScriptUndefined {};

/**
 * @brief JavaScript value types for SCXML data model
 *
 * SCXML W3C Compliance: null and undefined are now distinct types
 * - ScriptUndefined: typeof returns "undefined"
 * - ScriptNull: typeof returns "object"
 */
using ScriptValue = std::variant<ScriptUndefined,               // undefined
                                 ScriptNull,                    // null
                                 bool,                          // boolean
                                 int64_t,                       // integer
                                 double,                        // number
                                 std::string,                   // string
                                 std::shared_ptr<ScriptArray>,  // array
                                 std::shared_ptr<ScriptObject>  // object
                                 >;

/**
 * @brief JavaScript array type
 */
struct ScriptArray {
    std::vector<ScriptValue> elements;

    ScriptArray() = default;

    ScriptArray(std::initializer_list<ScriptValue> init) : elements(init) {}

    ScriptArray(const std::vector<ScriptValue> &elems) : elements(elems) {}
};

/**
 * @brief JavaScript object type
 */
struct ScriptObject {
    std::unordered_map<std::string, ScriptValue> properties;

    ScriptObject() = default;

    ScriptObject(std::initializer_list<std::pair<const std::string, ScriptValue>> init) : properties(init) {}

    ScriptObject(const std::unordered_map<std::string, ScriptValue> &props) : properties(props) {}
};

namespace RSM {

/**
 * @brief JavaScript execution result
 */
struct SCXML_API ExecutionResult {
    bool success = false;
    ScriptValue value = ScriptUndefined{};
    std::string errorMessage;

    bool isSuccess() const {
        return success;
    }

    bool isError() const {
        return !success;
    }

    template <typename T> T getValue() const {
        if (std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }
        return T{};
    }

    std::string getValueAsString() const;
};

/**
 * @brief SCXML Event representation
 */
class SCXML_API Event {
public:
    Event(const std::string &name, const std::string &type = "internal");
    virtual ~Event() = default;

    const std::string &getName() const {
        return name_;
    }

    const std::string &getType() const {
        return type_;
    }

    const std::string &getSendId() const {
        return sendId_;
    }

    const std::string &getOrigin() const {
        return origin_;
    }

    const std::string &getOriginType() const {
        return originType_;
    }

    const std::string &getInvokeId() const {
        return invokeId_;
    }

    void setSendId(const std::string &sendId) {
        sendId_ = sendId;
    }

    void setOrigin(const std::string &origin) {
        origin_ = origin;
    }

    void setOriginType(const std::string &originType) {
        originType_ = originType;
    }

    void setInvokeId(const std::string &invokeId) {
        invokeId_ = invokeId;
    }

    // Event data management - Enhanced version with raw JSON support
    bool hasData() const {
        return rawJsonData_.has_value() || !dataString_.empty();
    }

    void setData(const std::string &data) {
        dataString_ = data;
    }

    void setDataFromString(const std::string &data) {
        dataString_ = data;
    }

    void setRawJsonData(const std::string &json) {
        rawJsonData_ = json;
    }

    std::string getDataAsString() const {
        if (rawJsonData_.has_value()) {
            return rawJsonData_.value();
        }
        return dataString_.empty() ? "null" : dataString_;
    }

private:
    std::string name_;
    std::string type_;
    std::string sendId_;
    std::string origin_;
    std::string originType_;
    std::string invokeId_;
    std::string dataString_;
    mutable std::optional<std::string> rawJsonData_;  // Raw JSON storage
};

/**
 * @brief Session information
 */
struct SCXML_API SessionInfo {
    std::string sessionId;
    std::string parentSessionId;
    std::string sessionName;
    std::vector<std::string> ioProcessors;
    bool isActive = false;
};

}  // namespace RSM