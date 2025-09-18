#pragma once

#include <string>
#include <memory>
#include <optional>

namespace SCXML::Runtime {

/**
 * @brief SCXML Event representation
 * Simplified implementation using JSON-only data storage
 */
class Event {
public:
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

    // Event data - simplified JSON-only API
    bool hasData() const { 
        return rawJsonData_.has_value();
    }

    void setRawJsonData(const std::string& json) {
        rawJsonData_ = json;
    }

    std::string getDataAsString() const {
        return rawJsonData_.value_or("null");
    }

private:
    std::string name_;
    std::string type_;
    std::string sendId_;
    std::string origin_;
    std::string originType_;
    std::string invokeId_;
    mutable std::optional<std::string> rawJsonData_;  // Raw JSON storage
};

}  // namespace SCXML::Runtime