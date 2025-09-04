#include "events/Event.h"
#include <sstream>

namespace SCXML {
namespace Events {

Event::Event(const std::string &name, Type type) : name_(name), type_(type), data_(std::monostate{}) {}

Event::Event(const std::string &name, const EventData &data, Type type) : name_(name), type_(type), data_(data) {}

std::string Event::getDataAsString() const {
    if (std::holds_alternative<std::monostate>(data_)) {
        return "";
    } else if (std::holds_alternative<std::string>(data_)) {
        return std::get<std::string>(data_);
    } else if (std::holds_alternative<int>(data_)) {
        return std::to_string(std::get<int>(data_));
    } else if (std::holds_alternative<double>(data_)) {
        return std::to_string(std::get<double>(data_));
    } else if (std::holds_alternative<bool>(data_)) {
        return std::get<bool>(data_) ? "true" : "false";
    }
    return "";
}

bool Event::hasData() const {
    return !std::holds_alternative<std::monostate>(data_);
}

void Event::setOrigin(const std::string &origin, const std::string &originType) {
    origin_ = origin;
    originType_ = originType;
}

std::string Event::toString() const {
    std::ostringstream oss;
    oss << "Event{name='" << name_ << "'";

    if (hasData()) {
        oss << ", data='" << getDataAsString() << "'";
    }

    oss << ", type=";
    switch (type_) {
    case Type::PLATFORM:
        oss << "PLATFORM";
        break;
    case Type::INTERNAL:
        oss << "INTERNAL";
        break;
    case Type::EXTERNAL:
        oss << "EXTERNAL";
        break;
    }

    if (!origin_.empty()) {
        oss << ", origin='" << origin_ << "'";
    }

    if (!invokeId_.empty()) {
        oss << ", invokeId='" << invokeId_ << "'";
    }

    oss << "}";
    return oss.str();
}

}  // namespace Events
}  // namespace SCXML