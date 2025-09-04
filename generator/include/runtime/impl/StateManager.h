#pragma once

#include "../interfaces/IStateManager.h"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace SCXML {

namespace Model {
class IStateNode;
class DocumentModel;
}
namespace Runtime {

/**
 * @brief Default implementation of state management
 */
class StateManager : public IStateManager {
public:
    StateManager();
    virtual ~StateManager() = default;

    // IStateManager interface implementation
    void setCurrentState(const std::string &stateId) override;
    std::string getCurrentState() const override;
    std::vector<std::string> getActiveStates() const override;
    bool isInState(const std::string &stateId) const override;
    void enterState(const std::string &stateId) override;
    void exitState(const std::string &stateId) override;

    // Additional implementation methods (not in interface)
    bool isAtomic(const std::string &stateId) const;
    bool isCompound(const std::string &stateId) const;
    bool isFinal(const std::string &stateId) const;
    std::vector<std::string> getChildStates(const std::string &stateId) const;
    std::string getParentState(const std::string &stateId) const;
    std::vector<std::string> getAncestors(const std::string &stateId) const;
    std::unordered_set<std::string> getConfiguration() const;
    void setConfiguration(const std::unordered_set<std::string> &config);

    // IStateManager interface implementation
    void initializeFromModel(std::shared_ptr<Model::DocumentModel> model) override;
    const std::shared_ptr<Model::DocumentModel> getModel() const override;
    std::shared_ptr<Model::IStateNode> getStateNode(const std::string &stateId) const override;
    std::shared_ptr<Model::IStateNode> getCurrentStateNode() const override;

    // Additional state management methods
    void setModel(std::shared_ptr<Model::DocumentModel> model);

private:
    mutable std::shared_mutex stateMutex_;  // Use shared_mutex like original
    std::string currentState_;
    std::unordered_set<std::string> activeStatesSet_;  // Fast lookup set
    std::vector<std::string> activeStates_;            // Ordered list
    std::shared_ptr<Model::DocumentModel> model_;

    // State node cache for performance
    mutable std::unordered_map<std::string, std::shared_ptr<Model::IStateNode>> stateNodeCache_;

    void updateStateNodeCache() const;
    std::shared_ptr<Model::IStateNode> findStateNode(const std::string &stateId) const;
};

}  // namespace Runtime
}  // namespace SCXML