#pragma once
#include "common/SCXMLCommon.h"
#include "model/IExecutionContext.h"
#include "model/IHistoryNode.h"
#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <string>

using SCXML::Model::IExecutionContext;
using SCXML::Model::IHistoryNode;

namespace SCXML {
namespace Core {

// Forward declarations

/**
 * @brief Concrete implementation of SCXML <history> element
 *
 * This class implements both shallow and deep history semantics
 * as defined in the SCXML 1.0 specification.
 */
class HistoryNode : public Model::IHistoryNode {
public:
    /**
     * @brief Constructor
     * @param id History node ID
     * @param type History type (shallow or deep)
     */
    HistoryNode(const std::string &id, SCXML::HistoryType type = SCXML::HistoryType::SHALLOW);

    /**
     * @brief Destructor
     */
    virtual ~HistoryNode() = default;

    // IHistoryNode interface implementation
    SCXML::HistoryType getType() const override;
    void setType(SCXML::HistoryType type) override;

    const std::string &getId() const override;
    void setId(const std::string &id) override;

    const std::string &getParentState() const override;
    void setParentState(const std::string &parentState) override;

    const std::string &getDefaultTarget() const override;
    void setDefaultTarget(const std::string &defaultTarget) override;

    SCXML::Common::Result<void> recordHistory(SCXML::Model::IExecutionContext &context,
                                              const std::set<std::string> &activeStates) override;

    SCXML::Common::Result<std::set<std::string>>
    getStoredHistory(SCXML::Model::IExecutionContext &context) const override;

    SCXML::Common::Result<void> clearHistory(SCXML::Model::IExecutionContext &context) override;

    bool hasHistory(SCXML::Model::IExecutionContext &context) const override;

    SCXML::Common::Result<std::set<std::string>>
    resolveHistoryTransition(SCXML::Model::IExecutionContext &context) const override;

    std::vector<std::string> validate() const override;
    std::shared_ptr<IHistoryNode> clone() const override;

private:
    std::string id_;             ///< History node identifier
    SCXML::HistoryType type_;           ///< History type (shallow/deep)
    std::string parentState_;    ///< Parent state that owns this history
    std::string defaultTarget_;  ///< Default target if no history exists

    /**
     * @brief Get the storage key for this history in the data model
     * @return Storage key string
     */
    std::string getStorageKey() const;

    /**
     * @brief Filter states for shallow history
     * @param activeStates All active states
     * @return Filtered states for shallow history
     */
    std::set<std::string> filterShallowHistory(const std::set<std::string> &activeStates) const;

    /**
     * @brief Filter states for deep history
     * @param activeStates All active states
     * @return Filtered states for deep history
     */
    std::set<std::string> filterDeepHistory(const std::set<std::string> &activeStates) const;

    /**
     * @brief Check if a state is a child of the parent state
     * @param stateId State to check
     * @return True if state is a child of parent
     */
    bool isChildOfParent(const std::string &stateId) const;

    /**
     * @brief Get the depth level of a state relative to parent
     * @param stateId State ID
     * @return Depth level (0 = direct child, 1 = grandchild, etc.)
     */
    int getStateDepth(const std::string &stateId) const;

    /**
     * @brief Serialize state set to string for storage
     * @param states Set of state IDs
     * @return Serialized string
     */
    std::string serializeStateSet(const std::set<std::string> &states) const;

    /**
     * @brief Deserialize string to state set
     * @param serialized Serialized string
     * @return Set of state IDs
     */
    std::set<std::string> deserializeStateSet(const std::string &serialized) const;
};

/**
 * @brief Concrete implementation of History State Manager
 *
 * Provides centralized management of all history states within
 * a state machine and coordinates history recording and restoration.
 */
class HistoryStateManager : public Model::IHistoryStateManager {
public:
    /**
     * @brief Constructor
     */
    HistoryStateManager();

    /**
     * @brief Destructor
     */
    virtual ~HistoryStateManager() = default;

    // IHistoryStateManager interface implementation
    SCXML::Common::Result<void> registerHistoryNode(std::shared_ptr<IHistoryNode> historyNode) override;

    SCXML::Common::Result<void> recordHistoryOnExit(SCXML::Model::IExecutionContext &context,
                                                    const std::set<std::string> &exitingStates,
                                                    const std::set<std::string> &activeStates) override;

    SCXML::Common::Result<std::set<std::string>> getHistoryTargets(SCXML::Model::IExecutionContext &context,
                                                                   const std::string &historyId) const override;

    SCXML::Common::Result<void> clearHistoryForParent(SCXML::Model::IExecutionContext &context,
                                                      const std::string &parentStateId) override;

    std::vector<std::shared_ptr<IHistoryNode>> getAllHistoryNodes() const override;
    std::shared_ptr<IHistoryNode> findHistoryNode(const std::string &historyId) const override;
    std::vector<std::string> validateAllHistories() const override;
    std::string exportHistoryState(SCXML::Model::IExecutionContext &context) const override;

private:
    std::map<std::string, std::shared_ptr<IHistoryNode>> historyNodes_;  ///< Registered history nodes

    /**
     * @brief Find all history nodes that should record when exiting given states
     * @param exitingStates States being exited
     * @return Vector of history nodes to update
     */
    std::vector<std::shared_ptr<IHistoryNode>> findAffectedHistories(const std::set<std::string> &exitingStates) const;

    /**
     * @brief Get all ancestor states of a given state
     * @param stateId State ID
     * @return Set of ancestor state IDs
     */
    std::set<std::string> getAncestorStates(const std::string &stateId) const;

    /**
     * @brief Check if any of the exiting states is an ancestor of the history's parent
     * @param historyNode History node to check
     * @param exitingStates States being exited
     * @return True if history should be recorded
     */
    bool shouldRecordHistory(std::shared_ptr<IHistoryNode> historyNode,
                             const std::set<std::string> &exitingStates) const;
};

}  // namespace Core
}  // namespace SCXML
