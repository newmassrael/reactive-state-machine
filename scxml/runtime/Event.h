#pragma once

#include <string>
#include <memory>
#include <variant>
#include <unordered_map>

namespace SCXML::Runtime {

/**
 * @brief SCXML Event representation
 * Temporary implementation for JSEngine integration
 */
class Event {
public:
    using EventData = std::variant<std::monostate, bool, int64_t, double, std::string>;

    Event(const std::string& name, const std::string& type = "platform")
        : name_(name), type_(type) {}

    // Basic event properties
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

    // Event data
    bool hasData() const { return !std::holds_alternative<std::monostate>(data_); }
    const EventData& getData() const { return data_; }
    void setData(const EventData& data) { data_ = data; }

    std::string getDataAsString() const {
        return std::visit([](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return "\"" + v + "\"";  // JSON string
            } else if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::to_string(v);
            } else {
                return "null";
            }
        }, data_);
    }

private:
    std::string name_;
    std::string type_;
    std::string sendId_;
    std::string origin_;
    std::string originType_;
    std::string invokeId_;
    EventData data_ = std::monostate{};
};

}  // namespace SCXML::Runtime