#include "runtime/ParallelStateProcessor.h"
#include "model/DocumentModel.h"
#include "model/IStateNode.h"
#include "model/StateHierarchyManager.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>

namespace SCXML {

ParallelStateProcessor::ParallelStateProcessor(std::shared_ptr<StateHierarchyManager> hierarchyManager)
    : hierarchyManager_(hierarchyManager) {}

ParallelStateProcessor::ProcessingResult
ParallelStateProcessor::enterParallelState(std::shared_ptr<Model::DocumentModel> model,
                                           const std::string &parallelStateId, Runtime::RuntimeContext &context) {
    clearErrors();
    ProcessingResult result;

    if (!model || parallelStateId.empty()) {
        result.errorMessage = "Invalid model or parallel state ID";
        return result;
    }

    if (!isParallelState(model, parallelStateId)) {
        result.errorMessage = "State '" + parallelStateId + "' is not a parallel state";
        return result;
    }

    try {
        enterParallelStateInternal(model, parallelStateId, context, result);
        result.success = true;
        return result;

    } catch (const std::exception &e) {
        result.errorMessage = "Exception during parallel state entry: " + std::string(e.what());
        return result;
    }
}

ParallelStateProcessor::ProcessingResult
ParallelStateProcessor::exitParallelState(std::shared_ptr<Model::DocumentModel> model,
                                          const std::string &parallelStateId, Runtime::RuntimeContext &context) {
    clearErrors();
    ProcessingResult result;

    if (!model || parallelStateId.empty()) {
        result.errorMessage = "Invalid model or parallel state ID";
        return result;
    }

    try {
        exitParallelStateInternal(model, parallelStateId, context, result);
        result.success = true;
        return result;

    } catch (const std::exception &e) {
        result.errorMessage = "Exception during parallel state exit: " + std::string(e.what());
        return result;
    }
}

bool ParallelStateProcessor::isParallelState(std::shared_ptr<Model::DocumentModel> model,
                                             const std::string &stateId) const {
    if (!model) {
        return false;
    }

    Model::IStateNode *stateNode = model->findStateById(stateId);
    if (!stateNode) {
        return false;
    }

    // Use the actual getType() method to check if state is parallel
    return stateNode->getType() == Type::PARALLEL;
}

std::vector<ParallelStateProcessor::RegionInfo>
ParallelStateProcessor::getRegionInfo(const std::string &parallelStateId, Runtime::RuntimeContext &context) const {
    std::vector<RegionInfo> regionInfos;

    if (!hierarchyManager_) {
        return regionInfos;
    }

    // Get all child regions
    auto regions = hierarchyManager_->getChildren(parallelStateId);
    auto activeStates = context.getActiveStates();

    for (const auto &regionId : regions) {
        RegionInfo info;
        info.regionId = regionId;

        // Find active states in this region
        auto descendants = hierarchyManager_->getDescendants(regionId);
        descendants.push_back(regionId);  // Include the region itself

        for (const auto &descendant : descendants) {
            if (std::find(activeStates.begin(), activeStates.end(), descendant) != activeStates.end()) {
                info.activeStates.push_back(descendant);
            }
        }

        // Check if region is completed by checking if it's in a final state
        info.isCompleted = isRegionInFinalState(child->getId(), context);

        regionInfos.push_back(info);
    }

    return regionInfos;
}

bool ParallelStateProcessor::isParallelStateComplete(const std::string &parallelStateId,
                                                     Runtime::RuntimeContext &context) const {
    auto regionInfos = getRegionInfo(parallelStateId, context);

    // Parallel state is complete when all regions are completed
    for (const auto &info : regionInfos) {
        if (!info.isCompleted) {
            return false;
        }
    }

    return !regionInfos.empty();  // Must have at least one region to be complete
}

std::vector<std::string> ParallelStateProcessor::getRegions(std::shared_ptr<Model::DocumentModel> model,
                                                            const std::string &parallelStateId) const {
    if (!hierarchyManager_) {
        return {};
    }

    // For parallel states, all child states are regions
    return hierarchyManager_->getChildren(parallelStateId);
}

bool ParallelStateProcessor::isValidParallelConfiguration(const std::string &parallelStateId,
                                                          Runtime::RuntimeContext &context) const {
    auto regionInfos = getRegionInfo(parallelStateId, context);

    // Valid configuration: all regions must have at least one active state
    for (const auto &info : regionInfos) {
        if (info.activeStates.empty()) {
            return false;
        }
    }

    return !regionInfos.empty();
}

// ========== Protected Methods ==========

void ParallelStateProcessor::enterParallelStateInternal(std::shared_ptr<Model::DocumentModel> model,
                                                        const std::string &parallelStateId,
                                                        Runtime::RuntimeContext &context, ProcessingResult &result) {
    // 1. Enter the parallel state itself
    context.activateState(parallelStateId);
    result.enteredStates.push_back(parallelStateId);

    // 2. Get all regions (child states of parallel state)
    auto regions = getRegions(model, parallelStateId);

    if (regions.empty()) {
        addError("Parallel state '" + parallelStateId + "' has no regions");
        return;
    }

    // 3. Enter all regions simultaneously
    enterAllRegions(model, regions, context, result);
}

void ParallelStateProcessor::exitParallelStateInternal(std::shared_ptr<Model::DocumentModel> model,
                                                       const std::string &parallelStateId,
                                                       Runtime::RuntimeContext &context, ProcessingResult &result) {
    // 1. Get all regions
    auto regions = getRegions(model, parallelStateId);

    // 2. Exit all regions and their descendants
    exitAllRegions(regions, context, result);

    // 3. Exit the parallel state itself
    context.deactivateState(parallelStateId);
    result.exitedStates.push_back(parallelStateId);
}

void ParallelStateProcessor::enterAllRegions(std::shared_ptr<Model::DocumentModel> model,
                                             const std::vector<std::string> &regions, Runtime::RuntimeContext &context,
                                             ProcessingResult &result) {
    for (const auto &regionId : regions) {
        // Enter the region
        context.activateState(regionId);
        result.enteredStates.push_back(regionId);

        // Enter initial state of the region
        std::string initialState = getRegionInitialState(model, regionId);
        if (!initialState.empty()) {
            context.activateState(initialState);
            result.enteredStates.push_back(initialState);
        }
    }
}

void ParallelStateProcessor::exitAllRegions(const std::vector<std::string> &regions, Runtime::RuntimeContext &context,
                                            ProcessingResult &result) {
    if (!hierarchyManager_) {
        return;
    }

    for (const auto &regionId : regions) {
        // Get all active descendants of this region
        auto descendants = hierarchyManager_->getDescendants(regionId);
        auto activeStates = context.getActiveStates();

        // Exit descendants in reverse order (deepest first)
        std::reverse(descendants.begin(), descendants.end());

        for (const auto &descendant : descendants) {
            if (std::find(activeStates.begin(), activeStates.end(), descendant) != activeStates.end()) {
                context.deactivateState(descendant);
                result.exitedStates.push_back(descendant);
            }
        }

        // Exit the region itself
        if (context.isStateActive(regionId)) {
            context.deactivateState(regionId);
            result.exitedStates.push_back(regionId);
        }
    }
}

std::string ParallelStateProcessor::getRegionInitialState(std::shared_ptr<Model::DocumentModel> model,
                                                          const std::string &regionId) const {
    if (!model) {
        return "";
    }

    Model::IStateNode *regionNode = model->findStateById(regionId);
    if (!regionNode) {
        return "";
    }

    // Check for explicit initial state
    std::string initialState = regionNode->getInitialState();
    if (!initialState.empty()) {
        return initialState;
    }

    // Use first child as default
    if (!hierarchyManager_) {
        return "";
    }

    auto children = hierarchyManager_->getChildren(regionId);
    return children.empty() ? "" : children[0];
}

bool ParallelStateProcessor::isRegionComplete(std::shared_ptr<Model::DocumentModel> model, const std::string &regionId,
                                              Runtime::RuntimeContext &context) const {
    if (!model || !hierarchyManager_) {
        return false;
    }

    // Get all active descendants of the region
    auto descendants = hierarchyManager_->getDescendants(regionId);
    auto activeStates = context.getActiveStates();

    // Check if any active descendant is a final state
    for (const auto &descendant : descendants) {
        if (std::find(activeStates.begin(), activeStates.end(), descendant) != activeStates.end()) {
            Model::IStateNode *stateNode = model->findStateById(descendant);
            if (stateNode && stateNode->isFinalState()) {
                return true;
            }
        }
    }

    return false;
}

// ========== Private Methods ==========

void ParallelStateProcessor::addError(const std::string &message) {
    errorMessages_.push_back(message);
}

void ParallelStateProcessor::clearErrors() {
    errorMessages_.clear();
}

bool ParallelStateProcessor::isRegionInFinalState(const std::string &regionId, Runtime::RuntimeContext &context) const {
    // Get the current active state within this region
    const auto &activeStates = context.getActiveStates();

    // Find the deepest active state within this region
    std::string activeStateInRegion;
    for (const auto &activeState : activeStates) {
        // Check if this active state is within the region hierarchy
        if (activeState.find(regionId) == 0) {
            // This state is within the region, update to find the deepest one
            if (activeStateInRegion.empty() || activeState.length() > activeStateInRegion.length()) {
                activeStateInRegion = activeState;
            }
        }
    }

    if (activeStateInRegion.empty()) {
        return false;  // No active state in this region
    }

    // Get the state node and check if it's a final state
    auto model = context.getModel();
    if (!model) {
        return false;
    }

    auto stateNode = model->getStateById(activeStateInRegion);
    if (!stateNode) {
        return false;
    }

    return stateNode->isFinalState();
}

}  // namespace SCXML