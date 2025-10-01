#include "model/SCXMLModel.h"
#include "common/Logger.h"
#include "model/ITransitionNode.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>

RSM::SCXMLModel::SCXMLModel() : rootState_(nullptr) {
    LOG_DEBUG("Creating SCXML model");
}

RSM::SCXMLModel::~SCXMLModel() {
    LOG_DEBUG("Destroying SCXML model");
    // Smart pointers handle resource cleanup
}

void RSM::SCXMLModel::setRootState(std::shared_ptr<RSM::IStateNode> rootState) {
    LOG_DEBUG("Setting root state: {}", (rootState ? rootState->getId() : "null"));
    rootState_ = rootState;
    // Rebuild the complete state list to include all nested children
    rebuildAllStatesList();
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
    LOG_DEBUG("Setting initial state: {}", initialState);
    initialState_ = initialState;
}

const std::string &RSM::SCXMLModel::getInitialState() const {
    return initialState_;
}

void RSM::SCXMLModel::setDatamodel(const std::string &datamodel) {
    LOG_DEBUG("Setting datamodel: {}", datamodel);
    datamodel_ = datamodel;
}

const std::string &RSM::SCXMLModel::getDatamodel() const {
    return datamodel_;
}

void RSM::SCXMLModel::addContextProperty(const std::string &name, const std::string &type) {
    LOG_DEBUG("Adding context property: {} ({})", name, type);
    contextProperties_[name] = type;
}

const std::unordered_map<std::string, std::string> &RSM::SCXMLModel::getContextProperties() const {
    return contextProperties_;
}

void RSM::SCXMLModel::addInjectPoint(const std::string &name, const std::string &type) {
    LOG_DEBUG("Adding inject point: {} ({})", name, type);
    injectPoints_[name] = type;
}

const std::unordered_map<std::string, std::string> &RSM::SCXMLModel::getInjectPoints() const {
    return injectPoints_;
}

void RSM::SCXMLModel::addGuard(std::shared_ptr<RSM::IGuardNode> guard) {
    if (guard) {
        LOG_DEBUG("Adding guard: {}", guard->getId());
        guards_.push_back(guard);
    }
}

const std::vector<std::shared_ptr<RSM::IGuardNode>> &RSM::SCXMLModel::getGuards() const {
    return guards_;
}

void RSM::SCXMLModel::addState(std::shared_ptr<RSM::IStateNode> state) {
    if (state) {
        LOG_DEBUG("Adding state: {}", state->getId());
        allStates_.push_back(state);
        stateIdMap_[state->getId()] = state.get();
        // Rebuild the complete state list to include all nested children
        rebuildAllStatesList();
    }
}

const std::vector<std::shared_ptr<RSM::IStateNode>> &RSM::SCXMLModel::getAllStates() const {
    return allStates_;
}

RSM::IStateNode *RSM::SCXMLModel::findStateById(const std::string &id) const {
    // Search in map first
    auto it = stateIdMap_.find(id);
    if (it != stateIdMap_.end()) {
        return it->second;
    }

    // If not in map, search all top-level states
    std::set<std::string> visitedStates;  // Track already visited state IDs
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

    // Skip already visited states
    if (visitedStates.find(state->getId()) != visitedStates.end()) {
        return nullptr;
    }

    visitedStates.insert(state->getId());

    // Check current state
    if (state->getId() == id) {
        return state;
    }

    // Search child states
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
        LOG_DEBUG("Adding data model item: {}", dataItem->getId());
        dataModelItems_.push_back(dataItem);
    }
}

const std::vector<std::shared_ptr<RSM::IDataModelItem>> &RSM::SCXMLModel::getDataModelItems() const {
    return dataModelItems_;
}

bool RSM::SCXMLModel::validateStateRelationships() const {
    LOG_INFO("Validating state relationships");

    // Validate all states
    for (const auto &state : allStates_) {
        // Validate parent state
        RSM::IStateNode *parent = state->getParent();
        if (parent) {
            // Check if parent actually has this state as a child
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

        // Check if target states of all transitions exist
        for (const auto &transition : state->getTransitions()) {
            const auto targets = transition->getTargets();
            for (const auto &target : targets) {
                RSM::IStateNode *targetState = findStateById(target);
                if (!targetState) {
                    LOG_ERROR("Transition in state '{}' references non-existent target state '{}'", state->getId(),
                              target);
                    return false;
                }
            }
        }

        // Check if initial state exists
        if (!state->getInitialState().empty()) {
            if (state->getChildren().empty()) {
                LOG_WARN("State '{}' has initialState but no children", state->getId());
            } else {
                bool initialStateExists = false;
                for (const auto &child : state->getChildren()) {
                    if (child->getId() == state->getInitialState()) {
                        initialStateExists = true;
                        break;
                    }
                }

                if (!initialStateExists) {
                    LOG_ERROR("State '{}' references non-existent initial state '{}'", state->getId(),
                              state->getInitialState());
                    return false;
                }
            }
        }
    }

    LOG_INFO("All state relationships are valid");
    return true;
}

std::vector<std::string> RSM::SCXMLModel::findMissingStateIds() const {
    LOG_INFO("Looking for missing state IDs");

    std::vector<std::string> missingIds;
    std::unordered_set<std::string> existingIds;

    // Collect all state IDs
    for (const auto &state : allStates_) {
        existingIds.insert(state->getId());
    }

    // Check referenced state IDs
    for (const auto &state : allStates_) {
        // Check initial state
        if (!state->getInitialState().empty() && existingIds.find(state->getInitialState()) == existingIds.end()) {
            missingIds.push_back(state->getInitialState());
            LOG_WARN("Missing state ID referenced as initial state: {}", state->getInitialState());
        }

        // Check transition targets
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

std::set<std::string> RSM::SCXMLModel::getDataModelVariableNames() const {
    std::set<std::string> variableNames;

    for (const auto &dataItem : dataModelItems_) {
        if (dataItem) {
            variableNames.insert(dataItem->getId());
        }
    }

    return variableNames;
}

void RSM::SCXMLModel::printModelStructure() const {
    LOG_INFO("Printing model structure");
    LOG_INFO("SCXML Model Structure:\n");
    LOG_INFO("======================\n");
    LOG_INFO("Initial State: {}", initialState_);
    LOG_INFO("Datamodel: {}", datamodel_);

    LOG_INFO("Context Properties:\n");
    for (const auto &[name, type] : contextProperties_) {
        LOG_INFO("  {}: {}", name, type);
    }

    LOG_INFO("\nInject Points:\n");
    for (const auto &[name, type] : injectPoints_) {
        LOG_INFO("  {}: {}", name, type);
    }

    LOG_INFO("\nGuards:\n");
    for (const auto &guard : guards_) {
        LOG_INFO("  {}:", guard->getId());

        if (!guard->getCondition().empty()) {
            LOG_INFO("    Condition: {}", guard->getCondition());
        }

        if (!guard->getTargetState().empty()) {
            LOG_INFO("    Target State: {}", guard->getTargetState());
        }

        LOG_INFO("    Dependencies:\n");
        for (const auto &dep : guard->getDependencies()) {
            LOG_INFO("      {}", dep);
        }

        if (!guard->getExternalClass().empty()) {
            LOG_INFO("    External Class: {}", guard->getExternalClass());
        }

        if (guard->isReactive()) {
            LOG_INFO("    Reactive: Yes");
        }
    }

    LOG_INFO("\nState Hierarchy:\n");
    if (rootState_) {
        printStateHierarchy(rootState_.get(), 0);
    }

    LOG_INFO("Model structure printed");
}

void RSM::SCXMLModel::printStateHierarchy(RSM::IStateNode *state, int depth) const {
    if (!state) {
        return;
    }

    // 들여쓰기 생성
    std::string indent(depth * 2, ' ');

    // 현재 상태 정보 출력
    LOG_INFO("{}State: {}", indent, state->getId());

    // 자식 상태 재귀적으로 출력
    for (const auto &child : state->getChildren()) {
        printStateHierarchy(child.get(), depth + 1);
    }
}

void RSM::SCXMLModel::setBinding(const std::string &binding) {
    LOG_DEBUG("Setting binding mode: {}", binding);
    binding_ = binding;
}

const std::string &RSM::SCXMLModel::getBinding() const {
    return binding_;
}

void RSM::SCXMLModel::addSystemVariable(std::shared_ptr<RSM::IDataModelItem> systemVar) {
    if (systemVar) {
        LOG_DEBUG("Adding system variable: {}", systemVar->getId());
        systemVariables_.push_back(systemVar);
    }
}

const std::vector<std::shared_ptr<RSM::IDataModelItem>> &RSM::SCXMLModel::getSystemVariables() const {
    return systemVariables_;
}

void RSM::SCXMLModel::collectAllStatesRecursively(IStateNode *state,
                                                  std::vector<std::shared_ptr<IStateNode>> &allStates) const {
    if (!state) {
        return;
    }

    // Find the shared_ptr version of this raw pointer from the root states
    bool found = false;
    for (const auto &sharedState : allStates_) {
        if (sharedState.get() == state) {
            allStates.push_back(sharedState);
            found = true;
            break;
        }
    }

    // If not found in root states, recursively search in already collected states
    if (!found) {
        for (const auto &sharedState : allStates) {
            const auto &children = sharedState->getChildren();
            for (const auto &child : children) {
                if (child.get() == state) {
                    allStates.push_back(child);
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
    }

    // Recursively collect children
    const auto &children = state->getChildren();
    for (const auto &child : children) {
        collectAllStatesRecursively(child.get(), allStates);
    }
}

void RSM::SCXMLModel::rebuildAllStatesList() {
    std::vector<std::shared_ptr<IStateNode>> newAllStates;

    // Start from root state if available
    if (rootState_) {
        collectAllStatesRecursively(rootState_.get(), newAllStates);
    }

    // Also add any states that were explicitly added but might not be in the hierarchy
    for (const auto &state : allStates_) {
        bool alreadyIncluded = false;
        for (const auto &existingState : newAllStates) {
            if (existingState.get() == state.get()) {
                alreadyIncluded = true;
                break;
            }
        }
        if (!alreadyIncluded) {
            newAllStates.push_back(state);
            // Also recursively add their children
            collectAllStatesRecursively(state.get(), newAllStates);
        }
    }

    // Replace the current allStates_ with the complete list
    allStates_ = std::move(newAllStates);

    // Rebuild the state ID map as well
    stateIdMap_.clear();
    for (const auto &state : allStates_) {
        stateIdMap_[state->getId()] = state.get();
    }
}
