#include "runtime/CompoundStateProcessor.h"
#include "model/DocumentModel.h"
#include "model/IStateNode.h"
#include "model/StateHierarchyManager.h"
#include "runtime/InitialStateResolver.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>

namespace SCXML {

CompoundStateProcessor::CompoundStateProcessor(std::shared_ptr<StateHierarchyManager> hierarchyManager,
                                               std::shared_ptr<InitialStateResolver> stateResolver)
    : hierarchyManager_(hierarchyManager), stateResolver_(stateResolver) {}

CompoundStateProcessor::ProcessingResult
CompoundStateProcessor::enterCompoundState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                           const std::string &compoundStateId,
                                           SCXML::Runtime::RuntimeContext &context) {
    clearErrors();
    ProcessingResult result;

    if (!model || compoundStateId.empty()) {
        result.errorMessage = "Invalid model or compound state ID";
        return result;
    }

    if (!isCompoundState(model, compoundStateId)) {
        result.errorMessage = "State '" + compoundStateId + "' is not a compound state";
        return result;
    }

    try {
        enterCompoundStateInternal(model, compoundStateId, context, result);
        result.success = true;
        return result;

    } catch (const std::exception &e) {
        result.errorMessage = "Exception during compound state entry: " + std::string(e.what());
        return result;
    }
}

CompoundStateProcessor::ProcessingResult
CompoundStateProcessor::exitCompoundState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                          const std::string &compoundStateId, SCXML::Runtime::RuntimeContext &context) {
    clearErrors();
    ProcessingResult result;

    if (!model || compoundStateId.empty()) {
        result.errorMessage = "Invalid model or compound state ID";
        return result;
    }

    try {
        exitCompoundStateInternal(model, compoundStateId, context, result);
        result.success = true;
        return result;

    } catch (const std::exception &e) {
        result.errorMessage = "Exception during compound state exit: " + std::string(e.what());
        return result;
    }
}

bool CompoundStateProcessor::isCompoundState(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                             const std::string &stateId) const {
    if (!model || !hierarchyManager_) {
        return false;
    }

    SCXML::Model::IStateNode *stateNode = model->findStateById(stateId);
    if (!stateNode) {
        return false;
    }

    // A state is compound if it has child states
    auto children = hierarchyManager_->getChildren(stateId);
    return !children.empty();
}

std::vector<std::string> CompoundStateProcessor::getActiveDescendants(const std::string &compoundStateId,
                                                                      SCXML::Runtime::RuntimeContext &context) const {
    if (!hierarchyManager_) {
        return {};
    }

    std::vector<std::string> activeDescendants;
    auto activeStates = context.getActiveStates();
    auto allDescendants = hierarchyManager_->getDescendants(compoundStateId);

    // Find intersection of active states and descendants
    for (const auto &descendant : allDescendants) {
        if (std::find(activeStates.begin(), activeStates.end(), descendant) != activeStates.end()) {
            activeDescendants.push_back(descendant);
        }
    }

    return activeDescendants;
}

InitialStateResolver::InitialConfiguration
CompoundStateProcessor::resolveInitialConfiguration(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                    const std::string &compoundStateId,
                                                    SCXML::Runtime::RuntimeContext &context) {
    InitialStateResolver::InitialConfiguration config;

    if (!stateResolver_) {
        config.errorMessage = "No initial state resolver available";
        return config;
    }

    return stateResolver_->resolveCompoundState(model, compoundStateId, context);
}

std::string CompoundStateProcessor::getDefaultChild(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                    const std::string &compoundStateId) const {
    if (!hierarchyManager_) {
        return "";
    }

    auto children = hierarchyManager_->getChildren(compoundStateId);
    return children.empty() ? "" : children[0];
}

std::string CompoundStateProcessor::getInitialChild(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                    const std::string &compoundStateId) const {
    if (!model) {
        return "";
    }

    SCXML::Model::IStateNode *stateNode = model->findStateById(compoundStateId);
    if (!stateNode) {
        return "";
    }

    // Get initial state specification
    std::string initialState = stateNode->getInitialState();
    if (!initialState.empty()) {
        return initialState;
    }

    // Fall back to default child
    return getDefaultChild(model, compoundStateId);
}

// ========== Protected Methods ==========

void CompoundStateProcessor::enterCompoundStateInternal(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                        const std::string &compoundStateId,
                                                        SCXML::Runtime::RuntimeContext &context,
                                                        ProcessingResult &result) {
    // 1. Enter the compound state itself
    context.activateState(compoundStateId);
    result.enteredStates.push_back(compoundStateId);

    // 2. Resolve initial configuration for this compound state
    auto initialConfig = resolveInitialConfiguration(model, compoundStateId, context);

    if (!initialConfig.success) {
        // Fall back to default child if initial resolution fails
        std::string defaultChild = getDefaultChild(model, compoundStateId);
        if (!defaultChild.empty()) {
            context.activateState(defaultChild);
            result.enteredStates.push_back(defaultChild);

            // If default child is also compound, recurse
            if (isCompoundState(model, defaultChild)) {
                ProcessingResult childResult = enterCompoundState(model, defaultChild, context);
                result.enteredStates.insert(result.enteredStates.end(), childResult.enteredStates.begin(),
                                            childResult.enteredStates.end());
            }
        }
        return;
    }

    // 3. Enter all states in the initial configuration
    for (const auto &stateId : initialConfig.entryOrder) {
        if (stateId != compoundStateId) {  // Don't double-enter the compound state
            context.activateState(stateId);
            result.enteredStates.push_back(stateId);
        }
    }
}

void CompoundStateProcessor::exitCompoundStateInternal(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                       const std::string &compoundStateId,
                                                       SCXML::Runtime::RuntimeContext &context,
                                                       ProcessingResult &result) {
    // 1. Get all active descendants
    auto activeDescendants = getActiveDescendants(compoundStateId, context);

    // 2. Exit descendants in reverse document order (deepest first)
    std::reverse(activeDescendants.begin(), activeDescendants.end());

    for (const auto &descendant : activeDescendants) {
        context.deactivateState(descendant);
        result.exitedStates.push_back(descendant);
    }

    // 3. Exit the compound state itself
    context.deactivateState(compoundStateId);
    result.exitedStates.push_back(compoundStateId);
}

std::vector<std::string> CompoundStateProcessor::getChildStates(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                                const std::string &compoundStateId) const {
    if (!hierarchyManager_) {
        return {};
    }

    return hierarchyManager_->getChildren(compoundStateId);
}

bool CompoundStateProcessor::isValidConfiguration(const std::string &compoundStateId,
                                                  SCXML::Runtime::RuntimeContext &context) const {
    if (!hierarchyManager_) {
        return false;
    }

    // For compound states, at least one child must be active
    auto children = hierarchyManager_->getChildren(compoundStateId);
    auto activeStates = context.getActiveStates();

    for (const auto &child : children) {
        if (std::find(activeStates.begin(), activeStates.end(), child) != activeStates.end()) {
            return true;  // At least one child is active
        }
    }

    return false;  // No children are active
}

// ========== Private Methods ==========

void CompoundStateProcessor::addError(const std::string &message) {
    errorMessages_.push_back(message);
}

void CompoundStateProcessor::clearErrors() {
    errorMessages_.clear();
}

}  // namespace SCXML