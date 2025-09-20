#include "model/SCXMLModel.h"
#include "common/Logger.h"
#include "model/ITransitionNode.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>

RSM::SCXMLModel::SCXMLModel() : rootState_(nullptr) {
    Logger::debug("RSM::SCXMLModel::Constructor - Creating SCXML model");
}

RSM::SCXMLModel::~SCXMLModel() {
    Logger::debug("RSM::SCXMLModel::Destructor - Destroying SCXML model");
    // 스마트 포인터가 자원 정리를 담당
}

void RSM::SCXMLModel::setRootState(std::shared_ptr<RSM::IStateNode> rootState) {
    Logger::debug("RSM::SCXMLModel::setRootState() - Setting root state: " + (rootState ? rootState->getId() : "null"));
    rootState_ = rootState;
}

std::shared_ptr<RSM::IStateNode> RSM::SCXMLModel::getRootState() const {
    return rootState_;
}

void RSM::SCXMLModel::setName(const std::string &name) {
    name_ = name;
}

const std::string &RSM::SCXMLModel::getName() const {
    return name_;
}

void RSM::SCXMLModel::setInitialState(const std::string &initialState) {
    Logger::debug("RSM::SCXMLModel::setInitialState() - Setting initial state: " + initialState);
    initialState_ = initialState;
}

const std::string &RSM::SCXMLModel::getInitialState() const {
    return initialState_;
}

void RSM::SCXMLModel::setDatamodel(const std::string &datamodel) {
    Logger::debug("RSM::SCXMLModel::setDatamodel() - Setting datamodel: " + datamodel);
    datamodel_ = datamodel;
}

const std::string &RSM::SCXMLModel::getDatamodel() const {
    return datamodel_;
}

void RSM::SCXMLModel::addContextProperty(const std::string &name, const std::string &type) {
    Logger::debug("RSM::SCXMLModel::addContextProperty() - Adding context property: " + name + " (" + type + ")");
    contextProperties_[name] = type;
}

const std::unordered_map<std::string, std::string> &RSM::SCXMLModel::getContextProperties() const {
    return contextProperties_;
}

void RSM::SCXMLModel::addInjectPoint(const std::string &name, const std::string &type) {
    Logger::debug("RSM::SCXMLModel::addInjectPoint() - Adding inject point: " + name + " (" + type + ")");
    injectPoints_[name] = type;
}

const std::unordered_map<std::string, std::string> &RSM::SCXMLModel::getInjectPoints() const {
    return injectPoints_;
}

void RSM::SCXMLModel::addGuard(std::shared_ptr<RSM::IGuardNode> guard) {
    if (guard) {
        Logger::debug("RSM::SCXMLModel::addGuard() - Adding guard: " + guard->getId());
        guards_.push_back(guard);
    }
}

const std::vector<std::shared_ptr<RSM::IGuardNode>> &RSM::SCXMLModel::getGuards() const {
    return guards_;
}

void RSM::SCXMLModel::addState(std::shared_ptr<RSM::IStateNode> state) {
    if (state) {
        Logger::debug("RSM::SCXMLModel::addState() - Adding state: " + state->getId());
        allStates_.push_back(state);
        stateIdMap_[state->getId()] = state.get();
    }
}

const std::vector<std::shared_ptr<RSM::IStateNode>> &RSM::SCXMLModel::getAllStates() const {
    return allStates_;
}

RSM::IStateNode *RSM::SCXMLModel::findStateById(const std::string &id) const {
    // 먼저 맵에서 찾기
    auto it = stateIdMap_.find(id);
    if (it != stateIdMap_.end()) {
        return it->second;
    }

    // 맵에 없으면 모든 최상위 상태를 검색
    std::set<std::string> visitedStates;  // 이미 방문한 상태 ID 추적
    for (const auto &state : allStates_) {
        if (state->getId() == id) {
            return state.get();
        }

        RSM::IStateNode *result = findStateByIdRecursive(state.get(), id, visitedStates);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

RSM::IStateNode *RSM::SCXMLModel::findStateByIdRecursive(RSM::IStateNode *state, const std::string &id,
                                                         std::set<std::string> &visitedStates) const {
    if (!state) {
        return nullptr;
    }

    // 이미 방문한 상태는 건너뛰기
    if (visitedStates.find(state->getId()) != visitedStates.end()) {
        return nullptr;
    }

    visitedStates.insert(state->getId());

    // 현재 상태 확인
    if (state->getId() == id) {
        return state;
    }

    // 자식 상태 검색
    for (const auto &child : state->getChildren()) {
        RSM::IStateNode *result = findStateByIdRecursive(child.get(), id, visitedStates);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

void RSM::SCXMLModel::addDataModelItem(std::shared_ptr<RSM::IDataModelItem> dataItem) {
    if (dataItem) {
        Logger::debug("RSM::SCXMLModel::addDataModelItem() - Adding data model item: " + dataItem->getId());
        dataModelItems_.push_back(dataItem);
    }
}

const std::vector<std::shared_ptr<RSM::IDataModelItem>> &RSM::SCXMLModel::getDataModelItems() const {
    return dataModelItems_;
}

bool RSM::SCXMLModel::validateStateRelationships() const {
    Logger::info("RSM::SCXMLModel::validateStateRelationships() - Validating state "
                 "relationships");

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
                Logger::error("RSM::SCXMLModel::validateStateRelationships() - State '" + state->getId() +
                              "' has parent '" + parent->getId() + "' but is not in parent's children list");
                return false;
            }
        }

        // 모든 전환의 타겟 상태가 존재하는지 확인
        for (const auto &transition : state->getTransitions()) {
            const auto targets = transition->getTargets();
            for (const auto &target : targets) {
                RSM::IStateNode *targetState = findStateById(target);
                if (!targetState) {
                    Logger::error("RSM::SCXMLModel::validateStateRelationships() - Transition "
                                  "in state '" +
                                  state->getId() + "' references non-existent target state '" + target + "'");
                    return false;
                }
            }
        }

        // 초기 상태가 존재하는지 확인
        if (!state->getInitialState().empty()) {
            if (state->getChildren().empty()) {
                Logger::warn("RSM::SCXMLModel::validateStateRelationships() - State '" + state->getId() +
                             "' has initialState but no children");
            } else {
                bool initialStateExists = false;
                for (const auto &child : state->getChildren()) {
                    if (child->getId() == state->getInitialState()) {
                        initialStateExists = true;
                        break;
                    }
                }

                if (!initialStateExists) {
                    Logger::error("RSM::SCXMLModel::validateStateRelationships() - State '" + state->getId() +
                                  "' references non-existent initial state '" + state->getInitialState() + "'");
                    return false;
                }
            }
        }
    }

    Logger::info("RSM::SCXMLModel::validateStateRelationships() - All state "
                 "relationships are valid");
    return true;
}

std::vector<std::string> RSM::SCXMLModel::findMissingStateIds() const {
    Logger::info("RSM::SCXMLModel::findMissingStateIds() - Looking for missing state IDs");

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
            Logger::warn("RSM::SCXMLModel::findMissingStateIds() - Missing state ID "
                         "referenced as initial state: " +
                         state->getInitialState());
        }

        // 전환 타겟 확인
        for (const auto &transition : state->getTransitions()) {
            const auto targets = transition->getTargets();
            for (const auto &target : targets) {
                if (!target.empty() && existingIds.find(target) == existingIds.end()) {
                    missingIds.push_back(target);
                    Logger::warn("RSM::SCXMLModel::findMissingStateIds() - Missing state ID "
                                 "referenced as transition target: " +
                                 target);
                }
            }
        }
    }

    // 중복 제거
    std::sort(missingIds.begin(), missingIds.end());
    missingIds.erase(std::unique(missingIds.begin(), missingIds.end()), missingIds.end());

    Logger::info("RSM::SCXMLModel::findMissingStateIds() - Found " + std::to_string(missingIds.size()) +
                 " missing state IDs");
    return missingIds;
}

void RSM::SCXMLModel::printModelStructure() const {
    Logger::info("RSM::SCXMLModel::printModelStructure() - Printing model structure");
    Logger::info("SCXML Model Structure:\n");
    Logger::info("======================\n");
    Logger::info("Initial State: " + initialState_);
    Logger::info("Datamodel: " + datamodel_);

    Logger::info("Context Properties:\n");
    for (const auto &[name, type] : contextProperties_) {
        Logger::info("  " + name + ": " + type);
    }

    Logger::info("\nInject Points:\n");
    for (const auto &[name, type] : injectPoints_) {
        Logger::info("  " + name + ": " + type);
    }

    Logger::info("\nGuards:\n");
    for (const auto &guard : guards_) {
        Logger::info("  " + guard->getId() + ":");

        if (!guard->getCondition().empty()) {
            Logger::info("    Condition: " + guard->getCondition());
        }

        if (!guard->getTargetState().empty()) {
            Logger::info("    Target State: " + guard->getTargetState());
        }

        Logger::info("    Dependencies:\n");
        for (const auto &dep : guard->getDependencies()) {
            Logger::info("      " + dep);
        }

        if (!guard->getExternalClass().empty()) {
            Logger::info("    External Class: " + guard->getExternalClass());
        }

        if (guard->isReactive()) {
            Logger::info("    Reactive: Yes");
        }
    }

    Logger::info("\nState Hierarchy:\n");
    if (rootState_) {
        printStateHierarchy(rootState_.get(), 0);
    }

    Logger::info("RSM::SCXMLModel::printModelStructure() - Model structure printed");
}

void RSM::SCXMLModel::printStateHierarchy(RSM::IStateNode *state, int depth) const {
    if (!state) {
        return;
    }

    // 들여쓰기 생성
    std::string indent(depth * 2, ' ');

    // 현재 상태 정보 출력
    Logger::info(indent + "State: " + state->getId());

    // 자식 상태 재귀적으로 출력
    for (const auto &child : state->getChildren()) {
        printStateHierarchy(child.get(), depth + 1);
    }
}

void RSM::SCXMLModel::setBinding(const std::string &binding) {
    Logger::debug("RSM::SCXMLModel::setBinding() - Setting binding mode: " + binding);
    binding_ = binding;
}

const std::string &RSM::SCXMLModel::getBinding() const {
    return binding_;
}

void RSM::SCXMLModel::addSystemVariable(std::shared_ptr<RSM::IDataModelItem> systemVar) {
    if (systemVar) {
        Logger::debug("RSM::SCXMLModel::addSystemVariable() - Adding system variable: " + systemVar->getId());
        systemVariables_.push_back(systemVar);
    }
}

const std::vector<std::shared_ptr<RSM::IDataModelItem>> &RSM::SCXMLModel::getSystemVariables() const {
    return systemVariables_;
}
