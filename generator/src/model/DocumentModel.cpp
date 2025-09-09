#include "model/DocumentModel.h"
#include "common/Logger.h"
#include "model/DocumentModel.h"
#include "model/ITransitionNode.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace SCXML {
namespace Model {

DocumentModel::DocumentModel() : rootState_(nullptr) {
    SCXML::Common::Logger::debug("DocumentModel::Constructor - Creating SCXML model");
}

DocumentModel::~DocumentModel() {
    SCXML::Common::Logger::debug("DocumentModel::Destructor - Destroying SCXML model");
    // 스마트 포인터가 자원 정리를 담당
}

void DocumentModel::setRootState(std::shared_ptr<SCXML::Model::IStateNode> rootState) {
    SCXML::Common::Logger::debug("DocumentModel::setRootState() - Setting root state: " +
                                 (rootState ? rootState->getId() : "null"));
    rootState_ = rootState;
}

SCXML::Model::IStateNode *DocumentModel::getRootState() const {
    return rootState_.get();
}

void DocumentModel::setName(const std::string &name) {
    name_ = name;
}

const std::string &DocumentModel::getName() const {
    return name_;
}

void DocumentModel::setInitialState(const std::string &initialState) {
    SCXML::Common::Logger::debug("DocumentModel::setInitialState() - Setting initial state: " + initialState);
    initialState_ = initialState;
}

const std::string &DocumentModel::getInitialState() const {
    return initialState_;
}

void DocumentModel::setDatamodel(const std::string &datamodel) {
    SCXML::Common::Logger::debug("DocumentModel::setDatamodel() - Setting datamodel: " + datamodel);
    datamodel_ = datamodel;
}

const std::string &DocumentModel::getDatamodel() const {
    return datamodel_;
}

void DocumentModel::addContextProperty(const std::string &name, const std::string &type) {
    SCXML::Common::Logger::debug("DocumentModel::addContextProperty() - Adding context property: " + name + " (" +
                                 type + ")");
    contextProperties_[name] = type;
}

const std::unordered_map<std::string, std::string> &DocumentModel::getContextProperties() const {
    return contextProperties_;
}

void DocumentModel::addInjectPoint(const std::string &name, const std::string &type) {
    SCXML::Common::Logger::debug("DocumentModel::addInjectPoint() - Adding inject point: " + name + " (" + type + ")");
    injectPoints_[name] = type;
}

const std::unordered_map<std::string, std::string> &DocumentModel::getInjectPoints() const {
    return injectPoints_;
}

void DocumentModel::addGuard(std::shared_ptr<SCXML::Model::IGuardNode> guard) {
    if (guard) {
        SCXML::Common::Logger::debug("DocumentModel::addGuard() - Adding guard: " + guard->getId());
        guards_.push_back(guard);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IGuardNode>> &DocumentModel::getGuards() const {
    return guards_;
}

void DocumentModel::addState(std::shared_ptr<SCXML::Model::IStateNode> state) {
    if (state) {
        SCXML::Common::Logger::debug("DocumentModel::addState() - Adding state: " + state->getId());
        allStates_.push_back(state);
        stateIdMap_[state->getId()] = state.get();
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IStateNode>> &DocumentModel::getAllStates() const {
    return allStates_;
}

SCXML::Model::IStateNode *DocumentModel::findStateById(const std::string &id) const {
    // 먼저 맵에서 찾기 (가장 빠른 경로)
    auto it = stateIdMap_.find(id);
    if (it != stateIdMap_.end()) {
        return it->second;
    }

    // Check for dotted notation (e.g., "player.stopped")
    size_t dotPos = id.find('.');
    if (dotPos != std::string::npos) {
        // Split into parent and child parts
        std::string parentId = id.substr(0, dotPos);
        std::string childPath = id.substr(dotPos + 1);

        // Find parent state first
        SCXML::Model::IStateNode *parentState = findStateById(parentId);
        if (!parentState) {
            // If parent not found, try to find the full dotted ID as a direct state
            // This handles cases where state IDs actually contain dots
            std::set<std::string> visitedStates;
            for (const auto &state : allStates_) {
                if (state->getId() == id) {
                    return state.get();
                }

                SCXML::Model::IStateNode *result = findStateByIdRecursive(state.get(), id, visitedStates);
                if (result) {
                    return result;
                }
            }
            return nullptr;
        }

        // Search for child state within parent using remaining path
        return findStateInHierarchy(parentState, childPath);
    }

    // 맵에 없으면 모든 최상위 상태를 검색
    std::set<std::string> visitedStates;  // 이미 방문한 상태 ID 추적
    for (const auto &state : allStates_) {
        if (state->getId() == id) {
            return state.get();
        }

        SCXML::Model::IStateNode *result = findStateByIdRecursive(state.get(), id, visitedStates);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

SCXML::Model::IStateNode *DocumentModel::findStateByIdRecursive(SCXML::Model::IStateNode *state, const std::string &id,
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
        SCXML::Model::IStateNode *result = findStateByIdRecursive(child.get(), id, visitedStates);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

SCXML::Model::IStateNode *DocumentModel::findStateInHierarchy(SCXML::Model::IStateNode *parentState,
                                                              const std::string &childPath) const {
    if (!parentState) {
        return nullptr;
    }

    // Check for another dot in the path (nested hierarchy)
    size_t dotPos = childPath.find('.');
    if (dotPos != std::string::npos) {
        // More nesting - split again
        std::string immediateChild = childPath.substr(0, dotPos);
        std::string remainingPath = childPath.substr(dotPos + 1);

        // Find the immediate child first
        for (const auto &child : parentState->getChildren()) {
            if (child->getId() == immediateChild) {
                // Recursively search in the found child with remaining path
                return findStateInHierarchy(child.get(), remainingPath);
            }
        }
        return nullptr;
    } else {
        // No more dots - find direct child with this ID
        for (const auto &child : parentState->getChildren()) {
            if (child->getId() == childPath) {
                return child.get();
            }
        }
        return nullptr;
    }
}

void DocumentModel::addDataModelItem(std::shared_ptr<SCXML::Model::IDataModelItem> dataItem) {
    if (dataItem) {
        SCXML::Common::Logger::debug("DocumentModel::addDataModelItem() - Adding data model item: " +
                                     dataItem->getId());
        dataModelItems_.push_back(dataItem);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IDataModelItem>> &DocumentModel::getDataModelItems() const {
    return dataModelItems_;
}

bool DocumentModel::validateStateRelationships() const {
    SCXML::Common::Logger::info("DocumentModel::validateStateRelationships() - Validating state relationships");

    // 모든 상태에 대해 검증
    for (const auto &state : allStates_) {
        // 부모 상태 검증
        SCXML::Model::IStateNode *parent = state->getParent();
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
                SCXML::Common::Logger::error("DocumentModel::validateStateRelationships() - State '" + state->getId() +
                                             "' has parent '" + parent->getId() +
                                             "' but is not in parent's children list");
                return false;
            }
        }

        // 모든 전환의 타겟 상태가 존재하는지 확인
        for (const auto &transition : state->getTransitions()) {
            const auto targets = transition->getTargets();
            for (const auto &target : targets) {
                SCXML::Model::IStateNode *targetState = findStateById(target);
                if (!targetState) {
                    SCXML::Common::Logger::error("DocumentModel::validateStateRelationships() - Transition in state '" +
                                                 state->getId() + "' references non-existent target state '" + target +
                                                 "'");
                    return false;
                }
            }
        }

        // 초기 상태가 존재하는지 확인
        if (!state->getInitialState().empty()) {
            if (state->getChildren().empty()) {
                SCXML::Common::Logger::warning("DocumentModel::validateStateRelationships() - State '" +
                                               state->getId() + "' has initialState but no children");
            } else {
                bool initialStateExists = false;
                for (const auto &child : state->getChildren()) {
                    if (child->getId() == state->getInitialState()) {
                        initialStateExists = true;
                        break;
                    }
                }

                if (!initialStateExists) {
                    SCXML::Common::Logger::error("DocumentModel::validateStateRelationships() - State '" +
                                                 state->getId() + "' references non-existent initial state '" +
                                                 state->getInitialState() + "'");
                    return false;
                }
            }
        }
    }

    SCXML::Common::Logger::info("DocumentModel::validateStateRelationships() - All state relationships are valid");
    return true;
}

std::vector<std::string> DocumentModel::findMissingStateIds() const {
    SCXML::Common::Logger::info("DocumentModel::findMissingStateIds() - Looking for missing state IDs");

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
            SCXML::Common::Logger::warning(
                "DocumentModel::findMissingStateIds() - Missing state ID referenced as initial state: " +
                state->getInitialState());
        }

        // 전환 타겟 확인
        for (const auto &transition : state->getTransitions()) {
            const auto targets = transition->getTargets();
            for (const auto &target : targets) {
                if (!target.empty() && existingIds.find(target) == existingIds.end()) {
                    missingIds.push_back(target);
                    SCXML::Common::Logger::warning(
                        "DocumentModel::findMissingStateIds() - Missing state ID referenced as transition target: " +
                        target);
                }
            }
        }
    }

    // 중복 제거
    std::sort(missingIds.begin(), missingIds.end());
    missingIds.erase(std::unique(missingIds.begin(), missingIds.end()), missingIds.end());

    SCXML::Common::Logger::info("DocumentModel::findMissingStateIds() - Found " + std::to_string(missingIds.size()) +
                                " missing state IDs");
    return missingIds;
}

void DocumentModel::printModelStructure() const {
    SCXML::Common::Logger::info("DocumentModel::printModelStructure() - Printing model structure");
    SCXML::Common::Logger::info("SCXML Model Structure:");
    SCXML::Common::Logger::info("======================");
    SCXML::Common::Logger::info("Initial State: " + initialState_);
    SCXML::Common::Logger::info("Datamodel: " + datamodel_);
    SCXML::Common::Logger::info("");

    SCXML::Common::Logger::info("Context Properties:");
    for (const auto &[name, type] : contextProperties_) {
        SCXML::Common::Logger::info("  " + name + ": " + type);
    }

    SCXML::Common::Logger::info("");
    SCXML::Common::Logger::info("Inject Points:");
    for (const auto &[name, type] : injectPoints_) {
        SCXML::Common::Logger::info("  " + name + ": " + type);
    }

    SCXML::Common::Logger::info("");
    SCXML::Common::Logger::info("Guards:");
    for (const auto &guard : guards_) {
        SCXML::Common::Logger::info("  " + guard->getId() + ":");

        if (!guard->getCondition().empty()) {
            SCXML::Common::Logger::info("    Condition: " + guard->getCondition());
        }

        if (!guard->getTargetState().empty()) {
            SCXML::Common::Logger::info("    Target State: " + guard->getTargetState());
        }

        SCXML::Common::Logger::info("    Dependencies:");
        for (const auto &dep : guard->getDependencies()) {
            SCXML::Common::Logger::info("      " + dep);
        }

        if (!guard->getExternalClass().empty()) {
            SCXML::Common::Logger::info("    External Class: " + guard->getExternalClass());
        }

        if (guard->isReactive()) {
            SCXML::Common::Logger::info("    Reactive: Yes");
        }
    }

    SCXML::Common::Logger::info("");
    SCXML::Common::Logger::info("State Hierarchy:");
    if (rootState_) {
        printStateHierarchy(rootState_.get(), 0);
    }

    SCXML::Common::Logger::info("DocumentModel::printModelStructure() - Model structure printed");
}

void DocumentModel::printStateHierarchy(SCXML::Model::IStateNode *state, int depth) const {
    if (!state) {
        return;
    }

    // 들여쓰기 생성
    std::string indent(static_cast<std::size_t>(depth * 2), ' ');

    // 현재 상태 정보 출력
    SCXML::Common::Logger::info(indent + "State: " + state->getId());

    // 자식 상태 재귀적으로 출력
    for (const auto &child : state->getChildren()) {
        printStateHierarchy(child.get(), depth + 1);
    }
}

void DocumentModel::setBinding(const std::string &binding) {
    SCXML::Common::Logger::debug("DocumentModel::setBinding() - Setting binding mode: " + binding);
    binding_ = binding;
}

const std::string &DocumentModel::getBinding() const {
    return binding_;
}

void DocumentModel::addSystemVariable(std::shared_ptr<SCXML::Model::IDataModelItem> systemVar) {
    if (systemVar) {
        SCXML::Common::Logger::debug("DocumentModel::addSystemVariable() - Adding system variable: " +
                                     systemVar->getId());
        systemVariables_.push_back(systemVar);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::IDataModelItem>> &DocumentModel::getSystemVariables() const {
    return systemVariables_;
}

void DocumentModel::addDocumentTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition) {
    if (transition) {
        SCXML::Common::Logger::debug("DocumentModel::addDocumentTransition() - Adding document-level transition");
        documentTransitions_.push_back(transition);
    }
}

const std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>> &DocumentModel::getDocumentTransitions() const {
    return documentTransitions_;
}

}  // namespace Model
}  // namespace SCXML
