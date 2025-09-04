#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>

namespace SCXML {
namespace Events {

/**
 * @brief SCXML Event implementation
 *
 * Represents events in SCXML state machines according to W3C specification
 */
class Event {
public:
    using EventData = std::variant<std::monostate, std::string, int, double, bool>;

    /**
     * @brief Event types as defined by SCXML specification
     */
    enum class Type {
        PLATFORM,  // Platform-generated events
        INTERNAL,  // Internal events (raised by state machine)
        EXTERNAL   // External events (from outside)
    };

    /**
     * @brief Constructor
     */
    explicit Event(const std::string &name, Type type = Type::EXTERNAL);

    /**
     * @brief Constructor with data
     */
    Event(const std::string &name, const EventData &data, Type type = Type::EXTERNAL);

    /**
     * @brief Destructor
     */
    virtual ~Event() = default;

    // ====== Accessors ======

    /**
     * @brief Get event name
     */
    const std::string &getName() const {
        return name_;
    }

    /**
     * @brief Get event type
     */
    Type getType() const {
        return type_;
    }

    /**
     * @brief Get event data
     */
    const EventData &getData() const {
        return data_;
    }

    /**
     * @brief Get event data as string
     */
    std::string getDataAsString() const;

    /**
     * @brief Check if event has data
     */
    bool hasData() const;

    /**
     * @brief Get event origin (sender information)
     */
    const std::string &getOrigin() const {
        return origin_;
    }

    /**
     * @brief Get event origin type
     */
    const std::string &getOriginType() const {
        return originType_;
    }

    /**
     * @brief Get invoke ID (if sent by invoked process)
     */
    const std::string &getInvokeId() const {
        return invokeId_;
    }

    // ====== Mutators ======

    /**
     * @brief Set event data
     */
    void setData(const EventData &data) {
        data_ = data;
    }

    /**
     * @brief Set event origin information
     */
    void setOrigin(const std::string &origin, const std::string &originType = "");

    /**
     * @brief Set invoke ID
     */
    void setInvokeId(const std::string &invokeId) {
        invokeId_ = invokeId;
    }

    // ====== Utility Methods ======

    /**
     * @brief Convert event to string representation
     */
    std::string toString() const;

    /**
     * @brief Check if this is a platform event
     */
    bool isPlatform() const {
        return type_ == Type::PLATFORM;
    }

    /**
     * @brief Check if this is an internal event
     */
    bool isInternal() const {
        return type_ == Type::INTERNAL;
    }

    /**
     * @brief Check if this is an external event
     */
    bool isExternal() const {
        return type_ == Type::EXTERNAL;
    }

private:
    std::string name_;
    Type type_;
    EventData data_;
    std::string origin_;
    std::string originType_;
    std::string invokeId_;
};

// Type aliases for convenience
using EventPtr = std::shared_ptr<Event>;
using EventData = Event::EventData;
using EventType = Event::Type;

// ====== Utility Functions ======

/**
 * @brief Create an event with name and type
 */
inline EventPtr makeEvent(const std::string &name, Event::Type type = Event::Type::INTERNAL) {
    return std::make_shared<Event>(name, type);
}

/**
 * @brief Create an event with name, type, and data
 */
inline EventPtr makeEvent(const std::string &name, Event::Type type, const EventData &data) {
    auto event = std::make_shared<Event>(name, data, type);
    return event;
}

/**
 * @brief Create an event with name, type, data, and sendId
 */
inline EventPtr makeEvent(const std::string &name, Event::Type type, const EventData &data,
                          const std::string & /* sendId */) {
    auto event = std::make_shared<Event>(name, data, type);
    // Note: sendId would typically be handled by the IOProcessor
    (void)0;  // Suppress unused parameter warning
    return event;
}

}  // namespace Events
}  // namespace SCXML