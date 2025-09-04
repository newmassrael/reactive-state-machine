#include "runtime/StateMachineInspector.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <iomanip>
#include <regex>
#include <sstream>

namespace SCXML {
namespace Core {

std::string StateMachineInspector::EventTrace::toString() const {
    std::stringstream ss;
    ss << "[" << std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count() << "ms] ";
    ss << eventType << " event '" << eventName << "'";

    if (!source.empty()) {
        ss << " from " << source;
    }
    if (!target.empty()) {
        ss << " to " << target;
    }

    ss << " - " << (processed ? "PROCESSED" : "PENDING");

    if (!error.empty()) {
        ss << " (ERROR: " << error << ")";
    }

    return ss.str();
}

std::string StateMachineInspector::TransitionTrace::toString() const {
    std::stringstream ss;
    ss << "[" << std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count() << "ms] ";
    ss << "Transition: " << fromState << " -> " << toState;

    if (!event.empty()) {
        ss << " on event '" << event << "'";
    }
    if (!condition.empty()) {
        ss << " with condition '" << condition << "'";
    }

    ss << " - " << (successful ? "SUCCESS" : "FAILED");

    if (!error.empty()) {
        ss << " (ERROR: " << error << ")";
    }

    return ss.str();
}

std::string StateMachineInspector::ExecutionSnapshot::toJSON() const {
    std::stringstream json;
    json << "{";
    json << "\"timestamp\":"
         << std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count() << ",";
    json << "\"machineName\":\"" << machineName << "\",";
    json << "\"sessionId\":\"" << sessionId << "\",";

    // Active states
    json << "\"activeStates\":[";
    for (size_t i = 0; i < activeStates.size(); ++i) {
        if (i > 0) {
            json << ",";
        }
        json << "\"" << activeStates[i] << "\"";
    }
    json << "],";

    // Data model
    json << "\"dataModel\":{";
    bool first = true;
    for (const auto &pair : dataModel) {
        if (!first) {
            json << ",";
        }
        first = false;
        json << "\"" << pair.first << "\":\"" << pair.second << "\"";
    }
    json << "},";

    // Event queue
    json << "\"eventQueue\":[";
    for (size_t i = 0; i < eventQueue.size(); ++i) {
        if (i > 0) {
            json << ",";
        }
        json << "\"" << eventQueue[i] << "\"";
    }
    json << "]";

    json << "}";
    return json.str();
}

std::string StateMachineInspector::PerformanceMetrics::toString() const {
    std::stringstream ss;
    ss << "Performance Metrics:\n";
    ss << "  Total Events: " << totalEvents << "\n";
    ss << "  Total Transitions: " << totalTransitions << "\n";
    ss << "  Average Event Processing Time: " << averageEventProcessingTime << " μs\n";
    ss << "  Average Transition Time: " << averageTransitionTime << " μs\n";

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(lastUpdate - startTime);
    ss << "  Total Runtime: " << duration.count() << " seconds\n";

    if (duration.count() > 0) {
        ss << "  Events per Second: " << (totalEvents / duration.count()) << "\n";
        ss << "  Transitions per Second: " << (totalTransitions / duration.count()) << "\n";
    }

    return ss.str();
}

StateMachineInspector::StateMachineInspector()
    : eventTracingEnabled_(false), transitionTracingEnabled_(false), maxTraceEntries_(1000) {
    metrics_.startTime = std::chrono::steady_clock::now();
    metrics_.lastUpdate = metrics_.startTime;
}

SCXML::Common::Result<void> StateMachineInspector::initialize(SCXML::Runtime::RuntimeContext &context) {
    try {
        // Clear existing traces
        eventTraces_.clear();
        transitionTraces_.clear();

        // Reset metrics
        resetPerformanceMetrics();

        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Failed to initialize inspector: " + std::string(e.what()));
    }
}

void StateMachineInspector::setEventTracingEnabled(bool enabled) {
    eventTracingEnabled_ = enabled;
}

void StateMachineInspector::setTransitionTracingEnabled(bool enabled) {
    transitionTracingEnabled_ = enabled;
}

void StateMachineInspector::setMaxTraceEntries(size_t maxEntries) {
    maxTraceEntries_ = maxEntries;
}

void StateMachineInspector::recordEventTrace(const std::string &eventName, const std::string &eventType,
                                             const std::string &source, SCXML::Runtime::RuntimeContext &context) {
    if (!eventTracingEnabled_) {
        return;
    }

    EventTrace trace;
    trace.eventName = eventName;
    trace.eventType = eventType;
    trace.source = source;
    trace.timestamp = std::chrono::steady_clock::now();
    trace.processed = false;

    eventTraces_.push_back(trace);
    maintainTraceLimit(eventTraces_);

    // Update metrics
    metrics_.totalEvents++;
    updatePerformanceMetrics("event", std::chrono::microseconds(0));

    // Call inspection callback if set
    if (inspectionCallback_) {
        inspectionCallback_("event_trace", context);
    }
}

void StateMachineInspector::recordTransitionTrace(const std::string &fromState, const std::string &toState,
                                                  const std::string &event, const std::string &condition,
                                                  bool successful, const std::string &error,
                                                  SCXML::Runtime::RuntimeContext &context) {
    if (!transitionTracingEnabled_) {
        return;
    }

    TransitionTrace trace;
    trace.fromState = fromState;
    trace.toState = toState;
    trace.event = event;
    trace.condition = condition;
    trace.timestamp = std::chrono::steady_clock::now();
    trace.successful = successful;
    trace.error = error;

    transitionTraces_.push_back(trace);
    maintainTraceLimit(transitionTraces_);

    // Update metrics
    metrics_.totalTransitions++;
    updatePerformanceMetrics("transition", std::chrono::microseconds(0));

    // Call inspection callback if set
    if (inspectionCallback_) {
        inspectionCallback_("transition_trace", context);
    }
}

StateMachineInspector::ExecutionSnapshot
StateMachineInspector::getCurrentSnapshot(SCXML::Runtime::RuntimeContext &context) const {
    ExecutionSnapshot snapshot;
    snapshot.timestamp = std::chrono::steady_clock::now();
    snapshot.activeStates = getCurrentActiveStates(context);
    snapshot.eventQueue = getCurrentEventQueue(context);

    // Get data model variables
    auto dataModel = context.getDataModel().getAllVariables();
    for (const auto &pair : dataModel) {
        snapshot.dataModel[pair.first] = pair.second;
    }

    // Get machine name and session ID
    if (context.getDataModel().hasVariable("_name")) {
        auto nameResult = context.getDataModel().getVariable("_name");
        if (nameResult.isSuccess()) {
            snapshot.machineName = nameResult.getValue();
        }
    }

    if (context.getDataModel().hasVariable("_sessionid")) {
        auto sessionResult = context.getDataModel().getVariable("_sessionid");
        if (sessionResult.isSuccess()) {
            snapshot.sessionId = sessionResult.getValue();
        }
    }

    return snapshot;
}

std::vector<StateMachineInspector::EventTrace> StateMachineInspector::getEventTraceHistory(size_t maxEntries) const {
    if (maxEntries == 0 || maxEntries >= eventTraces_.size()) {
        return eventTraces_;
    }

    return std::vector<EventTrace>(eventTraces_.end() - maxEntries, eventTraces_.end());
}

std::vector<StateMachineInspector::TransitionTrace>
StateMachineInspector::getTransitionTraceHistory(size_t maxEntries) const {
    if (maxEntries == 0 || maxEntries >= transitionTraces_.size()) {
        return transitionTraces_;
    }

    return std::vector<TransitionTrace>(transitionTraces_.end() - maxEntries, transitionTraces_.end());
}

StateMachineInspector::PerformanceMetrics StateMachineInspector::getPerformanceMetrics() const {
    metrics_.lastUpdate = std::chrono::steady_clock::now();
    return metrics_;
}

std::string StateMachineInspector::generateDebugReport(SCXML::Runtime::RuntimeContext &context) const {
    std::stringstream report;
    auto snapshot = getCurrentSnapshot(context);

    report << "=== SCXML State Machine Debug Report ===\n\n";

    // Basic information
    report << "Machine Name: " << snapshot.machineName << "\n";
    report << "Session ID: " << snapshot.sessionId << "\n";
    report << "Timestamp: " << formatTimestamp(snapshot.timestamp) << "\n\n";

    // Active states
    report << "Active States (" << snapshot.activeStates.size() << "):\n";
    for (const auto &state : snapshot.activeStates) {
        report << "  - " << state << "\n";
    }
    report << "\n";

    // Event queue
    report << "Event Queue (" << snapshot.eventQueue.size() << "):\n";
    for (size_t i = 0; i < snapshot.eventQueue.size(); ++i) {
        report << "  " << (i + 1) << ". " << snapshot.eventQueue[i] << "\n";
    }
    report << "\n";

    // Data model
    report << "Data Model (" << snapshot.dataModel.size() << " variables):\n";
    for (const auto &pair : snapshot.dataModel) {
        report << "  " << pair.first << " = " << pair.second << "\n";
    }
    report << "\n";

    // Performance metrics
    report << getPerformanceMetrics().toString() << "\n";

    // Recent event traces
    report << "Recent Event Traces (last 10):\n";
    auto recentEvents = getEventTraceHistory(10);
    for (const auto &trace : recentEvents) {
        report << "  " << trace.toString() << "\n";
    }
    report << "\n";

    // Recent transition traces
    report << "Recent Transition Traces (last 10):\n";
    auto recentTransitions = getTransitionTraceHistory(10);
    for (const auto &trace : recentTransitions) {
        report << "  " << trace.toString() << "\n";
    }
    report << "\n";

    // State machine validation
    auto validationResult = validateStateMachine(context);
    if (validationResult.isSuccess()) {
        const auto &issues = validationResult.getValue();
        if (issues.empty()) {
            report << "State Machine Validation: PASSED\n";
        } else {
            report << "State Machine Validation Issues (" << issues.size() << "):\n";
            for (const auto &issue : issues) {
                report << "  WARNING: " << issue << "\n";
            }
        }
    } else {
        report << "State Machine Validation: FAILED\n";
        report << "  ERROR: " << validationResult.getError() << "\n";
    }

    report << "\n=== End Debug Report ===\n";
    return report.str();
}

SCXML::Common::Result<std::vector<std::string>>
StateMachineInspector::validateStateMachine(SCXML::Runtime::RuntimeContext &context) const {
    std::vector<std::string> issues;

    try {
        // Get current snapshot
        auto snapshot = getCurrentSnapshot(context);

        // Check if there are active states
        if (snapshot.activeStates.empty()) {
            issues.push_back("No active states found");
        }

        // Check for required system variables
        if (snapshot.sessionId.empty()) {
            issues.push_back("Missing _sessionid system variable");
        }

        if (snapshot.machineName.empty()) {
            issues.push_back("Missing _name system variable");
        }

        // Check event queue size
        if (snapshot.eventQueue.size() > 100) {
            issues.push_back("Event queue is very large (" + std::to_string(snapshot.eventQueue.size()) + " events)");
        }

        // Check for circular references in data model (simplified check)
        for (const auto &pair : snapshot.dataModel) {
            if (pair.second.find(pair.first) != std::string::npos) {
                issues.push_back("Possible circular reference in variable: " + pair.first);
            }
        }

        return SCXML::Common::Result<std::vector<std::string>>::success(issues);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::vector<std::string>>::failure("Exception during validation: " +
                                                                        std::string(e.what()));
    }
}

std::map<std::string, std::string>
StateMachineInspector::getStateHierarchy(SCXML::Runtime::RuntimeContext &context) const {
    // Simplified implementation - would need actual state tree access
    std::map<std::string, std::string> hierarchy;
    auto activeStates = getCurrentActiveStates(context);

    for (const auto &state : activeStates) {
        // Simple heuristic: if state contains dot, parent is prefix
        size_t lastDot = state.find_last_of('.');
        if (lastDot != std::string::npos) {
            std::string parent = state.substr(0, lastDot);
            hierarchy[state] = parent;
        } else {
            hierarchy[state] = "";  // Root state
        }
    }

    return hierarchy;
}

std::vector<std::string> StateMachineInspector::getReachableStates(SCXML::Runtime::RuntimeContext &context) const {
    // Simplified implementation - would need transition analysis
    auto activeStates = getCurrentActiveStates(context);
    std::vector<std::string> reachableStates = activeStates;

    // Add some common reachable states (example)
    reachableStates.push_back("final");
    reachableStates.push_back("error");

    return reachableStates;
}

std::map<std::string, std::vector<std::string>>
StateMachineInspector::getPossibleTransitions(SCXML::Runtime::RuntimeContext &context) const {
    // Simplified implementation - would need transition analysis
    std::map<std::string, std::vector<std::string>> transitions;

    // Example transitions
    transitions["start"] = {"working", "error"};
    transitions["stop"] = {"idle", "final"};
    transitions["reset"] = {"initial"};

    return transitions;
}

void StateMachineInspector::clearTraceHistory() {
    eventTraces_.clear();
    transitionTraces_.clear();
}

void StateMachineInspector::resetPerformanceMetrics() {
    metrics_.totalEvents = 0;
    metrics_.totalTransitions = 0;
    metrics_.averageTransitionTime = 0;
    metrics_.averageEventProcessingTime = 0;
    metrics_.startTime = std::chrono::steady_clock::now();
    metrics_.lastUpdate = metrics_.startTime;
}

std::string StateMachineInspector::exportTraceDataToJSON() const {
    std::stringstream json;
    json << "{";

    // Event traces
    json << "\"eventTraces\":[";
    for (size_t i = 0; i < eventTraces_.size(); ++i) {
        if (i > 0) {
            json << ",";
        }
        const auto &trace = eventTraces_[i];
        json << "{";
        json << "\"eventName\":\"" << trace.eventName << "\",";
        json << "\"eventType\":\"" << trace.eventType << "\",";
        json << "\"source\":\"" << trace.source << "\",";
        json << "\"timestamp\":"
             << std::chrono::duration_cast<std::chrono::milliseconds>(trace.timestamp.time_since_epoch()).count()
             << ",";
        json << "\"processed\":" << (trace.processed ? "true" : "false");
        if (!trace.error.empty()) {
            json << ",\"error\":\"" << trace.error << "\"";
        }
        json << "}";
    }
    json << "],";

    // Transition traces
    json << "\"transitionTraces\":[";
    for (size_t i = 0; i < transitionTraces_.size(); ++i) {
        if (i > 0) {
            json << ",";
        }
        const auto &trace = transitionTraces_[i];
        json << "{";
        json << "\"fromState\":\"" << trace.fromState << "\",";
        json << "\"toState\":\"" << trace.toState << "\",";
        json << "\"event\":\"" << trace.event << "\",";
        json << "\"condition\":\"" << trace.condition << "\",";
        json << "\"timestamp\":"
             << std::chrono::duration_cast<std::chrono::milliseconds>(trace.timestamp.time_since_epoch()).count()
             << ",";
        json << "\"successful\":" << (trace.successful ? "true" : "false");
        if (!trace.error.empty()) {
            json << ",\"error\":\"" << trace.error << "\"";
        }
        json << "}";
    }
    json << "],";

    // Performance metrics
    json << "\"performanceMetrics\":{";
    json << "\"totalEvents\":" << metrics_.totalEvents << ",";
    json << "\"totalTransitions\":" << metrics_.totalTransitions << ",";
    json << "\"averageEventProcessingTime\":" << metrics_.averageEventProcessingTime << ",";
    json << "\"averageTransitionTime\":" << metrics_.averageTransitionTime;
    json << "}";

    json << "}";
    return json.str();
}

void StateMachineInspector::setInspectionCallback(
    std::function<void(const std::string &, SCXML::Runtime::RuntimeContext &)> callback) {
    inspectionCallback_ = callback;
}

template <typename T> void StateMachineInspector::maintainTraceLimit(std::vector<T> &traces) {
    if (maxTraceEntries_ > 0 && traces.size() > maxTraceEntries_) {
        size_t toRemove = traces.size() - maxTraceEntries_;
        traces.erase(traces.begin(), traces.begin() + toRemove);
    }
}

void StateMachineInspector::updatePerformanceMetrics(const std::string &operationType,
                                                     std::chrono::microseconds duration) const {
    metrics_.lastUpdate = std::chrono::steady_clock::now();

    if (operationType == "event") {
        // Update average event processing time (simplified)
        metrics_.averageEventProcessingTime = (metrics_.averageEventProcessingTime + duration.count()) / 2;
    } else if (operationType == "transition") {
        // Update average transition time (simplified)
        metrics_.averageTransitionTime = (metrics_.averageTransitionTime + duration.count()) / 2;
    }
}

std::string StateMachineInspector::formatTimestamp(std::chrono::steady_clock::time_point timestamp) const {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

    std::stringstream ss;
    ss << ms << "ms";
    return ss.str();
}

std::vector<std::string> StateMachineInspector::getCurrentActiveStates(SCXML::Runtime::RuntimeContext &context) const {
    // Get actual active states from runtime context
    std::vector<std::string> activeStates = context.getActiveStates();
    return activeStates;
}

std::vector<std::string> StateMachineInspector::getCurrentEventQueue(SCXML::Runtime::RuntimeContext &context) const {
    // Simplified implementation - would need access to actual event queue
    std::vector<std::string> events;
    return events;
}

}  // namespace Core
}  // namespace SCXML