#pragma once

#include "common/Result.h"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace SCXML {
// Forward declarations
namespace Runtime {
class RuntimeContext;
}

/**
 * Enumeration of standard SCXML platform event types
 */
enum class PlatformEventType {
    ERROR_EXECUTION,      // error.execution
    ERROR_COMMUNICATION,  // error.communication
    ERROR_PLATFORM,       // error.platform
    DONE_STATE,           // done.state.id
    DONE_INVOKE,          // done.invoke.id
    DONE_DATA             // done.data
};

/**
 * Structure representing a platform event with metadata
 */
struct PlatformEvent {
    PlatformEventType type;
    std::string eventName;
    std::string targetId;
    std::string errorMessage;
    std::string errorCode;
    nlohmann::json eventData;
    std::chrono::steady_clock::time_point timestamp;
    std::string sourceLocation;

    PlatformEvent() : timestamp(std::chrono::steady_clock::now()) {}

    PlatformEvent(PlatformEventType t, const std::string &name, const std::string &target = "",
                  const std::string &data = "")
        : type(t), eventName(name), targetId(target), timestamp(std::chrono::steady_clock::now()) {
        if (!data.empty()) {
            try {
                eventData = nlohmann::json::parse(data);
            } catch (const std::exception &) {
                eventData = data;  // Store as string if not valid JSON
            }
        }
    }
};

/**
 * Handler function type for platform events
 */
using PlatformEventHandler = std::function<SCXML::Common::Result<void>(const PlatformEvent &)>;

/**
 * Interface for platform event management in SCXML
 * Handles error.* and done.* events according to W3C specification
 */
class IPlatformEventManager {
public:
    virtual ~IPlatformEventManager() = default;

    /**
     * Register a handler for a specific platform event type
     */
    virtual SCXML::Common::Result<void> registerHandler(PlatformEventType type, PlatformEventHandler handler) = 0;

    /**
     * Fire a platform event
     */
    virtual SCXML::Common::Result<void> fireEvent(const PlatformEvent &event) = 0;

    /**
     * Fire an error event with specific error details
     */
    virtual SCXML::Common::Result<void> fireErrorEvent(const std::string &errorType, const std::string &errorMessage,
                                                       const std::string &errorCode = "",
                                                       const std::string &sourceLocation = "") = 0;

    /**
     * Fire a done event for state completion
     */
    virtual SCXML::Common::Result<void> fireDoneStateEvent(const std::string &stateId,
                                                           const std::string &eventData = "") = 0;

    /**
     * Fire a done event for invoke completion
     */
    virtual SCXML::Common::Result<void> fireDoneInvokeEvent(const std::string &invokeId,
                                                            const std::string &eventData = "") = 0;

    /**
     * Get event history for debugging
     */
    virtual std::vector<PlatformEvent> getEventHistory(size_t maxEvents = 100) const = 0;

    /**
     * Clear event history
     */
    virtual void clearHistory() = 0;

    /**
     * Enable/disable event logging
     */
    virtual void setLoggingEnabled(bool enabled) = 0;
};

/**
 * Concrete implementation of platform event manager
 */
class PlatformEventManager : public IPlatformEventManager {
private:
    mutable std::mutex handlerMutex_;
    mutable std::mutex historyMutex_;

    std::map<PlatformEventType, std::vector<PlatformEventHandler>> handlers_;
    std::vector<PlatformEvent> eventHistory_;
    bool loggingEnabled_;
    size_t maxHistorySize_;

public:
    explicit PlatformEventManager(size_t maxHistorySize = 1000);
    ~PlatformEventManager() override = default;

    SCXML::Common::Result<void> registerHandler(PlatformEventType type, PlatformEventHandler handler) override;

    SCXML::Common::Result<void> fireEvent(const PlatformEvent &event) override;

    SCXML::Common::Result<void> fireErrorEvent(const std::string &errorType, const std::string &errorMessage,
                                               const std::string &errorCode = "",
                                               const std::string &sourceLocation = "") override;

    SCXML::Common::Result<void> fireDoneStateEvent(const std::string &stateId,
                                                   const std::string &eventData = "") override;

    SCXML::Common::Result<void> fireDoneInvokeEvent(const std::string &invokeId,
                                                    const std::string &eventData = "") override;

    std::vector<PlatformEvent> getEventHistory(size_t maxEvents = 100) const override;

    void clearHistory() override;

    void setLoggingEnabled(bool enabled) override;

private:
    /**
     * Add event to history if logging is enabled
     */
    void addToHistory(const PlatformEvent &event);

    /**
     * Convert error type string to platform event type
     */
    PlatformEventType getErrorEventType(const std::string &errorType);

    /**
     * Generate standard SCXML event name from type and target
     */
    std::string generateEventName(PlatformEventType type, const std::string &target);
};

/**
 * Error event factory for common SCXML errors
 */
class ErrorEventFactory {
public:
    /**
     * Create execution error event
     */
    static PlatformEvent createExecutionError(const std::string &message, const std::string &location = "");

    /**
     * Create communication error event (for send/invoke operations)
     */
    static PlatformEvent createCommunicationError(const std::string &message, const std::string &target = "");

    /**
     * Create platform error event (for system-level errors)
     */
    static PlatformEvent createPlatformError(const std::string &message, const std::string &errorCode = "");

    /**
     * Create done.state event
     */
    static PlatformEvent createDoneStateEvent(const std::string &stateId, const nlohmann::json &data = {});

    /**
     * Create done.invoke event
     */
    static PlatformEvent createDoneInvokeEvent(const std::string &invokeId, const nlohmann::json &data = {});
};

/**
 * Utility class for integrating platform events with runtime context
 */
class PlatformEventIntegrator {
public:
    /**
     * Install standard error handlers in runtime context
     */
    static SCXML::Common::Result<void> installStandardHandlers(SCXML::Runtime::RuntimeContext &context,
                                                               IPlatformEventManager &eventManager);

    /**
     * Create error event handler that queues events in runtime context
     */
    static PlatformEventHandler createEventQueueHandler(SCXML::Runtime::RuntimeContext &context);

    /**
     * Create logging event handler
     */
    static PlatformEventHandler createLoggingHandler(const std::string &logPrefix = "PlatformEvent");
};

}  // namespace Runtime
}  // namespace SCXML