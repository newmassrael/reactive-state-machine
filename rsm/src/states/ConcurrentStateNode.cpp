#include "states/ConcurrentStateNode.h"
#include "common/Logger.h"
#include "model/DoneData.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/IExecutionContext.h"
#include "states/ConcurrentRegion.h"
#include <algorithm>
#include <cassert>

namespace RSM {

ConcurrentStateNode::ConcurrentStateNode(const std::string &id, const ConcurrentStateConfig &config)
    : id_(id), parent_(nullptr), config_(config), hasNotifiedCompletion_(false), historyType_(HistoryType::NONE),
      initialTransition_(nullptr) {
    LOG_DEBUG("Creating parallel state: {}", id);

    // Initialize DoneData
    doneData_ = std::make_unique<DoneData>();
}

ConcurrentStateNode::~ConcurrentStateNode() {
    LOG_DEBUG("Destroying concurrent state: {}", id_);
}

// IStateNode interface implementation

const std::string &ConcurrentStateNode::getId() const {
    return id_;
}

Type ConcurrentStateNode::getType() const {
    return Type::PARALLEL;
}

void ConcurrentStateNode::setParent(IStateNode *parent) {
    LOG_DEBUG("Setting parent for {}: {}", id_, (parent ? parent->getId() : "null"));
    parent_ = parent;
}

IStateNode *ConcurrentStateNode::getParent() const {
    return parent_;
}

void ConcurrentStateNode::addChild(std::shared_ptr<IStateNode> child) {
    if (child) {
        LOG_DEBUG("Adding child to {}: {}", id_, child->getId());
        children_.push_back(child);

        // SCXML W3C specification section 3.4: child states in parallel states become regions
        // Automatically create ConcurrentRegion wrapper for SCXML compliance
        std::string regionId = child->getId();
        auto region = std::make_shared<ConcurrentRegion>(regionId, child);

        auto result = addRegion(region);
        if (!result.isSuccess) {
            LOG_ERROR("Failed to create region for child '{}': {}", child->getId(), result.errorMessage);
        } else {
            LOG_DEBUG("Successfully created region: {}", regionId);
        }
    } else {
        LOG_WARN("Attempt to add null child to {}", id_);
    }
}

const std::vector<std::shared_ptr<IStateNode>> &ConcurrentStateNode::getChildren() const {
    return children_;
}

void ConcurrentStateNode::addTransition(std::shared_ptr<ITransitionNode> transition) {
    if (transition) {
        LOG_DEBUG("Adding transition to {}", id_);
        transitions_.push_back(transition);
    } else {
        LOG_WARN("Attempt to add null transition to {}", id_);
    }
}

const std::vector<std::shared_ptr<ITransitionNode>> &ConcurrentStateNode::getTransitions() const {
    return transitions_;
}

void ConcurrentStateNode::addDataItem(std::shared_ptr<IDataModelItem> dataItem) {
    if (dataItem) {
        LOG_DEBUG("Adding data item to {}", id_);
        dataItems_.push_back(dataItem);
    } else {
        LOG_WARN("Attempt to add null data item to {}", id_);
    }
}

const std::vector<std::shared_ptr<IDataModelItem>> &ConcurrentStateNode::getDataItems() const {
    return dataItems_;
}

void ConcurrentStateNode::setOnEntry(const std::string &callback) {
    LOG_DEBUG("Setting onEntry callback for {}", id_);
    onEntry_ = callback;
}

const std::string &ConcurrentStateNode::getOnEntry() const {
    return onEntry_;
}

void ConcurrentStateNode::setOnExit(const std::string &callback) {
    LOG_DEBUG("Setting onExit callback for {}", id_);
    onExit_ = callback;
}

const std::string &ConcurrentStateNode::getOnExit() const {
    return onExit_;
}

void ConcurrentStateNode::setInitialState(const std::string &state) {
    LOG_DEBUG("Setting initial state for {}: {}", id_, state);
    initialState_ = state;
}

const std::string &ConcurrentStateNode::getInitialState() const {
    return initialState_;
}

void ConcurrentStateNode::addInvoke(std::shared_ptr<IInvokeNode> invoke) {
    if (invoke) {
        LOG_DEBUG("Adding invoke to {}", id_);
        invokeNodes_.push_back(invoke);
    } else {
        LOG_WARN("Attempt to add null invoke to {}", id_);
    }
}

const std::vector<std::shared_ptr<IInvokeNode>> &ConcurrentStateNode::getInvoke() const {
    return invokeNodes_;
}

void ConcurrentStateNode::setHistoryType(bool isDeep) {
    historyType_ = isDeep ? HistoryType::DEEP : HistoryType::SHALLOW;
    LOG_DEBUG("Setting history type for {} to {}", id_, (isDeep ? "DEEP" : "SHALLOW"));
}

HistoryType ConcurrentStateNode::getHistoryType() const {
    return historyType_;
}

bool ConcurrentStateNode::isShallowHistory() const {
    return historyType_ == HistoryType::SHALLOW;
}

bool ConcurrentStateNode::isDeepHistory() const {
    return historyType_ == HistoryType::DEEP;
}

void ConcurrentStateNode::addReactiveGuard(const std::string &guardId) {
    LOG_DEBUG("Adding reactive guard to {}: {}", id_, guardId);
    reactiveGuards_.push_back(guardId);
}

const std::vector<std::string> &ConcurrentStateNode::getReactiveGuards() const {
    return reactiveGuards_;
}

// W3C SCXML 3.8/3.9: Block-based action methods
void ConcurrentStateNode::addEntryActionBlock(std::vector<std::shared_ptr<IActionNode>> block) {
    if (!block.empty()) {
        entryActionBlocks_.push_back(std::move(block));
    }
}

const std::vector<std::vector<std::shared_ptr<IActionNode>>> &ConcurrentStateNode::getEntryActionBlocks() const {
    return entryActionBlocks_;
}

void ConcurrentStateNode::addExitActionBlock(std::vector<std::shared_ptr<IActionNode>> block) {
    if (!block.empty()) {
        exitActionBlocks_.push_back(std::move(block));
    }
}

const std::vector<std::vector<std::shared_ptr<IActionNode>>> &ConcurrentStateNode::getExitActionBlocks() const {
    return exitActionBlocks_;
}

bool ConcurrentStateNode::isFinalState() const {
    // A concurrent state is final when all its regions are in final states
    return areAllRegionsComplete();
}

const DoneData &ConcurrentStateNode::getDoneData() const {
    return *doneData_;
}

DoneData &ConcurrentStateNode::getDoneData() {
    return *doneData_;
}

void ConcurrentStateNode::setDoneDataContent(const std::string &content) {
    LOG_DEBUG("Setting done data content for {}", id_);
    doneData_->setContent(content);
}

void ConcurrentStateNode::addDoneDataParam(const std::string &name, const std::string &value) {
    LOG_DEBUG("Adding done data param to {}: {} = {}", id_, name, value);
    doneData_->addParam(name, value);
}

void ConcurrentStateNode::clearDoneDataParams() {
    LOG_DEBUG("Clearing done data params for {}", id_);
    doneData_->clearParams();
}

std::shared_ptr<ITransitionNode> ConcurrentStateNode::getInitialTransition() const {
    return initialTransition_;
}

void ConcurrentStateNode::setInitialTransition(std::shared_ptr<ITransitionNode> transition) {
    LOG_DEBUG("Setting initial transition for {} (Note: Concurrent states typically don't use initial transitions)",
              id_);
    initialTransition_ = transition;
}

// Concurrent state specific methods

ConcurrentOperationResult ConcurrentStateNode::addRegion(std::shared_ptr<IConcurrentRegion> region) {
    if (!region) {
        return ConcurrentOperationResult::failure("", "Cannot add null region");
    }

    const std::string &regionId = region->getId();

    // Check for duplicate region IDs
    for (const auto &existingRegion : regions_) {
        if (existingRegion->getId() == regionId) {
            return ConcurrentOperationResult::failure(regionId,
                                                      fmt::format("Region with ID '{}' already exists", regionId));
        }
    }

    regions_.push_back(region);
    LOG_DEBUG("Added region '{}' to {}", regionId, id_);

    return ConcurrentOperationResult::success(regionId);
}

ConcurrentOperationResult ConcurrentStateNode::removeRegion(const std::string &regionId) {
    auto it =
        std::find_if(regions_.begin(), regions_.end(), [&regionId](const std::shared_ptr<IConcurrentRegion> &region) {
            return region->getId() == regionId;
        });

    if (it == regions_.end()) {
        return ConcurrentOperationResult::failure(regionId, fmt::format("Region with ID '{}' not found", regionId));
    }

    regions_.erase(it);
    LOG_DEBUG("Removed region '{}' from {}", regionId, id_);

    return ConcurrentOperationResult::success(regionId);
}

const std::vector<std::shared_ptr<IConcurrentRegion>> &ConcurrentStateNode::getRegions() const {
    return regions_;
}

std::shared_ptr<IConcurrentRegion> ConcurrentStateNode::getRegion(const std::string &regionId) const {
    auto it =
        std::find_if(regions_.begin(), regions_.end(), [&regionId](const std::shared_ptr<IConcurrentRegion> &region) {
            return region->getId() == regionId;
        });

    return (it != regions_.end()) ? *it : nullptr;
}

ConcurrentOperationResult ConcurrentStateNode::enterParallelState() {
    LOG_DEBUG("Entering parallel state: {}", id_);

    // SCXML W3C specification section 3.4: parallel states MUST have regions
    if (regions_.empty()) {
        std::string error = fmt::format(
            "SCXML violation: parallel state '{}' has no regions. SCXML specification requires at least one region.",
            id_);
        LOG_ERROR("{}", error);
        assert(false && "SCXML violation: parallel state must have at least one region");
        return ConcurrentOperationResult::failure(id_, error);
    }

    // State entry managed by architectural separation

    // SCXML W3C specification section 3.4: ALL child regions MUST be activated simultaneously
    LOG_DEBUG("Activating {} regions simultaneously", regions_.size());

    auto results = activateAllRegions();

    // Check if any region failed to activate
    for (const auto &result : results) {
        if (!result.isSuccess) {
            std::string error = fmt::format("Failed to activate region '{}': {}", result.regionId, result.errorMessage);
            LOG_ERROR("{}", error);
            // Cleanup on failure
            return ConcurrentOperationResult::failure(id_, error);
        }
    }

    // Entry process complete
    // Completion checking is delegated to StateMachine::enterState()

    LOG_DEBUG("Successfully entered parallel state: {}", id_);
    return ConcurrentOperationResult::success(id_);
}

ConcurrentOperationResult ConcurrentStateNode::exitParallelState(std::shared_ptr<IExecutionContext> executionContext) {
    LOG_DEBUG("Exiting parallel state: {}", id_);

    // W3C SCXML 3.13: Exit regions FIRST (children before parent) - test 404
    auto results = deactivateAllRegions(executionContext);

    // Log warnings for any deactivation issues but continue (exit should not fail)
    for (const auto &result : results) {
        if (!result.isSuccess) {
            LOG_WARN("Warning during region deactivation '{}': {}", result.regionId, result.errorMessage);
        }
    }

    // W3C SCXML 3.13: Execute parallel state's own exit actions AFTER regions (test 404)
    const auto &exitActionBlocks = getExitActionBlocks();
    if (!exitActionBlocks.empty() && executionContext && executionContext->isValid()) {
        auto &actionExecutor = executionContext->getActionExecutor();
        auto *actionExecutorImpl = dynamic_cast<ActionExecutorImpl *>(&actionExecutor);

        // Set immediate mode to false for exit actions
        if (actionExecutorImpl) {
            actionExecutorImpl->setImmediateMode(false);
        }

        for (const auto &actionBlock : exitActionBlocks) {
            for (const auto &action : actionBlock) {
                if (action) {
                    LOG_DEBUG("Executing parallel state exit action: {}", action->getActionType());
                    action->execute(*executionContext);
                }
            }
        }

        // Restore immediate mode
        if (actionExecutorImpl) {
            actionExecutorImpl->setImmediateMode(true);
        }
    }

    // Reset completion notification state when exiting
    hasNotifiedCompletion_ = false;

    LOG_DEBUG("Successfully exited parallel state: {}", id_);
    return ConcurrentOperationResult::success(id_);
}

std::vector<ConcurrentOperationResult> ConcurrentStateNode::activateAllRegions() {
    std::vector<ConcurrentOperationResult> results;
    results.reserve(regions_.size());

    LOG_DEBUG("Activating {} regions in parallel state: {}", regions_.size(), id_);

    for (auto &region : regions_) {
        LOG_DEBUG("ConcurrentStateNode: Parallel state '{}' activating region '{}'", id_, region->getId());
        auto result = region->activate();
        results.push_back(result);

        if (!result.isSuccess) {
            LOG_WARN("Failed to activate region '{}': {}", region->getId(), result.errorMessage);
        } else {
            LOG_DEBUG("ConcurrentStateNode: Region '{}' activation result: success={}, isActive={}", region->getId(),
                      result.isSuccess, region->isActive());
        }
    }

    // SCXML W3C specification section 3.4: Check for parallel state completion after activation
    // Note: Completion check is deferred to StateMachine after state is fully entered
    // This ensures done.state events are generated only when parallel state is active
    if (areAllRegionsInFinalState()) {
        LOG_DEBUG("All regions immediately reached final states after activation in {}", id_);
        // Don't trigger completion callback here - StateMachine will check after hierarchyManager->enterState()
    }

    return results;
}

std::vector<ConcurrentOperationResult>
ConcurrentStateNode::deactivateAllRegions(std::shared_ptr<IExecutionContext> executionContext) {
    std::vector<ConcurrentOperationResult> results;
    results.reserve(regions_.size());

    LOG_DEBUG("Deactivating {} regions in {}", regions_.size(), id_);

    // W3C SCXML 3.13: Exit in reverse document order (test 404)
    for (auto it = regions_.rbegin(); it != regions_.rend(); ++it) {
        auto &region = *it;
        auto result = region->deactivate(executionContext);
        results.push_back(result);

        if (!result.isSuccess) {
            LOG_WARN("Failed to deactivate region '{}': {}", region->getId(), result.errorMessage);
        }
    }

    return results;
}

bool ConcurrentStateNode::areAllRegionsComplete() const {
    // SCXML W3C specification section 3.4: parallel states MUST have regions
    if (regions_.empty()) {
        LOG_ERROR(
            "SCXML violation: parallel state '{}' has no regions. SCXML specification requires at least one region.",
            id_);
        // No fallback - this is a specification violation
        assert(false && "SCXML violation: parallel state must have at least one region");
        return false;
    }

    // SCXML W3C specification section 3.4: ALL regions must be in final state for completion
    // No configuration options - this is mandated by specification
    bool isComplete =
        std::all_of(regions_.begin(), regions_.end(), [this](const std::shared_ptr<IConcurrentRegion> &region) {
            if (!region) {
                LOG_ERROR("SCXML violation: null region in parallel state '{}'", id_);
                assert(false && "SCXML violation: parallel state cannot have null regions");
                return false;
            }
            return region->isInFinalState();
        });

    // Trigger completion callback if state transitions from incomplete to complete
    // This implements SCXML W3C specification section 3.4 for done.state event generation
    if (isComplete && !hasNotifiedCompletion_ && completionCallback_) {
        hasNotifiedCompletion_ = true;
        LOG_DEBUG("All regions complete, triggering done.state event for {}", id_);

        // Call the completion callback to notify the runtime system
        // The runtime system will generate the done.state.{id} event
        completionCallback_(id_);
    }

    // Reset notification state if we're no longer complete
    // This allows for re-notification if the state completes again
    if (!isComplete && hasNotifiedCompletion_) {
        hasNotifiedCompletion_ = false;
        LOG_DEBUG("Reset completion notification state for {}", id_);
    }

    return isComplete;
}

std::vector<ConcurrentRegionInfo> ConcurrentStateNode::getConfiguration() const {
    std::vector<ConcurrentRegionInfo> configuration;
    configuration.reserve(regions_.size());

    for (const auto &region : regions_) {
        configuration.push_back(region->getInfo());
    }

    return configuration;
}

std::vector<ConcurrentOperationResult> ConcurrentStateNode::processEventInAllRegions(const EventDescriptor &event) {
    std::vector<ConcurrentOperationResult> results;

    // SCXML W3C specification section 3.4: events MUST be broadcast to all active regions
    // No configuration option - this is mandated by specification
    assert(!regions_.empty() && "SCXML violation: parallel state must have regions for event processing");

    results.reserve(regions_.size());

    LOG_DEBUG("Broadcasting event '{}' to {} regions in {}", event.eventName, regions_.size(), id_);

    for (auto &region : regions_) {
        assert(region && "SCXML violation: parallel state cannot have null regions");

        LOG_DEBUG("DEBUG: Region '{}' isActive: {}", region->getId(), region->isActive());
        if (region->isActive()) {
            LOG_DEBUG("DEBUG: Processing event '{}' in active region '{}'", event.eventName, region->getId());
            auto result = region->processEvent(event);
            results.push_back(result);
        } else {
            LOG_DEBUG("DEBUG: Skipping inactive region '{}' for event '{}'", region->getId(), event.eventName);
        }
    }

    // SCXML W3C specification section 3.4: Check for parallel state completion
    // "When all of the children reach final states, the <parallel> element itself is considered to be in a final state"
    if (areAllRegionsInFinalState()) {
        areAllRegionsComplete();  // This will handle completion callback if appropriate
    }

    return results;
}

const ConcurrentStateConfig &ConcurrentStateNode::getConfig() const {
    return config_;
}

void ConcurrentStateNode::setConfig(const ConcurrentStateConfig &config) {
    LOG_DEBUG("Updating configuration for {}", id_);
    config_ = config;
}

std::vector<std::string> ConcurrentStateNode::validateConcurrentState() const {
    std::vector<std::string> errors;

    // SCXML W3C specification section 3.4: parallel states MUST have at least one region
    if (regions_.empty()) {
        errors.push_back(fmt::format(
            "SCXML violation: Parallel state '{}' has no regions. SCXML specification requires at least one region.",
            id_));
    }

    // Validate each region
    for (const auto &region : regions_) {
        auto regionErrors = region->validate();
        for (const auto &error : regionErrors) {
            errors.push_back(fmt::format("Region '{}': {}", region->getId(), error));
        }
    }

    // Check for duplicate region IDs (shouldn't happen if addRegion is used correctly)
    for (size_t i = 0; i < regions_.size(); ++i) {
        for (size_t j = i + 1; j < regions_.size(); ++j) {
            if (regions_[i]->getId() == regions_[j]->getId()) {
                errors.push_back(fmt::format("Duplicate region ID found: {}", regions_[i]->getId()));
            }
        }
    }

    return errors;
}

void ConcurrentStateNode::setCompletionCallback(const ParallelStateCompletionCallback &callback) {
    LOG_DEBUG("Setting completion callback for {}", id_);
    completionCallback_ = callback;
}

void ConcurrentStateNode::setExecutionContextForRegions(std::shared_ptr<IExecutionContext> executionContext) {
    LOG_DEBUG("Setting ExecutionContext for {} regions in {}", regions_.size(), id_);

    // SOLID: Dependency Injection - provide ExecutionContext to all existing regions
    for (auto &region : regions_) {
        if (region) {
            // Cast to ConcurrentRegion to access setExecutionContext method
            auto concreteRegion = std::dynamic_pointer_cast<ConcurrentRegion>(region);
            if (concreteRegion) {
                concreteRegion->setExecutionContext(executionContext);
                LOG_DEBUG("Set ExecutionContext for region: {}", region->getId());
            }
        }
    }
}

bool ConcurrentStateNode::areAllRegionsInFinalState() const {
    LOG_DEBUG("Checking {} regions in {}", regions_.size(), id_);

    if (regions_.empty()) {
        LOG_WARN("No regions in parallel state: {}", id_);
        return false;
    }

    // SCXML W3C specification section 3.4: All child regions must be in final state
    for (const auto &region : regions_) {
        assert(region && "SCXML violation: parallel state cannot have null regions");

        if (!region->isInFinalState()) {
            LOG_DEBUG("Region {} not in final state yet in {}", region->getId(), id_);
            return false;
        }
    }

    LOG_DEBUG("All {} regions in parallel state {} have reached final states", regions_.size(), id_);
    return true;
}

void ConcurrentStateNode::generateDoneStateEvent() {
    // SCXML W3C specification section 3.4: Generate done.state.{stateId} event
    // "When all of the children reach final states, the <parallel> element itself is considered to be in a final state"

    if (hasNotifiedCompletion_) {
        LOG_DEBUG("Already notified completion for {}", id_);
        return;
    }

    std::string doneEventName = fmt::format("done.state.{}", id_);
    LOG_DEBUG("Generating done.state event: {} for completed parallel state: {}", doneEventName, id_);

    // Use completion callback to notify StateMachine
    if (completionCallback_) {
        try {
            completionCallback_(id_);
            hasNotifiedCompletion_ = true;
            LOG_DEBUG("Successfully notified completion via callback for {}", id_);
        } catch (const std::exception &e) {
            LOG_ERROR("Exception in completion callback: {} for {}", e.what(), id_);
        }
    } else {
        LOG_WARN("No completion callback set for parallel state: {}", id_);
    }
}

}  // namespace RSM