#include "runtime/StateHierarchyManager.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include "model/SCXMLModel.h"
#include "states/ConcurrentStateNode.h"
#include <algorithm>

namespace RSM {

StateHierarchyManager::StateHierarchyManager(std::shared_ptr<SCXMLModel> model) : model_(model) {
    Logger::debug("StateHierarchyManager: Initialized with SCXML model");
}

bool StateHierarchyManager::enterState(const std::string &stateId) {
    if (!model_ || stateId.empty()) {
        Logger::warn("StateHierarchyManager::enterState - Invalid parameters");
        return false;
    }

    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        Logger::warn("StateHierarchyManager::enterState - State not found: " + stateId);
        return false;
    }

    Logger::debug("StateHierarchyManager::enterState - Entering state: " + stateId);

    // 상태를 활성 구성에 추가
    addStateToConfiguration(stateId);

    // SCXML W3C specification section 3.4: parallel states behave differently from compound states
    if (stateNode->getType() == Type::PARALLEL) {
        // SCXML W3C specification section 3.4: ALL child regions MUST be activated when entering parallel state
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        Logger::debug("StateHierarchyManager::enterState - Entering parallel state with region activation: " + stateId);

        auto result = parallelState->enterParallelState();
        if (!result.isSuccess) {
            Logger::error("StateHierarchyManager::enterState - Failed to enter parallel state '" + stateId +
                          "': " + result.errorMessage);
            return false;
        }

        // SCXML W3C specification: Add ALL child regions to active configuration
        const auto &regions = parallelState->getRegions();
        assert(!regions.empty() && "SCXML violation: parallel state must have at least one region");

        for (const auto &region : regions) {
            assert(region && "SCXML violation: parallel state cannot have null regions");

            // Add region's root state to active configuration
            auto rootState = region->getRootState();
            assert(rootState && "SCXML violation: region must have root state");

            std::string regionStateId = rootState->getId();
            addStateToConfiguration(regionStateId);
            Logger::debug("StateHierarchyManager::enterState - Added region state to configuration: " + regionStateId);

            // SCXML W3C specification: Enter initial child state of each region
            const auto &children = rootState->getChildren();
            if (!children.empty()) {
                std::string initialChild = rootState->getInitialState();
                if (initialChild.empty()) {
                    // SCXML W3C: Use first child as default initial state
                    initialChild = children[0]->getId();
                }

                addStateToConfiguration(initialChild);
                Logger::debug("StateHierarchyManager::enterState - Added initial child state to configuration: " +
                              initialChild);
            }
        }

        Logger::debug("StateHierarchyManager::enterState - Successfully activated all regions in parallel state: " +
                      stateId);
    } else if (isCompoundState(stateNode)) {
        // Only for non-parallel compound states: enter initial child state
        std::string initialChild = findInitialChildState(stateNode);
        if (!initialChild.empty()) {
            Logger::debug("StateHierarchyManager::enterState - Entering initial child: " + initialChild);
            // 재귀적으로 자식 상태 진입
            return enterState(initialChild);
        } else {
            Logger::warn("StateHierarchyManager::enterState - No initial child found for compound state: " + stateId);
        }
    }

    Logger::debug("StateHierarchyManager::enterState - Successfully entered: " + stateId);
    return true;
}

std::string StateHierarchyManager::getCurrentState() const {
    Logger::debug("StateHierarchyManager::getCurrentState - Active states count: " +
                  std::to_string(activeStates_.size()));

    if (activeStates_.empty()) {
        Logger::debug("StateHierarchyManager::getCurrentState - No active states, returning empty");
        return "";
    }

    // Debug output: log each active state in the configuration
    for (size_t i = 0; i < activeStates_.size(); ++i) {
        Logger::debug("StateHierarchyManager::getCurrentState - Active state[" + std::to_string(i) +
                      "]: " + activeStates_[i]);
    }

    // SCXML W3C specification: parallel states define the current state context
    // Find the first parallel state in the active configuration
    if (model_) {
        for (const auto &stateId : activeStates_) {
            auto stateNode = model_->findStateById(stateId);
            if (stateNode) {
                auto stateType = stateNode->getType();
                Logger::debug("StateHierarchyManager::getCurrentState - State: " + stateId +
                              ", Type: " + std::to_string(static_cast<int>(stateType)) +
                              " (PARALLEL=" + std::to_string(static_cast<int>(Type::PARALLEL)) + ")");

                if (stateType == Type::PARALLEL) {
                    Logger::debug("StateHierarchyManager::getCurrentState - Found parallel state, returning: " +
                                  stateId);
                    return stateId;  // Return the parallel state as current state
                }
            } else {
                Logger::warn("StateHierarchyManager::getCurrentState - State node not found for: " + stateId);
            }
        }
    }

    // Return the last (most specific) state in the active configuration
    std::string result = activeStates_.back();
    Logger::debug("StateHierarchyManager::getCurrentState - No parallel state found, returning last state: " + result);
    return result;
}

std::vector<std::string> StateHierarchyManager::getActiveStates() const {
    return activeStates_;
}

bool StateHierarchyManager::isStateActive(const std::string &stateId) const {
    return activeSet_.find(stateId) != activeSet_.end();
}

void StateHierarchyManager::exitState(const std::string &stateId) {
    if (stateId.empty()) {
        return;
    }

    Logger::debug("StateHierarchyManager::exitState - Exiting state: " + stateId);

    // SCXML W3C specification section 3.4: parallel states need special exit handling
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && stateNode->getType() == Type::PARALLEL) {
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
            if (parallelState) {
                Logger::debug("StateHierarchyManager::exitState - Exiting parallel state with region deactivation: " +
                              stateId);
                auto result = parallelState->exitParallelState();
                if (!result.isSuccess) {
                    Logger::warn("StateHierarchyManager::exitState - Warning during parallel state exit '" + stateId +
                                 "': " + result.errorMessage);
                }
            }
        }
    }

    // Use specialized exit logic for parallel states vs hierarchical states
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && stateNode->getType() == Type::PARALLEL) {
            // SCXML W3C: For parallel states, remove the parallel state and ALL its descendants
            exitParallelStateAndDescendants(stateId);
            return;
        }
    }

    // SCXML W3C: For non-parallel states, use traditional hierarchical cleanup
    exitHierarchicalState(stateId);
}

void StateHierarchyManager::reset() {
    Logger::debug("StateHierarchyManager::reset - Clearing all active states");
    activeStates_.clear();
    activeSet_.clear();
}

bool StateHierarchyManager::isHierarchicalModeNeeded() const {
    // 활성 상태가 2개 이상이면 계층적 모드가 필요
    return activeStates_.size() > 1;
}

// Exit a parallel state by removing it and all its descendant regions
void StateHierarchyManager::exitParallelStateAndDescendants(const std::string &parallelStateId) {
    std::vector<std::string> statesToRemove;

    // SCXML W3C: Remove the parallel state itself
    auto it = std::find(activeStates_.begin(), activeStates_.end(), parallelStateId);
    if (it != activeStates_.end()) {
        statesToRemove.push_back(parallelStateId);
    }

    // SCXML W3C: Remove all descendant states of the parallel state
    if (model_) {
        auto parallelNode = model_->findStateById(parallelStateId);
        if (parallelNode && parallelNode->getType() == Type::PARALLEL) {
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(parallelNode);
            if (parallelState) {
                const auto &regions = parallelState->getRegions();
                for (const auto &region : regions) {
                    if (region && region->getRootState()) {
                        // Remove region root state and its children
                        std::string regionId = region->getRootState()->getId();
                        collectDescendantStates(regionId, statesToRemove);
                    }
                }
            }
        }
    }

    // Remove all collected states
    for (const auto &state : statesToRemove) {
        removeStateFromConfiguration(state);
    }

    Logger::debug("StateHierarchyManager::exitParallelStateAndDescendants - Removed " +
                  std::to_string(statesToRemove.size()) + " parallel states");
}

// Exit a hierarchical state by removing it and all child states
void StateHierarchyManager::exitHierarchicalState(const std::string &stateId) {
    std::vector<std::string> statesToRemove;

    bool foundState = false;
    for (auto it = activeStates_.begin(); it != activeStates_.end(); ++it) {
        if (*it == stateId) {
            foundState = true;
        }
        if (foundState) {
            statesToRemove.push_back(*it);
        }
    }

    for (const auto &state : statesToRemove) {
        removeStateFromConfiguration(state);
    }

    Logger::debug("StateHierarchyManager::exitHierarchicalState - Removed " + std::to_string(statesToRemove.size()) +
                  " hierarchical states");
}

// Recursively find all child states of a parent in the active configuration
void StateHierarchyManager::collectDescendantStates(const std::string &parentId, std::vector<std::string> &collector) {
    // Add the parent state itself if it's in active states
    auto it = std::find(activeStates_.begin(), activeStates_.end(), parentId);
    if (it != activeStates_.end()) {
        collector.push_back(parentId);
    }

    // Find and add all child states recursively
    if (model_) {
        auto parentNode = model_->findStateById(parentId);
        if (parentNode) {
            const auto &children = parentNode->getChildren();
            for (const auto &child : children) {
                if (child) {
                    collectDescendantStates(child->getId(), collector);
                }
            }
        }
    }
}

void StateHierarchyManager::addStateToConfiguration(const std::string &stateId) {
    if (stateId.empty() || activeSet_.find(stateId) != activeSet_.end()) {
        return;  // 이미 활성 상태이거나 빈 ID
    }

    activeStates_.push_back(stateId);
    activeSet_.insert(stateId);

    // Check state type for debugging
    std::string typeInfo = "unknown";
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode) {
            auto stateType = stateNode->getType();
            typeInfo = std::to_string(static_cast<int>(stateType));
            if (stateType == Type::PARALLEL) {
                typeInfo += "(PARALLEL)";
            } else if (stateType == Type::FINAL) {
                typeInfo += "(FINAL)";
            } else if (stateType == Type::COMPOUND) {
                typeInfo += "(COMPOUND)";
            } else if (stateType == Type::ATOMIC) {
                typeInfo += "(ATOMIC)";
            }
        }
    }

    Logger::debug("StateHierarchyManager::addStateToConfiguration - Added: " + stateId + " type=" + typeInfo +
                  " (total active: " + std::to_string(activeStates_.size()) + ")");

    // Log current state order for debugging
    std::string stateOrder = "Current order: [";
    for (size_t i = 0; i < activeStates_.size(); ++i) {
        if (i > 0) {
            stateOrder += ", ";
        }
        stateOrder += activeStates_[i];
    }
    stateOrder += "]";
    Logger::debug("StateHierarchyManager::addStateToConfiguration - " + stateOrder);
}

void StateHierarchyManager::removeStateFromConfiguration(const std::string &stateId) {
    if (stateId.empty()) {
        return;
    }

    // 벡터에서 제거
    auto it = std::find(activeStates_.begin(), activeStates_.end(), stateId);
    if (it != activeStates_.end()) {
        activeStates_.erase(it);
    }

    // 세트에서 제거
    activeSet_.erase(stateId);

    Logger::debug("StateHierarchyManager::removeStateFromConfiguration - Removed: " + stateId);
}

std::string StateHierarchyManager::findInitialChildState(IStateNode *stateNode) const {
    if (!stateNode) {
        return "";
    }

    // 1. 명시적 initial 속성 확인
    std::string explicitInitial = stateNode->getInitialState();
    if (!explicitInitial.empty()) {
        Logger::debug("StateHierarchyManager::findInitialChildState - Found explicit initial: " + explicitInitial);
        return explicitInitial;
    }

    // 2. 첫 번째 자식 상태 사용 (기본값)
    const auto &children = stateNode->getChildren();
    if (!children.empty() && children[0]) {
        std::string defaultInitial = children[0]->getId();
        Logger::debug("StateHierarchyManager::findInitialChildState - Using default initial: " + defaultInitial);
        return defaultInitial;
    }

    Logger::debug("StateHierarchyManager::findInitialChildState - No child states found");
    return "";
}

bool StateHierarchyManager::isCompoundState(IStateNode *stateNode) const {
    if (!stateNode) {
        return false;
    }

    // SCXML W3C specification: only COMPOUND types are compound states, not PARALLEL
    // Parallel states have different semantics and should not auto-enter children
    return stateNode->getType() == Type::COMPOUND;
}

}  // namespace RSM