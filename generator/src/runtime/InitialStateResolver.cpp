#include "runtime/InitialStateResolver.h"
#include "model/DocumentModel.h"
#include "model/IStateNode.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <sstream>

namespace SCXML {

Core::InitialStateResolver::InitialStateResolver() {}

Core::InitialStateResolver::InitialConfiguration
Core::InitialStateResolver::resolveInitialStates(std::shared_ptr<Model::DocumentModel> model,
                                                 Runtime::RuntimeContext &context) {
    (void)context;  // Suppress unused parameter warning

    clearState();
    InitialConfiguration config;

    if (!model) {
        config.errorMessage = "Null SCXML model provided";
        return config;
    }

    SCXML::Model::IStateNode *rootState = model->getRootState();
    if (!rootState) {
        config.errorMessage = "No root state found in SCXML model";
        return config;
    }

    try {
        // Start with SCXML document's initial attribute
        std::string documentInitial = model->getInitialState();

        if (!documentInitial.empty()) {
            // Document specifies explicit initial state
            SCXML::Model::IStateNode *initialState = model->findStateById(documentInitial);
            if (!initialState) {
                config.errorMessage = "Initial state '" + documentInitial + "' not found";
                return config;
            }

            // Add the initial state and its ancestors
            std::set<std::string> visited;
            addToConfiguration(documentInitial, config, visited);

            // Add proper ancestors to ensure valid configuration
            auto ancestors = getProperAncestors(model, documentInitial);
            for (const auto &ancestor : ancestors) {
                addToConfiguration(ancestor, config, visited);
            }

        } else {
            // No explicit initial state, resolve from root
            if (isCompoundState(*rootState)) {
                resolveCompoundStateInternal(model, *rootState, config);
            } else if (isParallelState(*rootState)) {
                resolveParallelState(model, *rootState, config);
            } else if (isAtomicState(*rootState)) {
                resolveAtomicState(model, *rootState, config);
            }
        }

        // Ensure we have at least one active state
        if (config.activeStates.empty()) {
            // Fallback: use root state if it's atomic, or find first atomic descendant
            if (isAtomicState(*rootState)) {
                config.activeStates.push_back(rootState->getId());
                config.entryOrder.push_back(rootState->getId());
            } else {
                config.errorMessage = "No atomic state found for initial configuration";
                return config;
            }
        }

        // Validate the configuration
        if (!validateConfiguration(model, config)) {
            config.errorMessage = "Invalid initial state configuration";
            config.activeStates.clear();
            config.entryOrder.clear();
            return config;
        }

        config.success = true;
        return config;

    } catch (const std::exception &e) {
        config.errorMessage = "Exception during initial state resolution: " + std::string(e.what());
        config.activeStates.clear();
        config.entryOrder.clear();
        return config;
    }
}

Core::InitialStateResolver::InitialConfiguration
Core::InitialStateResolver::resolveCompoundState(std::shared_ptr<Model::DocumentModel> model,
                                                 const std::string &stateId, Runtime::RuntimeContext &context) {
    (void)context;  // Suppress unused parameter warning

    clearState();
    InitialConfiguration config;

    if (!model) {
        config.errorMessage = "Null SCXML model provided";
        return config;
    }

    SCXML::Model::IStateNode *stateNode = model->findStateById(stateId);
    if (!stateNode) {
        config.errorMessage = "State '" + stateId + "' not found";
        return config;
    }

    if (isCompoundState(*stateNode)) {
        resolveCompoundStateInternal(model, *stateNode, config);
    } else if (isParallelState(*stateNode)) {
        resolveParallelState(model, *stateNode, config);
    } else {
        resolveAtomicState(model, *stateNode, config);
    }

    config.success = !config.activeStates.empty() && validateConfiguration(model, config);
    return config;
}

bool Core::InitialStateResolver::needsInitialResolution(std::shared_ptr<Model::DocumentModel> model,
                                                        const std::string &stateId) const {
    if (!model) {
        return false;
    }

    SCXML::Model::IStateNode *stateNode = model->findStateById(stateId);

    return isCompoundState(*stateNode) || isParallelState(*stateNode);
}

bool Core::InitialStateResolver::validateConfiguration(std::shared_ptr<Model::DocumentModel> model,
                                                       const InitialConfiguration &config) const {
    if (!model || config.activeStates.empty()) {
        return false;
    }

    // Check that all states in configuration exist
    for (const auto &stateId : config.activeStates) {
        if (!model->findStateById(stateId)) {
            return false;
        }
    }

    // Check that entry order contains all active states
    std::set<std::string> activeSet(config.activeStates.begin(), config.activeStates.end());
    std::set<std::string> entrySet(config.entryOrder.begin(), config.entryOrder.end());

    return activeSet == entrySet;
}

// ========== Protected Methods ==========

void Core::InitialStateResolver::resolveAtomicState(std::shared_ptr<Model::DocumentModel> model,
                                                    SCXML::Model::IStateNode &stateNode, InitialConfiguration &config) {
    (void)model;  // Suppress unused parameter warning
    std::set<std::string> visited;
    addToConfiguration(stateNode.getId(), config, visited);
}

void Core::InitialStateResolver::resolveCompoundStateInternal(std::shared_ptr<Model::DocumentModel> model,
                                                              SCXML::Model::IStateNode &stateNode,
                                                              InitialConfiguration &config) {
    std::set<std::string> visited;

    // Add the compound state itself to configuration
    addToConfiguration(stateNode.getId(), config, visited);

    // Find the initial child state
    std::string initialChild = getEffectiveInitial(stateNode);

    if (initialChild.empty()) {
        // No explicit initial state, use default (first child)
        initialChild = getDefaultInitial(stateNode);
    }

    if (!initialChild.empty()) {
        SCXML::Model::IStateNode *childState = model->findStateById(initialChild);
        if (childState) {
            // Recursively resolve the child state
            if (isCompoundState(*childState)) {
                resolveCompoundStateInternal(model, *childState, config);
            } else if (isParallelState(*childState)) {
                resolveParallelState(model, *childState, config);
            } else {
                resolveAtomicState(model, *childState, config);
            }
        }
    }
}

void Core::InitialStateResolver::resolveParallelState(std::shared_ptr<Model::DocumentModel> model,
                                                      SCXML::Model::IStateNode &stateNode,
                                                      InitialConfiguration &config) {
    std::set<std::string> visited;

    // Add the parallel state itself to configuration
    addToConfiguration(stateNode.getId(), config, visited);

    // For parallel states, all child regions must be active
    // Enhanced parallel state support - enter all child regions
    if (isParallelState(stateNode)) {
        // Get all child regions/states for parallel execution
        auto children = stateNode.getChildren();
        for (const auto &child : children) {
            if (!child) {
                continue;
            }
            SCXML::Model::IStateNode *childState = child.get();
            if (childState) {
                config.activeStates.push_back(childState->getId());
                if (isCompoundState(*childState)) {
                    resolveCompoundStateInternal(model, *childState, config);
                }
            }
        }
        return;  // Skip the single child handling below
    }

    // For compound states, handle first/default child

    std::string firstChild = getDefaultInitial(stateNode);
    if (!firstChild.empty()) {
        SCXML::Model::IStateNode *childState = model->findStateById(firstChild);
        if (childState) {
            if (isCompoundState(*childState)) {
                resolveCompoundStateInternal(model, *childState, config);
            } else if (isParallelState(*childState)) {
                resolveParallelState(model, *childState, config);
            } else {
                resolveAtomicState(model, *childState, config);
            }
        }
    }
}

std::string Core::InitialStateResolver::getEffectiveInitial(SCXML::Model::IStateNode &stateNode) const {
    // Use the actual getInitialState() method from IStateNode interface
    return stateNode.getInitialState();
}

std::string Core::InitialStateResolver::getDefaultInitial(SCXML::Model::IStateNode &stateNode) const {
    // Get child states from IStateNode and return first child's ID
    const auto &children = stateNode.getChildren();
    if (!children.empty()) {
        return children[0]->getId();
    }

    // If no children, return empty string
    return "";
}

void Core::InitialStateResolver::addToConfiguration(const std::string &stateId, InitialConfiguration &config,
                                                    std::set<std::string> &visited) {
    if (stateId.empty() || visited.count(stateId)) {
        return;  // Avoid cycles and empty states
    }

    visited.insert(stateId);

    // Add to active states if not already present
    if (std::find(config.activeStates.begin(), config.activeStates.end(), stateId) == config.activeStates.end()) {
        config.activeStates.push_back(stateId);
    }

    // Add to entry order if not already present
    if (std::find(config.entryOrder.begin(), config.entryOrder.end(), stateId) == config.entryOrder.end()) {
        config.entryOrder.push_back(stateId);
    }
}

std::vector<std::string> Core::InitialStateResolver::getProperAncestors(std::shared_ptr<Model::DocumentModel> model,
                                                                        const std::string &stateId) const {
    (void)stateId;  // Suppress unused parameter warning

    std::vector<std::string> ancestors;

    if (!model) {
        return ancestors;
    }

    // Note: State hierarchy implementation requires complex parent-child relationship tracking
    // For now, return empty vector
    // In a complete implementation, this would traverse up the state hierarchy

    return ancestors;
}

bool Core::InitialStateResolver::isCompoundState(SCXML::Model::IStateNode &stateNode) const {
    // Use the actual getType() method to check if it's COMPOUND
    return stateNode.getType() == Type::COMPOUND;
}

bool Core::InitialStateResolver::isParallelState(const SCXML::Model::IStateNode &stateNode) const {
    // Use the actual getType() method to check if it's PARALLEL
    return stateNode.getType() == Type::PARALLEL;
}

bool Core::InitialStateResolver::isAtomicState(const SCXML::Model::IStateNode &stateNode) const {
    // Use the actual getType() method to check if it's ATOMIC
    return stateNode.getType() == Type::ATOMIC;
}

bool Core::InitialStateResolver::isFinalState(const SCXML::Model::IStateNode &stateNode) const {
    // Use the actual isFinalState() method from IStateNode interface
    return stateNode.isFinalState();
}

// ========== Private Methods ==========

void Core::InitialStateResolver::clearState() {
    resolvedStates_.clear();
    errorMessages_.clear();
}

void Core::InitialStateResolver::addError(const std::string &message) {
    errorMessages_.push_back(message);
}

}  // namespace SCXML