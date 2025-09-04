#include "runtime/impl/StateManager.h"
#include "common/Logger.h"
#include "model/DocumentModel.h"
#include "model/IStateNode.h"
#include <algorithm>

namespace SCXML {
namespace Runtime {

StateManager::StateManager() {
    SCXML::Common::Logger::debug("StateManager::Constructor - Creating state manager");
}

void StateManager::initializeFromModel(std::shared_ptr<Model::DocumentModel> model) {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    model_ = model;
    
    // Clear existing state
    activeStates_.clear();
    activeStatesSet_.clear();
    currentState_.clear();
    
    SCXML::Common::Logger::debug("StateManager::initializeFromModel - Initialized with model");
}

// Default destructor is already declared in header

void StateManager::setCurrentState(const std::string &stateId) {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    currentState_ = stateId;

    // Ensure current state is in active states (original logic)
    if (activeStatesSet_.find(stateId) == activeStatesSet_.end()) {
        activeStates_.push_back(stateId);
        activeStatesSet_.insert(stateId);
    }

    SCXML::Common::Logger::debug("StateManager::setCurrentState - Set to: " + stateId);
}

std::string StateManager::getCurrentState() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return currentState_;
}

std::vector<std::string> StateManager::getActiveStates() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return activeStates_;  // Return the ordered list directly
}

bool StateManager::isInState(const std::string &stateId) const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    return activeStatesSet_.find(stateId) != activeStatesSet_.end();
}

bool StateManager::isAtomic(const std::string & /* stateId */) const {
    // Placeholder implementation - would need model to determine
    return true;
}

bool StateManager::isCompound(const std::string & /* stateId */) const {
    // Placeholder implementation - would need model to determine
    return false;
}

bool StateManager::isFinal(const std::string & /* stateId */) const {
    // Placeholder implementation - would need model to determine
    return false;
}

std::vector<std::string> StateManager::getChildStates(const std::string & /* stateId */) const {
    // Placeholder implementation - would need model to determine
    return {};
}

std::string StateManager::getParentState(const std::string & /* stateId */) const {
    // Placeholder implementation - would need model to determine
    return "";
}

std::vector<std::string> StateManager::getAncestors(const std::string & /* stateId */) const {
    // Placeholder implementation - would need model to determine
    return {};
}

void StateManager::enterState(const std::string &stateId) {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    // Original activateState logic
    if (activeStatesSet_.find(stateId) == activeStatesSet_.end()) {
        activeStates_.push_back(stateId);
        activeStatesSet_.insert(stateId);
    }
    SCXML::Common::Logger::debug("StateManager::enterState - Entered: " + stateId);
}

void StateManager::exitState(const std::string &stateId) {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    // Original deactivateState logic
    activeStatesSet_.erase(stateId);
    auto it = std::find(activeStates_.begin(), activeStates_.end(), stateId);
    if (it != activeStates_.end()) {
        activeStates_.erase(it);

        // If this was the current state, clear it (original logic)
        if (currentState_ == stateId) {
            currentState_.clear();
        }
    }
    SCXML::Common::Logger::debug("StateManager::exitState - Exited: " + stateId);
}

std::unordered_set<std::string> StateManager::getConfiguration() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    // Convert activeStatesSet to unordered_set for interface compatibility
    return activeStatesSet_;
}

void StateManager::setConfiguration(const std::unordered_set<std::string> &config) {
    std::unique_lock<std::shared_mutex> lock(stateMutex_);
    // Update both data structures
    activeStatesSet_ = config;
    activeStates_.clear();
    activeStates_.assign(config.begin(), config.end());
}

void StateManager::setModel(std::shared_ptr<Model::DocumentModel> model) {
    model_ = model;
}

const std::shared_ptr<Model::DocumentModel> StateManager::getModel() const {
    return model_;
}

std::shared_ptr<Model::IStateNode> StateManager::getStateNode(const std::string & /* stateId */) const {
    // Placeholder implementation - would need to get from model
    return nullptr;
}

std::shared_ptr<Model::IStateNode> StateManager::getCurrentStateNode() const {
    std::shared_lock<std::shared_mutex> lock(stateMutex_);
    if (!model_ || currentState_.empty()) {
        return nullptr;
    }

    // Original logic: Get current state node from model
    try {
        auto stateNode = model_->findStateById(currentState_);
        if (stateNode) {
            // Convert raw pointer to shared_ptr with no-op deleter (original logic)
            return std::shared_ptr<Model::IStateNode>(stateNode, [](Model::IStateNode *) {});
        }
        SCXML::Common::Logger::debug("State node not found for ID: " + currentState_);
    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("Error accessing state node: " + std::string(e.what()));
    }

    return nullptr;
}

void StateManager::updateStateNodeCache() const {
    // Cache implementation - would populate from model if needed
    if (!model_) {
        return;
    }

    // Could implement caching logic here for performance
    SCXML::Common::Logger::debug("StateManager::updateStateNodeCache - Cache update requested");
}

std::shared_ptr<Model::IStateNode> StateManager::findStateNode(const std::string &stateId) const {
    // Check cache first
    auto cacheIt = stateNodeCache_.find(stateId);
    if (cacheIt != stateNodeCache_.end()) {
        return cacheIt->second;
    }

    // Search in model
    if (model_) {
        try {
            auto stateNode = model_->findStateById(stateId);
            if (stateNode) {
                auto sharedNode = std::shared_ptr<Model::IStateNode>(stateNode, [](Model::IStateNode *) {});
                stateNodeCache_[stateId] = sharedNode;  // Cache it
                return sharedNode;
            }
        } catch (const std::exception &e) {
            SCXML::Common::Logger::error("Error finding state node: " + std::string(e.what()));
        }
    }

    return nullptr;
}

}  // namespace Runtime
}  // namespace SCXML