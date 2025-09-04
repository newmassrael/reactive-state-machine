#pragma once

#include "common/SCXMLCommon.h"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SCXML {
// Forward declarations
namespace Runtime {
class RuntimeContext;
}

namespace Core {

/**
 * @brief State machine introspection and debugging system
 *
 * This class provides comprehensive debugging, inspection, and analysis
 * capabilities for SCXML state machines at runtime.
 */
class StateMachineInspector {
public:
    /**
     * @brief Event trace entry containing event processing information
     */
    struct EventTrace {
        std::string eventName;
        std::string eventType;
        std::string source;
        std::string target;
        std::chrono::steady_clock::time_point timestamp;
        std::string data;
        bool processed;
        std::string error;

        std::string toString() const;
    };

    /**
     * @brief State transition trace entry
     */
    struct TransitionTrace {
        std::string fromState;
        std::string toState;
        std::string event;
        std::string condition;
        std::chrono::steady_clock::time_point timestamp;
        bool successful;
        std::string error;

        std::string toString() const;
    };

    /**
     * @brief State machine execution snapshot
     */
    struct ExecutionSnapshot {
        std::vector<std::string> activeStates;
        std::map<std::string, std::string> dataModel;
        std::vector<std::string> eventQueue;
        std::chrono::steady_clock::time_point timestamp;
        std::string machineName;
        std::string sessionId;

        std::string toJSON() const;
    };

    /**
     * @brief Performance metrics for state machine execution
     */
    struct PerformanceMetrics {
        size_t totalEvents = 0;
        size_t totalTransitions = 0;
        size_t averageTransitionTime = 0;       // microseconds
        size_t averageEventProcessingTime = 0;  // microseconds
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastUpdate;

        std::string toString() const;
    };

    /**
     * @brief Constructor
     */
    StateMachineInspector();

    /**
     * @brief Destructor
     */
    virtual ~StateMachineInspector() = default;

    /**
     * @brief Initialize inspector with runtime context
     * @param context Runtime context to inspect
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> initialize(SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Enable or disable event tracing
     * @param enabled True to enable tracing
     */
    void setEventTracingEnabled(bool enabled);

    /**
     * @brief Enable or disable transition tracing
     * @param enabled True to enable tracing
     */
    void setTransitionTracingEnabled(bool enabled);

    /**
     * @brief Set maximum number of trace entries to keep
     * @param maxEntries Maximum number of entries (0 = unlimited)
     */
    void setMaxTraceEntries(size_t maxEntries);

    /**
     * @brief Record an event trace entry
     * @param event Event information
     * @param context Runtime context
     */
    void recordEventTrace(const std::string &eventName, const std::string &eventType, const std::string &source,
                          SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Record a transition trace entry
     * @param fromState Source state
     * @param toState Target state
     * @param event Triggering event
     * @param condition Transition condition
     * @param successful Whether transition was successful
     * @param error Error message if failed
     * @param context Runtime context
     */
    void recordTransitionTrace(const std::string &fromState, const std::string &toState, const std::string &event,
                               const std::string &condition, bool successful, const std::string &error,
                               SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get current execution snapshot
     * @param context Runtime context
     * @return Execution snapshot
     */
    ExecutionSnapshot getCurrentSnapshot(SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Get event trace history
     * @param maxEntries Maximum number of entries to return (0 = all)
     * @return Vector of event trace entries
     */
    std::vector<EventTrace> getEventTraceHistory(size_t maxEntries = 0) const;

    /**
     * @brief Get transition trace history
     * @param maxEntries Maximum number of entries to return (0 = all)
     * @return Vector of transition trace entries
     */
    std::vector<TransitionTrace> getTransitionTraceHistory(size_t maxEntries = 0) const;

    /**
     * @brief Get current performance metrics
     * @return Performance metrics
     */
    PerformanceMetrics getPerformanceMetrics() const;

    /**
     * @brief Generate comprehensive debug report
     * @param context Runtime context
     * @return Debug report as formatted string
     */
    std::string generateDebugReport(SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Validate current state machine state
     * @param context Runtime context
     * @return Validation result with any issues found
     */
    SCXML::Common::Result<std::vector<std::string>> validateStateMachine(SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Get state hierarchy information
     * @param context Runtime context
     * @return Map of state ID to parent state ID
     */
    std::map<std::string, std::string> getStateHierarchy(SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Get all reachable states from current configuration
     * @param context Runtime context
     * @return Vector of reachable state IDs
     */
    std::vector<std::string> getReachableStates(SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Get all possible transitions from current configuration
     * @param context Runtime context
     * @return Map of event name to possible target states
     */
    std::map<std::string, std::vector<std::string>>
    getPossibleTransitions(SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Clear all trace history
     */
    void clearTraceHistory();

    /**
     * @brief Reset performance metrics
     */
    void resetPerformanceMetrics();

    /**
     * @brief Export trace data to JSON
     * @return JSON string containing all trace data
     */
    std::string exportTraceDataToJSON() const;

    /**
     * @brief Set custom inspection callback
     * @param callback Function to call on inspection events
     */
    void setInspectionCallback(std::function<void(const std::string &, SCXML::Runtime::RuntimeContext &)> callback);

private:
    bool eventTracingEnabled_;       ///< Event tracing enabled flag
    bool transitionTracingEnabled_;  ///< Transition tracing enabled flag
    size_t maxTraceEntries_;         ///< Maximum trace entries to keep

    std::vector<EventTrace> eventTraces_;            ///< Event trace history
    std::vector<TransitionTrace> transitionTraces_;  ///< Transition trace history

    mutable PerformanceMetrics metrics_;  ///< Performance metrics

    std::function<void(const std::string &, SCXML::Runtime::RuntimeContext &)>
        inspectionCallback_;  ///< Custom callback

    /**
     * @brief Maintain trace entry limits
     * @param traces Trace vector to maintain
     */
    template <typename T> void maintainTraceLimit(std::vector<T> &traces);

    /**
     * @brief Update performance metrics
     * @param operationType Type of operation (event, transition)
     * @param duration Operation duration in microseconds
     */
    void updatePerformanceMetrics(const std::string &operationType, std::chrono::microseconds duration) const;

    /**
     * @brief Format timestamp as string
     * @param timestamp Timestamp to format
     * @return Formatted timestamp string
     */
    std::string formatTimestamp(std::chrono::steady_clock::time_point timestamp) const;

    /**
     * @brief Get current active states as string vector
     * @param context Runtime context
     * @return Vector of active state names
     */
    std::vector<std::string> getCurrentActiveStates(SCXML::Runtime::RuntimeContext &context) const;

    /**
     * @brief Get current event queue as string vector
     * @param context Runtime context
     * @return Vector of queued event names
     */
    std::vector<std::string> getCurrentEventQueue(SCXML::Runtime::RuntimeContext &context) const;
};

} // namespace Model
}  // namespace SCXML