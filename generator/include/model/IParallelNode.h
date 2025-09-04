#pragma once
#include "IExecutionContext.h"
#include "common/SCXMLCommon.h"
#include <chrono>
#include <sstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace SCXML {
namespace Model {

// Forward declarations

/**
 * @brief Interface for SCXML <parallel> element implementation
 *
 * Parallel states allow multiple child states to be active simultaneously,
 * providing concurrent execution capabilities within a state machine.
 */
class IParallelNode {
public:
    virtual ~IParallelNode() = default;

    /**
     * @brief Get the parallel state ID
     * @return Unique identifier for this parallel state
     */
    virtual const std::string &getId() const = 0;

    /**
     * @brief Set the parallel state ID
     * @param id Unique identifier
     */
    virtual void setId(const std::string &id) = 0;

    /**
     * @brief Add a child state to this parallel state
     * @param childStateId Child state identifier
     */
    virtual void addChildState(const std::string &childStateId) = 0;

    /**
     * @brief Remove a child state from this parallel state
     * @param childStateId Child state identifier
     * @return True if child was found and removed
     */
    virtual bool removeChildState(const std::string &childStateId) = 0;

    /**
     * @brief Get all child state IDs
     * @return Vector of child state identifiers
     */
    virtual std::vector<std::string> getChildStates() const = 0;

    /**
     * @brief Check if a state is a child of this parallel state
     * @param stateId State identifier to check
     * @return True if state is a child
     */
    virtual bool isChildState(const std::string &stateId) const = 0;

    /**
     * @brief Enter the parallel state (activate all child states)
     * @param context Execution context
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> enter(IExecutionContext &context) = 0;

    /**
     * @brief Exit the parallel state (deactivate all child states)
     * @param context Execution context
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> exit(IExecutionContext &context) = 0;

    /**
     * @brief Check if the parallel state is complete (all children in final states)
     * @param context Execution context
     * @return True if parallel state is complete
     */
    virtual bool isComplete(IExecutionContext &context) const = 0;

    /**
     * @brief Get currently active child states
     * @param context Execution context
     * @return Set of active child state IDs
     */
    virtual std::set<std::string> getActiveChildStates(IExecutionContext &context) const = 0;

    /**
     * @brief Process an event in parallel across all active child states
     * @param context Execution context
     * @param eventName Event to process
     * @return Result containing states that handled the event
     */
    virtual SCXML::Common::Result<std::set<std::string>>
    processEventInParallel(IExecutionContext &context, const std::string &eventName) = 0;

    /**
     * @brief Validate parallel state configuration
     * @return List of validation errors, empty if valid
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Clone this parallel node
     * @return Deep copy of this parallel node
     */
    virtual std::shared_ptr<IParallelNode> clone() const = 0;

    /**
     * @brief Get completion condition for this parallel state
     * @return Completion condition expression
     */
    virtual const std::string &getCompletionCondition() const = 0;

    /**
     * @brief Set completion condition
     * @param condition Expression that determines when parallel state is complete
     */
    virtual void setCompletionCondition(const std::string &condition) = 0;
};

/**
 * @brief Interface for Parallel State Manager
 *
 * Manages the execution of parallel states and coordinates concurrent
 * state transitions and event processing.
 */
class IParallelStateManager {
public:
    virtual ~IParallelStateManager() = default;

    /**
     * @brief Register a parallel state with the manager
     * @param parallelState Parallel state to register
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> registerParallelState(std::shared_ptr<IParallelNode> parallelState) = 0;

    /**
     * @brief Enter all registered parallel states that should be active
     * @param context Execution context
     * @param statesToEnter Set of states to enter (may include parallel states)
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> enterParallelStates(IExecutionContext &context,
                                                            const std::set<std::string> &statesToEnter) = 0;

    /**
     * @brief Exit all registered parallel states that should be inactive
     * @param context Execution context
     * @param statesToExit Set of states to exit (may include parallel states)
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> exitParallelStates(IExecutionContext &context,
                                                           const std::set<std::string> &statesToExit) = 0;

    /**
     * @brief Process event across all active parallel states
     * @param context Execution context
     * @param eventName Event to process
     * @return Result containing summary of event processing results
     */
    virtual SCXML::Common::Result<std::map<std::string, bool>>
    processEventAcrossParallelStates(IExecutionContext &context, const std::string &eventName) = 0;

    /**
     * @brief Check completion status of all parallel states
     * @param context Execution context
     * @return Map of parallel state ID to completion status
     */
    virtual std::map<std::string, bool>
    checkParallelStateCompletions(IExecutionContext &context) const = 0;

    /**
     * @brief Get all currently active parallel states
     * @param context Execution context
     * @return Set of active parallel state IDs
     */
    virtual std::set<std::string> getActiveParallelStates(IExecutionContext &context) const = 0;

    /**
     * @brief Find parallel state by ID
     * @param parallelStateId Parallel state ID
     * @return Parallel state or nullptr if not found
     */
    virtual std::shared_ptr<IParallelNode> findParallelState(const std::string &parallelStateId) const = 0;

    /**
     * @brief Get all registered parallel states
     * @return Vector of parallel states
     */
    virtual std::vector<std::shared_ptr<IParallelNode>> getAllParallelStates() const = 0;

    /**
     * @brief Validate all parallel state configurations
     * @return List of validation errors, empty if all valid
     */
    virtual std::vector<std::string> validateAllParallelStates() const = 0;

    /**
     * @brief Export parallel state information for debugging
     * @param context Execution context
     * @return JSON representation of parallel state status
     */
    virtual std::string exportParallelStateInfo(IExecutionContext &context) const = 0;

    /**
     * @brief Set event processing callback for monitoring
     * @param callback Function to call when events are processed
     */
    virtual void
    setEventProcessingCallback(std::function<void(const std::string &, const std::string &, bool)> callback) = 0;
};

/**
 * @brief Parallel execution context for managing concurrent operations
 */
struct ParallelExecutionContext {
    std::string parallelStateId;                        ///< ID of the parallel state
    std::set<std::string> activeChildren;               ///< Currently active child states
    std::map<std::string, bool> childCompletionStatus;  ///< Completion status of each child
    std::chrono::steady_clock::time_point startTime;    ///< When parallel execution started
    size_t processedEvents;                             ///< Number of events processed in this context

    /**
     * @brief Constructor
     * @param id Parallel state ID
     */
    ParallelExecutionContext(const std::string &id) : parallelStateId(id), processedEvents(0) {
        startTime = std::chrono::steady_clock::now();
    }

    /**
     * @brief Check if all children are complete
     * @return True if all children are in complete state
     */
    bool allChildrenComplete() const {
        if (childCompletionStatus.empty()) {
            return false;
        }

        for (const auto &pair : childCompletionStatus) {
            if (!pair.second) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Get execution duration
     * @return Duration since parallel execution started
     */
    std::chrono::milliseconds getExecutionDuration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    }

    /**
     * @brief Convert to JSON representation
     * @return JSON string
     */
    std::string toJSON() const {
        std::ostringstream json;
        json << "{";
        json << "\"parallelStateId\":\"" << parallelStateId << "\",";
        json << "\"activeChildren\":[";
        bool first = true;
        for (const auto &child : activeChildren) {
            if (!first) {
                json << ",";
            }
            json << "\"" << child << "\"";
            first = false;
        }
        json << "],";
        json << "\"executionDurationMs\":" << getExecutionDuration().count() << ",";
        json << "\"processedEvents\":" << processedEvents << ",";
        json << "\"allComplete\":" << (allChildrenComplete() ? "true" : "false");
        json << "}";
        return json.str();
    }
};

}  // namespace Model
}  // namespace SCXML
