#include "model/StateHierarchy.h"
#include "common/Logger.h"
#include "model/ITransitionNode.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stack>
#include <unordered_set>

RSM::StateHierarchy::StateHierarchy() : rootState_(nullptr) {
    LOG_DEBUG("Creating state hierarchy");
}

RSM::StateHierarchy::~StateHierarchy() {
    LOG_DEBUG("Destroying state hierarchy");
    // Smart pointers handle resource cleanup
}

void RSM::StateHierarchy::setRootState(std::shared_ptr<RSM::IStateNode> rootState) {
    LOG_DEBUG("Setting root state: {}", (rootState ? rootState->getId() : "null"));
    rootState_ = rootState;

    if (rootState_) {
        // Add root state to state ID map when set
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

    // If parent ID is specified, find the parent and add as child
    if (!parentId.empty()) {
        RSM::IStateNode *parent = findStateById(parentId);
        if (!parent) {
            LOG_ERROR("Parent state not found: {}", parentId);
            return false;
        }

        // Set parent-child relationship
        state->setParent(parent);
        parent->addChild(state);
    } else if (rootState_ && rootState_.get() != state.get()) {
        // If no parent ID specified but not root, add as child of root
        state->setParent(rootState_.get());
        rootState_->addChild(state);
    }

    // Add to state list and map
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

    // Self is not its own descendant
    if (ancestor == descendant) {
        return false;
    }

    // Check parent-child relationship
    RSM::IStateNode *parent = descendant->getParent();

    // Return false if no parent
    if (!parent) {
        return false;
    }

    // True if direct parent
    if (parent == ancestor) {
        return true;
    }

    // Recursively check ancestors
    return isDescendantOf(ancestor, parent);
}

const std::vector<std::shared_ptr<RSM::IStateNode>> &RSM::StateHierarchy::getAllStates() const {
    return allStates_;
}

bool RSM::StateHierarchy::validateRelationships() const {
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

        // W3C SCXML 3.3: Check if initial state(s) exist - supports space-separated list
        if (!state->getInitialState().empty()) {
            std::istringstream iss(state->getInitialState());
            std::string initialStateId;

            while (iss >> initialStateId) {
                // Search in entire state hierarchy (not just direct children)
                if (!findStateById(initialStateId)) {
                    LOG_ERROR("State '{}' references non-existent initial state '{}'", state->getId(), initialStateId);
                    return false;
                }
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

    // Remove duplicates
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

    // Generate indentation
    std::string indent(depth * 2, ' ');

    // Output current state information
    LOG_INFO("{}State: {}", indent, state->getId());

    // Output state type
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

    // Output initial state information
    if (!state->getInitialState().empty()) {
        LOG_INFO(" [initial: {}]", state->getInitialState());
    }

    // Line break handled by previous Logger::info calls

    // Output transition information
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

    // Recursively output child states
    for (const auto &child : state->getChildren()) {
        printStateHierarchy(child.get(), depth + 1);
    }
}
