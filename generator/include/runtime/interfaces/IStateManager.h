#pragma once

#include <memory>
#include <string>
#include <vector>

namespace SCXML {

// Forward declarations for Model namespace
namespace Model {
class DocumentModel;
class IStateNode;
class DoneData;
}

namespace Runtime {

// Using declarations for Model namespace
using SCXML::Model::DocumentModel;
using SCXML::Model::IStateNode;
using SCXML::Model::DoneData;

/**
 * @brief Interface for state management operations
 */
class IStateManager {
public:
    virtual ~IStateManager() = default;

    // Core interface methods
    virtual void initializeFromModel(std::shared_ptr<DocumentModel> model) = 0;
    virtual const std::shared_ptr<DocumentModel> getModel() const = 0;
    virtual std::shared_ptr<IStateNode> getStateNode(const std::string &stateId) const = 0;
    virtual std::shared_ptr<IStateNode> getCurrentStateNode() const = 0;

    // Additional methods needed by RuntimeContext
    virtual void setCurrentState(const std::string &stateId) = 0;
    virtual std::string getCurrentState() const = 0;
    virtual std::vector<std::string> getActiveStates() const = 0;
    virtual bool isInState(const std::string &stateId) const = 0;
    virtual void enterState(const std::string &stateId) = 0;
    virtual void exitState(const std::string &stateId) = 0;
};

}  // namespace Runtime
}  // namespace SCXML
