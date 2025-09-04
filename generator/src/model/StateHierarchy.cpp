#include "model/StateHierarchy.h"
#include "common/Logger.h"
#include "model/ITransitionNode.h"
#include <algorithm>
#include <iostream>
#include <stack>
#include <unordered_set>

namespace SCXML {

StateHierarchy::StateHierarchy() : rootState_(nullptr) {
    SCXML::Common::Logger::debug("StateHierarchy::Constructor - Creating state hierarchy");
}

StateHierarchy::~StateHierarchy() {
    SCXML::Common::Logger::debug("StateHierarchy::Destructor - Destroying state hierarchy");
    // 스마트 포인터가 자원 정리를 담당
}

void StateHierarchy::setRootState(std::shared_ptr<Model::IStateNode> rootState) {
    SCXML::Common::Logger::debug("StateHierarchy::setRootState() - Setting root state: " + (rootState ? rootState->getId() : "null"));
    rootState_ = rootState;

    if (rootState_) {
        // 루트 상태가 설정되면 상태 ID 맵에 추가
        addState(rootState_);
    }
}

Model::IStateNode *StateHierarchy::getRootState() const {
    return rootState_.get();
}

bool StateHierarchy::addState(std::shared_ptr<Model::IStateNode> state, const std::string &parentId) {
    if (!state) {
        SCXML::Common::Logger::warning("StateHierarchy::addState() - Attempt to add null state");
        return false;
    }

    SCXML::Common::Logger::debug("StateHierarchy::addState() - Adding state: " + state->getId());

    // 부모 ID가 지정된 경우, 해당 부모를 찾고 자식으로 추가
    if (!parentId.empty()) {
        Model::IStateNode *parent = findStateById(parentId);
        if (!parent) {
            SCXML::Common::Logger::error("StateHierarchy::addState() - Parent state not found: " + parentId);
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

Model::IStateNode *StateHierarchy::findStateById(const std::string &id) const {
    auto it = stateIdMap_.find(id);
    if (it != stateIdMap_.end()) {
        return it->second;
    }
    return nullptr;
}

bool StateHierarchy::isDescendantOf(const std::string &ancestorId, const std::string &descendantId) const {
    Model::IStateNode *ancestor = findStateById(ancestorId);
    Model::IStateNode *descendant = findStateById(descendantId);

    if (!ancestor || !descendant) {
        return false;
    }

    return isDescendantOf(ancestor, descendant);
}

bool StateHierarchy::isDescendantOf(Model::IStateNode *ancestor, Model::IStateNode *descendant) const {
    if (!ancestor || !descendant) {
        return false;
    }

    // 자기 자신은 자신의 descendant가 아님
    if (ancestor == descendant) {
        return false;
    }

    // 부모-자식 관계 확인
    Model::IStateNode *parent = descendant->getParent();

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

const std::vector<std::shared_ptr<Model::IStateNode>> &StateHierarchy::getAllStates() const {
    return allStates_;
}

bool StateHierarchy::validateRelationships() const {
    SCXML::Common::Logger::info("StateHierarchy::validateRelationships() - Validating state relationships");

    // 모든 상태에 대해 검증
    for (const auto &state : allStates_) {
        // 부모 상태 검증
        Model::IStateNode *parent = state->getParent();
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
                SCXML::Common::Logger::error("StateHierarchy::validateRelationships() - State '" + state->getId() + "' has parent '" +
                              parent->getId() + "' but is not in parent's children list");
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
                SCXML::Common::Logger::error("StateHierarchy::validateRelationships() - State '" + state->getId() +
                              "' references non-existent initial state '" + state->getInitialState() + "'");
                return false;
            }
        }
    }

    SCXML::Common::Logger::info("StateHierarchy::validateRelationships() - All state relationships are valid");
    return true;
}

std::vector<std::string> StateHierarchy::findMissingStateIds() const {
    SCXML::Common::Logger::info("StateHierarchy::findMissingStateIds() - Looking for missing state IDs");

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
            SCXML::Common::Logger::warning("StateHierarchy::findMissingStateIds() - Missing state ID referenced as initial state: " +
                            state->getInitialState());
        }

        // 전환 타겟 확인
        for (const auto &transition : state->getTransitions()) {
            const auto targets = transition->getTargets();
            for (const auto &target : targets) {
                if (!target.empty() && existingIds.find(target) == existingIds.end()) {
                    missingIds.push_back(target);
                    SCXML::Common::Logger::warning(
                        "StateHierarchy::findMissingStateIds() - Missing state ID referenced as transition target: " +
                        target);
                }
            }
        }
    }

    // 중복 제거
    std::sort(missingIds.begin(), missingIds.end());
    missingIds.erase(std::unique(missingIds.begin(), missingIds.end()), missingIds.end());

    SCXML::Common::Logger::info("StateHierarchy::findMissingStateIds() - Found " + std::to_string(missingIds.size()) +
                 " missing state IDs");
    return missingIds;
}

void StateHierarchy::printHierarchy() const {
    SCXML::Common::Logger::info("StateHierarchy::printHierarchy() - Printing state hierarchy");

    SCXML::Common::Logger::info("State Hierarchy:");
    SCXML::Common::Logger::info("===============");

    if (rootState_) {
        printStateHierarchy(rootState_.get(), 0);
    } else {
        SCXML::Common::Logger::info("  <No root state>");
    }

    SCXML::Common::Logger::info("StateHierarchy::printHierarchy() - State hierarchy printed");
}

void StateHierarchy::printStateHierarchy(Model::IStateNode *state, int depth) const {
    if (!state) {
        return;
    }

    // 들여쓰기 생성
    std::string indent(static_cast<std::size_t>(depth * 2), ' ');

    // 현재 상태 정보 출력
    std::string stateInfo = indent + "State: " + state->getId();

    // 상태 타입 출력
    switch (state->getType()) {
    case SCXML::Type::ATOMIC:
        stateInfo += " (atomic)";
        break;
    case SCXML::Type::COMPOUND:
        stateInfo += " (compound)";
        break;
    case SCXML::Type::PARALLEL:
        stateInfo += " (parallel)";
        break;
    case SCXML::Type::FINAL:
        stateInfo += " (final)";
        break;
    case SCXML::Type::HISTORY:
        stateInfo += " (history)";
        break;
    case SCXML::Type::INITIAL:
        stateInfo += " (initial)";
        break;
    }

    // 초기 상태 정보 출력
    if (!state->getInitialState().empty()) {
        stateInfo += " [initial: " + state->getInitialState() + "]";
    }

    SCXML::Common::Logger::info(stateInfo);

    // 전환 정보 출력
    for (const auto &transition : state->getTransitions()) {
        std::string transitionInfo = indent + "  Transition: ";
        transitionInfo += (transition->getEvent().empty() ? "<no event>" : transition->getEvent()) + " -> ";

        const auto &targets = transition->getTargets();
        if (targets.empty()) {
            transitionInfo += "<no target>";
        } else {
            for (size_t i = 0; i < targets.size(); ++i) {
                transitionInfo += targets[i];
                if (i < targets.size() - 1) {
                    transitionInfo += ", ";
                }
            }
        }

        if (!transition->getGuard().empty()) {
            transitionInfo += " [guard: " + transition->getGuard() + "]";
        }

        SCXML::Common::Logger::info(transitionInfo);
    }

    // 자식 상태 재귀적으로 출력
    for (const auto &child : state->getChildren()) {
        printStateHierarchy(child.get(), depth + 1);
    }
}

}  // namespace SCXML
