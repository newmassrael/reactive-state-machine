#include "model/StateHierarchy.h"
#include "common/Logger.h"
#include "model/ITransitionNode.h"
#include <algorithm>
#include <iostream>
#include <stack>
#include <unordered_set>

RSM::StateHierarchy::StateHierarchy() : rootState_(nullptr) {
    LOG_DEBUG("Creating state hierarchy");
}

RSM::StateHierarchy::~StateHierarchy() {
    LOG_DEBUG("Destroying state hierarchy");
    // 스마트 포인터가 자원 정리를 담당
}

void RSM::StateHierarchy::setRootState(std::shared_ptr<RSM::IStateNode> rootState) {
    LOG_DEBUG("Setting root state: {}", (rootState ? rootState->getId() : "null"));
    rootState_ = rootState;

    if (rootState_) {
        // 루트 상태가 설정되면 상태 ID 맵에 추가
        addState(rootState_);
    }
}

RSM::IStateNode *RSM::StateHierarchy::getRootState() const {
    return rootState_.get();
}

bool RSM::StateHierarchy::addState(std::shared_ptr<RSM::IStateNode> state, const std::string &parentId) {
    if (!state) {
        LOG_WARN("Attempt to add null state");
        return false;
    }

    LOG_DEBUG("Adding state: {}", state->getId());

    // 부모 ID가 지정된 경우, 해당 부모를 찾고 자식으로 추가
    if (!parentId.empty()) {
        RSM::IStateNode *parent = findStateById(parentId);
        if (!parent) {
            LOG_ERROR("Parent state not found: {}", parentId);
            return false;
        }

        // 부모-자식 관계 설정
        state->setParent(parent);
        parent->addChild(state);
    } else if (rootState_ && rootState_.get() != state.get()) {
        // 부모 ID가 지정되지 않았지만 루트가 아닌 경우, 루트의 자식으로 추가
        state->setParent(rootState_.get());
        rootState_->addChild(state);
    }

    // 상태 목록 및 맵에 추가
    allStates_.push_back(state);
    stateIdMap_[state->getId()] = state.get();

    return true;
}

RSM::IStateNode *RSM::StateHierarchy::findStateById(const std::string &id) const {
    auto it = stateIdMap_.find(id);
    if (it != stateIdMap_.end()) {
        return it->second;
    }
    return nullptr;
}

bool RSM::StateHierarchy::isDescendantOf(const std::string &ancestorId, const std::string &descendantId) const {
    RSM::IStateNode *ancestor = findStateById(ancestorId);
    RSM::IStateNode *descendant = findStateById(descendantId);

    if (!ancestor || !descendant) {
        return false;
    }

    return isDescendantOf(ancestor, descendant);
}

bool RSM::StateHierarchy::isDescendantOf(RSM::IStateNode *ancestor, RSM::IStateNode *descendant) const {
    if (!ancestor || !descendant) {
        return false;
    }

    // 자기 자신은 자신의 descendant가 아님
    if (ancestor == descendant) {
        return false;
    }

    // 부모-자식 관계 확인
    RSM::IStateNode *parent = descendant->getParent();

    // 부모가 없으면 false 반환
    if (!parent) {
        return false;
    }

    // 직계 부모이면 true
    if (parent == ancestor) {
        return true;
    }

    // 재귀적으로 조상 확인
    return isDescendantOf(ancestor, parent);
}

const std::vector<std::shared_ptr<RSM::IStateNode>> &RSM::StateHierarchy::getAllStates() const {
    return allStates_;
}

bool RSM::StateHierarchy::validateRelationships() const {
    LOG_INFO("Validating state relationships");

    // 모든 상태에 대해 검증
    for (const auto &state : allStates_) {
        // 부모 상태 검증
        RSM::IStateNode *parent = state->getParent();
        if (parent) {
            // 부모가 실제로 이 상태를 자식으로 가지고 있는지 확인
            bool foundAsChild = false;
            for (const auto &childState : parent->getChildren()) {
                if (childState.get() == state.get()) {
                    foundAsChild = true;
                    break;
                }
            }

            if (!foundAsChild) {
                LOG_ERROR("State '{}' has parent '{}' but is not in parent's children list", state->getId(),
                          parent->getId());
                return false;
            }
        }

        // 초기 상태가 존재하는지 확인
        if (!state->getInitialState().empty()) {
            bool initialStateExists = false;
            for (const auto &child : state->getChildren()) {
                if (child->getId() == state->getInitialState()) {
                    initialStateExists = true;
                    break;
                }
            }

            if (!initialStateExists && !state->getChildren().empty()) {
                LOG_ERROR("State '{}' references non-existent initial state '{}'", state->getId(),
                          state->getInitialState());
                return false;
            }
        }
    }

    LOG_INFO("All state relationships are valid");
    return true;
}

std::vector<std::string> RSM::StateHierarchy::findMissingStateIds() const {
    LOG_INFO("Looking for missing state IDs");

    std::vector<std::string> missingIds;
    std::unordered_set<std::string> existingIds;

    // 모든 상태 ID 수집
    for (const auto &state : allStates_) {
        existingIds.insert(state->getId());
    }

    // 참조된 상태 ID 확인
    for (const auto &state : allStates_) {
        // 초기 상태 확인
        if (!state->getInitialState().empty() && existingIds.find(state->getInitialState()) == existingIds.end()) {
            missingIds.push_back(state->getInitialState());
            LOG_WARN("Missing state ID referenced as initial state: {}", state->getInitialState());
        }

        // 전환 타겟 확인
        for (const auto &transition : state->getTransitions()) {
            const auto targets = transition->getTargets();
            for (const auto &target : targets) {
                if (!target.empty() && existingIds.find(target) == existingIds.end()) {
                    missingIds.push_back(target);
                    LOG_WARN("Missing state ID referenced as transition target: {}", target);
                }
            }
        }
    }

    // 중복 제거
    std::sort(missingIds.begin(), missingIds.end());
    missingIds.erase(std::unique(missingIds.begin(), missingIds.end()), missingIds.end());

    LOG_INFO("Found {} missing state IDs", missingIds.size());
    return missingIds;
}

void RSM::StateHierarchy::printHierarchy() const {
    LOG_INFO("Printing state hierarchy");

    LOG_INFO("State Hierarchy:");
    LOG_INFO("===============");

    if (rootState_) {
        printStateHierarchy(rootState_.get(), 0);
    } else {
        LOG_INFO("  <No root state>");
    }

    LOG_INFO("State hierarchy printed");
}

void RSM::StateHierarchy::printStateHierarchy(RSM::IStateNode *state, int depth) const {
    if (!state) {
        return;
    }

    // 들여쓰기 생성
    std::string indent(depth * 2, ' ');

    // 현재 상태 정보 출력
    LOG_INFO("{}State: {}", indent, state->getId());

    // 상태 타입 출력
    switch (state->getType()) {
    case Type::ATOMIC:
        LOG_INFO(" (atomic)");
        break;
    case Type::COMPOUND:
        LOG_INFO(" (compound)");
        break;
    case Type::PARALLEL:
        LOG_INFO(" (parallel)");
        break;
    case Type::FINAL:
        LOG_INFO(" (final)");
        break;
    case Type::HISTORY:
        LOG_INFO(" (history)");
        break;
    case Type::INITIAL:
        LOG_INFO(" (initial)");
        break;
    }

    // 초기 상태 정보 출력
    if (!state->getInitialState().empty()) {
        LOG_INFO(" [initial: {}]", state->getInitialState());
    }

    // Line break handled by previous Logger::info calls

    // 전환 정보 출력
    for (const auto &transition : state->getTransitions()) {
        LOG_INFO("{}  Transition: {} -> ", indent,
                 (transition->getEvent().empty() ? "<no event>" : transition->getEvent()));

        const auto &targets = transition->getTargets();
        if (targets.empty()) {
            LOG_INFO("<no target>");
        } else {
            for (size_t i = 0; i < targets.size(); ++i) {
                LOG_INFO("{}", targets[i]);
                if (i < targets.size() - 1) {
                    LOG_INFO(", ");
                }
            }
        }

        if (!transition->getGuard().empty()) {
            LOG_INFO(" [guard: {}]", transition->getGuard());
        }

        // Line break handled by previous Logger::info calls
    }

    // 자식 상태 재귀적으로 출력
    for (const auto &child : state->getChildren()) {
        printStateHierarchy(child.get(), depth + 1);
    }
}
