#include "runtime/StateHierarchyManager.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include "model/SCXMLModel.h"
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

    // 복합 상태인 경우 초기 자식 상태로 진입
    if (isCompoundState(stateNode)) {
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
    if (activeStates_.empty()) {
        return "";
    }

    // 가장 마지막에 추가된 상태가 가장 깊은 상태
    return activeStates_.back();
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

    // 해당 상태와 그 하위 상태들을 찾아서 제거
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

    // 찾은 상태들을 제거
    for (const auto &state : statesToRemove) {
        removeStateFromConfiguration(state);
    }

    Logger::debug("StateHierarchyManager::exitState - Removed " + std::to_string(statesToRemove.size()) + " states");
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

void StateHierarchyManager::addStateToConfiguration(const std::string &stateId) {
    if (stateId.empty() || activeSet_.find(stateId) != activeSet_.end()) {
        return;  // 이미 활성 상태이거나 빈 ID
    }

    activeStates_.push_back(stateId);
    activeSet_.insert(stateId);

    Logger::debug("StateHierarchyManager::addStateToConfiguration - Added: " + stateId +
                  " (total active: " + std::to_string(activeStates_.size()) + ")");
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

    // Type::COMPOUND 또는 자식 상태가 있는 경우 복합 상태로 판단
    return stateNode->getType() == Type::COMPOUND || !stateNode->getChildren().empty();
}

}  // namespace RSM