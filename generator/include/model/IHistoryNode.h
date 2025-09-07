#pragma once
#include "IExecutionContext.h"
#include "common/SCXMLCommon.h"
#include "core/types.h"  // For SCXML::HistoryType
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace SCXML {
namespace Model {

// Forward declarations
using HistoryType = SCXML::HistoryType;  // Use core HistoryType as canonical source

/**
 * @brief Interface for SCXML <history> element implementation
 *
 * History states provide a way to remember a previously exited
 * state configuration and return to it later. SCXML supports both
 * shallow and deep history semantics.
 */
class IHistoryNode {
public:
    virtual ~IHistoryNode() = default;

    /**
     * @brief Get the history type (shallow or deep)
     * @return History type
     */
    virtual SCXML::HistoryType getType() const = 0;

    /**
     * @brief Set the history type
     * @param type History type
     */
    virtual void setType(SCXML::HistoryType type) = 0;

    /**
     * @brief Get the history state ID
     * @return Unique identifier for this history state
     */
    virtual const std::string &getId() const = 0;

    /**
     * @brief Set the history state ID
     * @param id Unique identifier
     */
    virtual void setId(const std::string &id) = 0;

    /**
     * @brief Get the parent state ID that owns this history
     * @return Parent state identifier
     */
    virtual const std::string &getParentState() const = 0;

    /**
     * @brief Set the parent state that owns this history
     * @param parentState Parent state identifier
     */
    virtual void setParentState(const std::string &parentState) = 0;

    /**
     * @brief Get default transition target if no history exists
     * @return Default target state ID
     */
    virtual const std::string &getDefaultTarget() const = 0;

    /**
     * @brief Set default transition target
     * @param defaultTarget Default target state ID
     */
    virtual void setDefaultTarget(const std::string &defaultTarget) = 0;

    /**
     * @brief Record the current state configuration for history
     * @param context Execution context containing current state
     * @param activeStates Set of currently active state IDs
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> recordHistory(SCXML::Model::IExecutionContext &context,
                                                      const std::set<std::string> &activeStates) = 0;

    /**
     * @brief Get the stored history state configuration
     * @param context Execution context
     * @return Result containing set of state IDs to restore or error
     */
    virtual SCXML::Common::Result<std::set<std::string>>
    getStoredHistory(SCXML::Model::IExecutionContext &context) const = 0;

    /**
     * @brief Clear stored history
     * @param context Execution context
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> clearHistory(SCXML::Model::IExecutionContext &context) = 0;

    /**
     * @brief Check if history has been recorded
     * @param context Execution context
     * @return True if history exists
     */
    virtual bool hasHistory(SCXML::Model::IExecutionContext &context) const = 0;

    /**
     * @brief Resolve target states for history transition
     * @param context Execution context
     * @return Result containing target state IDs or error
     */
    virtual SCXML::Common::Result<std::set<std::string>>
    resolveHistoryTransition(SCXML::Model::IExecutionContext &context) const = 0;

    /**
     * @brief Validate history node configuration
     * @return List of validation errors, empty if valid
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Clone this history node
     * @return Deep copy of this history node
     */
    virtual std::shared_ptr<IHistoryNode> clone() const = 0;

    /**
     * @brief Convert history type to string
     * @param type History type
     * @return String representation
     */
    static std::string historyTypeToString(SCXML::HistoryType type);

    /**
     * @brief Convert string to history type
     * @param typeStr String representation
     * @return History type
     */
    static SCXML::HistoryType stringToHistoryType(const std::string &typeStr);
};

/**
 * @brief Interface for History State Manager
 *
 * Manages all history states within a state machine and provides
 * centralized history recording and restoration services.
 */
class IHistoryStateManager {
public:
    virtual ~IHistoryStateManager() = default;

    /**
     * @brief Register a history node with the manager
     * @param historyNode History node to register
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> registerHistoryNode(std::shared_ptr<IHistoryNode> historyNode) = 0;

    /**
     * @brief Record history for all relevant history nodes when exiting states
     * @param context Execution context
     * @param exitingStates Set of states being exited
     * @param activeStates Set of currently active states
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> recordHistoryOnExit(SCXML::Model::IExecutionContext &context,
                                                            const std::set<std::string> &exitingStates,
                                                            const std::set<std::string> &activeStates) = 0;

    /**
     * @brief Get history restoration targets for a history transition
     * @param context Execution context
     * @param historyId History node ID
     * @return Result containing target state IDs or error
     */
    virtual SCXML::Common::Result<std::set<std::string>> getHistoryTargets(SCXML::Model::IExecutionContext &context,
                                                                           const std::string &historyId) const = 0;

    /**
     * @brief Clear all history for a specific parent state
     * @param context Execution context
     * @param parentStateId Parent state ID
     * @return Result indicating success or failure
     */
    virtual SCXML::Common::Result<void> clearHistoryForParent(SCXML::Model::IExecutionContext &context,
                                                              const std::string &parentStateId) = 0;

    /**
     * @brief Get all registered history nodes
     * @return Vector of history nodes
     */
    virtual std::vector<std::shared_ptr<IHistoryNode>> getAllHistoryNodes() const = 0;

    /**
     * @brief Find history node by ID
     * @param historyId History node ID
     * @return History node or nullptr if not found
     */
    virtual std::shared_ptr<IHistoryNode> findHistoryNode(const std::string &historyId) const = 0;

    /**
     * @brief Validate all history configurations
     * @return List of validation errors, empty if all valid
     */
    virtual std::vector<std::string> validateAllHistories() const = 0;

    /**
     * @brief Export history state for debugging
     * @param context Execution context
     * @return JSON representation of all history states
     */
    virtual std::string exportHistoryState(SCXML::Model::IExecutionContext &context) const = 0;
};

}  // namespace Model
}  // namespace SCXML
