#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>

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

namespace SCXML {

/**
 * @brief JavaScript value types for SCXML data model
 */
using ScriptValue = std::variant<
    std::monostate,  // undefined/null
    bool,            // boolean
    int64_t,         // integer
    double,          // number
    std::string      // string
>;

/**
 * @brief JavaScript execution result
 */
struct SCXML_API ExecutionResult {
    bool success = false;
    ScriptValue value = std::monostate{};
    std::string errorMessage;

    bool isSuccess() const { return success; }
    bool isError() const { return !success; }

    template<typename T>
    T getValue() const {
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
    Event(const std::string& name, const std::string& type = "internal");
    virtual ~Event() = default;

    const std::string& getName() const { return name_; }
    const std::string& getType() const { return type_; }
    const std::string& getSendId() const { return sendId_; }
    const std::string& getOrigin() const { return origin_; }
    const std::string& getOriginType() const { return originType_; }
    const std::string& getInvokeId() const { return invokeId_; }

    void setSendId(const std::string& sendId) { sendId_ = sendId; }
    void setOrigin(const std::string& origin) { origin_ = origin; }
    void setOriginType(const std::string& originType) { originType_ = originType; }
    void setInvokeId(const std::string& invokeId) { invokeId_ = invokeId; }

    // Event data management
    bool hasData() const { return !dataString_.empty(); }
    void setData(const std::string& data) { dataString_ = data; }
    std::string getDataAsString() const { return dataString_; }

private:
    std::string name_;
    std::string type_;
    std::string sendId_;
    std::string origin_;
    std::string originType_;
    std::string invokeId_;
    std::string dataString_;
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

}  // namespace SCXML