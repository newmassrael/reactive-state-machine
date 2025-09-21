#include "states/ConcurrentStateNode.h"
#include "common/Logger.h"
#include "model/DoneData.h"
#include "states/ConcurrentRegion.h"
#include <algorithm>
#include <cassert>

namespace RSM {

ConcurrentStateNode::ConcurrentStateNode(const std::string &id, const ConcurrentStateConfig &config)
    : id_(id), parent_(nullptr), config_(config), hasNotifiedCompletion_(false), historyType_(HistoryType::NONE),
      initialTransition_(nullptr) {
    Logger::debug("ConcurrentStateNode::Constructor - Creating concurrent state: " + id);

    // Initialize DoneData
    doneData_ = std::make_unique<DoneData>();
}

ConcurrentStateNode::~ConcurrentStateNode() {
    Logger::debug("ConcurrentStateNode::Destructor - Destroying concurrent state: " + id_);
}

// IStateNode interface implementation

const std::string &ConcurrentStateNode::getId() const {
    return id_;
}

Type ConcurrentStateNode::getType() const {
    return Type::PARALLEL;
}

void ConcurrentStateNode::setParent(IStateNode *parent) {
    Logger::debug("ConcurrentStateNode::setParent() - Setting parent for " + id_ + ": " +
                  (parent ? parent->getId() : "null"));
    parent_ = parent;
}

IStateNode *ConcurrentStateNode::getParent() const {
    return parent_;
}

void ConcurrentStateNode::addChild(std::shared_ptr<IStateNode> child) {
    if (child) {
        Logger::debug("ConcurrentStateNode::addChild() - Adding child to " + id_ + ": " + child->getId());
        children_.push_back(child);

        // SCXML W3C specification section 3.4: child states in parallel states become regions
        // Automatically create ConcurrentRegion wrapper for SCXML compliance
        std::string regionId = child->getId();
        auto region = std::make_shared<ConcurrentRegion>(regionId, child);

        auto result = addRegion(region);
        if (!result.isSuccess) {
            Logger::error("ConcurrentStateNode::addChild() - Failed to create region for child '" + child->getId() +
                          "': " + result.errorMessage);
        } else {
            Logger::debug("ConcurrentStateNode::addChild() - Successfully created region: " + regionId);
        }
    } else {
        Logger::warn("ConcurrentStateNode::addChild() - Attempt to add null child to " + id_);
    }
}

const std::vector<std::shared_ptr<IStateNode>> &ConcurrentStateNode::getChildren() const {
    return children_;
}

void ConcurrentStateNode::addTransition(std::shared_ptr<ITransitionNode> transition) {
    if (transition) {
        Logger::debug("ConcurrentStateNode::addTransition() - Adding transition to " + id_);
        transitions_.push_back(transition);
    } else {
        Logger::warn("ConcurrentStateNode::addTransition() - Attempt to add null transition to " + id_);
    }
}

const std::vector<std::shared_ptr<ITransitionNode>> &ConcurrentStateNode::getTransitions() const {
    return transitions_;
}

void ConcurrentStateNode::addDataItem(std::shared_ptr<IDataModelItem> dataItem) {
    if (dataItem) {
        Logger::debug("ConcurrentStateNode::addDataItem() - Adding data item to " + id_);
        dataItems_.push_back(dataItem);
    } else {
        Logger::warn("ConcurrentStateNode::addDataItem() - Attempt to add null data item to " + id_);
    }
}

const std::vector<std::shared_ptr<IDataModelItem>> &ConcurrentStateNode::getDataItems() const {
    return dataItems_;
}

void ConcurrentStateNode::setOnEntry(const std::string &callback) {
    Logger::debug("ConcurrentStateNode::setOnEntry() - Setting onEntry callback for " + id_);
    onEntry_ = callback;
}

const std::string &ConcurrentStateNode::getOnEntry() const {
    return onEntry_;
}

void ConcurrentStateNode::setOnExit(const std::string &callback) {
    Logger::debug("ConcurrentStateNode::setOnExit() - Setting onExit callback for " + id_);
    onExit_ = callback;
}

const std::string &ConcurrentStateNode::getOnExit() const {
    return onExit_;
}

void ConcurrentStateNode::setInitialState(const std::string &state) {
    Logger::debug("ConcurrentStateNode::setInitialState() - Setting initial state for " + id_ + ": " + state);
    initialState_ = state;
}

const std::string &ConcurrentStateNode::getInitialState() const {
    return initialState_;
}

void ConcurrentStateNode::addEntryAction(const std::string &actionId) {
    Logger::debug("ConcurrentStateNode::addEntryAction() - Adding entry action to " + id_ + ": " + actionId);
    entryActions_.push_back(actionId);
}

void ConcurrentStateNode::addExitAction(const std::string &actionId) {
    Logger::debug("ConcurrentStateNode::addExitAction() - Adding exit action to " + id_ + ": " + actionId);
    exitActions_.push_back(actionId);
}

void ConcurrentStateNode::addInvoke(std::shared_ptr<IInvokeNode> invoke) {
    if (invoke) {
        Logger::debug("ConcurrentStateNode::addInvoke() - Adding invoke to " + id_);
        invokeNodes_.push_back(invoke);
    } else {
        Logger::warn("ConcurrentStateNode::addInvoke() - Attempt to add null invoke to " + id_);
    }
}

const std::vector<std::shared_ptr<IInvokeNode>> &ConcurrentStateNode::getInvoke() const {
    return invokeNodes_;
}

void ConcurrentStateNode::setHistoryType(bool isDeep) {
    historyType_ = isDeep ? HistoryType::DEEP : HistoryType::SHALLOW;
    Logger::debug("ConcurrentStateNode::setHistoryType() - Setting history type for " + id_ + " to " +
                  (isDeep ? "DEEP" : "SHALLOW"));
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
    Logger::debug("ConcurrentStateNode::addReactiveGuard() - Adding reactive guard to " + id_ + ": " + guardId);
    reactiveGuards_.push_back(guardId);
}

const std::vector<std::string> &ConcurrentStateNode::getReactiveGuards() const {
    return reactiveGuards_;
}

const std::vector<std::string> &ConcurrentStateNode::getEntryActions() const {
    return entryActions_;
}

const std::vector<std::string> &ConcurrentStateNode::getExitActions() const {
    return exitActions_;
}

void ConcurrentStateNode::addEntryActionNode(std::shared_ptr<IActionNode> action) {
    if (action) {
        Logger::debug("ConcurrentStateNode::addEntryActionNode() - Adding entry action node to " + id_);
        entryActionNodes_.push_back(action);
    } else {
        Logger::warn("ConcurrentStateNode::addEntryActionNode() - Attempt to add null action node to " + id_);
    }
}

void ConcurrentStateNode::addExitActionNode(std::shared_ptr<IActionNode> action) {
    if (action) {
        Logger::debug("ConcurrentStateNode::addExitActionNode() - Adding exit action node to " + id_);
        exitActionNodes_.push_back(action);
    } else {
        Logger::warn("ConcurrentStateNode::addExitActionNode() - Attempt to add null action node to " + id_);
    }
}

const std::vector<std::shared_ptr<IActionNode>> &ConcurrentStateNode::getEntryActionNodes() const {
    return entryActionNodes_;
}

const std::vector<std::shared_ptr<IActionNode>> &ConcurrentStateNode::getExitActionNodes() const {
    return exitActionNodes_;
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
    Logger::debug("ConcurrentStateNode::setDoneDataContent() - Setting done data content for " + id_);
    doneData_->setContent(content);
}

void ConcurrentStateNode::addDoneDataParam(const std::string &name, const std::string &value) {
    Logger::debug("ConcurrentStateNode::addDoneDataParam() - Adding done data param to " + id_ + ": " + name + " = " +
                  value);
    doneData_->addParam(name, value);
}

void ConcurrentStateNode::clearDoneDataParams() {
    Logger::debug("ConcurrentStateNode::clearDoneDataParams() - Clearing done data params for " + id_);
    doneData_->clearParams();
}

std::shared_ptr<ITransitionNode> ConcurrentStateNode::getInitialTransition() const {
    return initialTransition_;
}

void ConcurrentStateNode::setInitialTransition(std::shared_ptr<ITransitionNode> transition) {
    Logger::debug("ConcurrentStateNode::setInitialTransition() - Setting initial transition for " + id_ +
                  " (Note: Concurrent states typically don't use initial transitions)");
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
            return ConcurrentOperationResult::failure(regionId, "Region with ID '" + regionId + "' already exists");
        }
    }

    regions_.push_back(region);
    Logger::debug("ConcurrentStateNode::addRegion() - Added region '" + regionId + "' to " + id_);

    return ConcurrentOperationResult::success(regionId);
}

ConcurrentOperationResult ConcurrentStateNode::removeRegion(const std::string &regionId) {
    auto it =
        std::find_if(regions_.begin(), regions_.end(), [&regionId](const std::shared_ptr<IConcurrentRegion> &region) {
            return region->getId() == regionId;
        });

    if (it == regions_.end()) {
        return ConcurrentOperationResult::failure(regionId, "Region with ID '" + regionId + "' not found");
    }

    regions_.erase(it);
    Logger::debug("ConcurrentStateNode::removeRegion() - Removed region '" + regionId + "' from " + id_);

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
    Logger::debug("ConcurrentStateNode::enterParallelState() - Entering parallel state: " + id_);

    // SCXML W3C specification section 3.4: parallel states MUST have regions
    if (regions_.empty()) {
        std::string error = "SCXML violation: parallel state '" + id_ +
                            "' has no regions. SCXML specification requires at least one region.";
        Logger::error("ConcurrentStateNode::enterParallelState() - " + error);
        assert(false && "SCXML violation: parallel state must have at least one region");
        return ConcurrentOperationResult::failure(id_, error);
    }

    // SCXML W3C specification section 3.4: ALL child regions MUST be activated simultaneously
    Logger::debug("ConcurrentStateNode::enterParallelState() - Activating " + std::to_string(regions_.size()) +
                  " regions simultaneously");

    auto results = activateAllRegions();

    // Check if any region failed to activate
    for (const auto &result : results) {
        if (!result.isSuccess) {
            std::string error = "Failed to activate region '" + result.regionId + "': " + result.errorMessage;
            Logger::error("ConcurrentStateNode::enterParallelState() - " + error);
            return ConcurrentOperationResult::failure(id_, error);
        }
    }

    Logger::debug("ConcurrentStateNode::enterParallelState() - Successfully entered parallel state: " + id_);
    return ConcurrentOperationResult::success(id_);
}

ConcurrentOperationResult ConcurrentStateNode::exitParallelState() {
    Logger::debug("ConcurrentStateNode::exitParallelState() - Exiting parallel state: " + id_);

    // SCXML W3C specification section 3.4: ALL child regions MUST be deactivated when exiting
    auto results = deactivateAllRegions();

    // Log warnings for any deactivation issues but continue (exit should not fail)
    for (const auto &result : results) {
        if (!result.isSuccess) {
            Logger::warn("ConcurrentStateNode::exitParallelState() - Warning during region deactivation '" +
                         result.regionId + "': " + result.errorMessage);
        }
    }

    // Reset completion notification state when exiting
    hasNotifiedCompletion_ = false;

    Logger::debug("ConcurrentStateNode::exitParallelState() - Successfully exited parallel state: " + id_);
    return ConcurrentOperationResult::success(id_);
}

std::vector<ConcurrentOperationResult> ConcurrentStateNode::activateAllRegions() {
    std::vector<ConcurrentOperationResult> results;
    results.reserve(regions_.size());

    Logger::debug("ConcurrentStateNode::activateAllRegions() - Activating " + std::to_string(regions_.size()) +
                  " regions in " + id_);

    for (auto &region : regions_) {
        auto result = region->activate();
        results.push_back(result);

        if (!result.isSuccess) {
            Logger::warn("ConcurrentStateNode::activateAllRegions() - Failed to activate region '" + region->getId() +
                         "': " + result.errorMessage);
        }
    }

    return results;
}

std::vector<ConcurrentOperationResult> ConcurrentStateNode::deactivateAllRegions() {
    std::vector<ConcurrentOperationResult> results;
    results.reserve(regions_.size());

    Logger::debug("ConcurrentStateNode::deactivateAllRegions() - Deactivating " + std::to_string(regions_.size()) +
                  " regions in " + id_);

    for (auto &region : regions_) {
        auto result = region->deactivate();
        results.push_back(result);

        if (!result.isSuccess) {
            Logger::warn("ConcurrentStateNode::deactivateAllRegions() - Failed to deactivate region '" +
                         region->getId() + "': " + result.errorMessage);
        }
    }

    return results;
}

bool ConcurrentStateNode::areAllRegionsComplete() const {
    // SCXML W3C specification section 3.4: parallel states MUST have regions
    if (regions_.empty()) {
        Logger::error("ConcurrentStateNode::areAllRegionsComplete() - SCXML violation: parallel state '" + id_ +
                      "' has no regions. SCXML specification requires at least one region.");
        // No fallback - this is a specification violation
        assert(false && "SCXML violation: parallel state must have at least one region");
        return false;
    }

    // SCXML W3C specification section 3.4: ALL regions must be in final state for completion
    // No configuration options - this is mandated by specification
    bool isComplete =
        std::all_of(regions_.begin(), regions_.end(), [this](const std::shared_ptr<IConcurrentRegion> &region) {
            if (!region) {
                Logger::error(
                    "ConcurrentStateNode::areAllRegionsComplete() - SCXML violation: null region in parallel state '" +
                    id_ + "'");
                assert(false && "SCXML violation: parallel state cannot have null regions");
                return false;
            }
            return region->isInFinalState();
        });

    // Trigger completion callback if state transitions from incomplete to complete
    // This implements SCXML W3C specification section 3.4 for done.state event generation
    if (isComplete && !hasNotifiedCompletion_ && completionCallback_) {
        hasNotifiedCompletion_ = true;
        Logger::debug(
            "ConcurrentStateNode::areAllRegionsComplete() - All regions complete, triggering done.state event for " +
            id_);

        // Call the completion callback to notify the runtime system
        // The runtime system will generate the done.state.{id} event
        completionCallback_(id_);
    }

    // Reset notification state if we're no longer complete
    // This allows for re-notification if the state completes again
    if (!isComplete && hasNotifiedCompletion_) {
        hasNotifiedCompletion_ = false;
        Logger::debug("ConcurrentStateNode::areAllRegionsComplete() - Reset completion notification state for " + id_);
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

    Logger::debug("ConcurrentStateNode::processEventInAllRegions() - SCXML compliant: broadcasting event to " +
                  std::to_string(regions_.size()) + " regions in " + id_);

    for (auto &region : regions_) {
        assert(region && "SCXML violation: parallel state cannot have null regions");

        if (region->isActive()) {
            auto result = region->processEvent(event);
            results.push_back(result);
        }
    }

    // SCXML W3C specification section 3.4: Check for parallel state completion
    // "When all of the children reach final states, the <parallel> element itself is considered to be in a final state"
    if (areAllRegionsInFinalState()) {
        Logger::info(
            "ConcurrentStateNode::processEventInAllRegions() - All regions completed, generating done.state event");
        generateDoneStateEvent();
    }

    return results;
}

const ConcurrentStateConfig &ConcurrentStateNode::getConfig() const {
    return config_;
}

void ConcurrentStateNode::setConfig(const ConcurrentStateConfig &config) {
    Logger::debug("ConcurrentStateNode::setConfig() - Updating configuration for " + id_);
    config_ = config;
}

std::vector<std::string> ConcurrentStateNode::validateConcurrentState() const {
    std::vector<std::string> errors;

    // SCXML W3C specification section 3.4: parallel states MUST have at least one region
    if (regions_.empty()) {
        errors.push_back("SCXML violation: Parallel state '" + id_ +
                         "' has no regions. SCXML specification requires at least one region.");
    }

    // Validate each region
    for (const auto &region : regions_) {
        auto regionErrors = region->validate();
        for (const auto &error : regionErrors) {
            errors.push_back("Region '" + region->getId() + "': " + error);
        }
    }

    // Check for duplicate region IDs (shouldn't happen if addRegion is used correctly)
    for (size_t i = 0; i < regions_.size(); ++i) {
        for (size_t j = i + 1; j < regions_.size(); ++j) {
            if (regions_[i]->getId() == regions_[j]->getId()) {
                errors.push_back("Duplicate region ID found: " + regions_[i]->getId());
            }
        }
    }

    return errors;
}

void ConcurrentStateNode::setCompletionCallback(const ParallelStateCompletionCallback &callback) {
    Logger::debug("ConcurrentStateNode::setCompletionCallback() - Setting completion callback for " + id_);
    completionCallback_ = callback;
}

void ConcurrentStateNode::setExecutionContextForRegions(std::shared_ptr<IExecutionContext> executionContext) {
    Logger::debug("ConcurrentStateNode::setExecutionContextForRegions() - Setting ExecutionContext for " +
                  std::to_string(regions_.size()) + " regions in " + id_);

    // SOLID: Dependency Injection - provide ExecutionContext to all existing regions
    for (auto &region : regions_) {
        if (region) {
            // Cast to ConcurrentRegion to access setExecutionContext method
            auto concreteRegion = std::dynamic_pointer_cast<ConcurrentRegion>(region);
            if (concreteRegion) {
                concreteRegion->setExecutionContext(executionContext);
                Logger::debug(
                    "ConcurrentStateNode::setExecutionContextForRegions() - Set ExecutionContext for region: " +
                    region->getId());
            }
        }
    }
}

bool ConcurrentStateNode::areAllRegionsInFinalState() const {
    if (regions_.empty()) {
        Logger::warn("ConcurrentStateNode::areAllRegionsInFinalState() - No regions in parallel state: " + id_);
        return false;
    }

    // SCXML W3C specification section 3.4: All child regions must be in final state
    for (const auto &region : regions_) {
        assert(region && "SCXML violation: parallel state cannot have null regions");

        if (!region->isInFinalState()) {
            Logger::debug("ConcurrentStateNode::areAllRegionsInFinalState() - Region " + region->getId() +
                          " not in final state yet");
            return false;
        }
    }

    Logger::info("ConcurrentStateNode::areAllRegionsInFinalState() - All " + std::to_string(regions_.size()) +
                 " regions in parallel state " + id_ + " have reached final states");
    return true;
}

void ConcurrentStateNode::generateDoneStateEvent() {
    // SCXML W3C specification section 3.4: Generate done.state.{stateId} event
    // "When all of the children reach final states, the <parallel> element itself is considered to be in a final state"

    if (hasNotifiedCompletion_) {
        Logger::debug("ConcurrentStateNode::generateDoneStateEvent() - Already notified completion for " + id_);
        return;
    }

    std::string doneEventName = "done.state." + id_;
    Logger::info("ConcurrentStateNode::generateDoneStateEvent() - Generating SCXML-compliant done.state event: " +
                 doneEventName + " for completed parallel state: " + id_);

    // Use completion callback to notify StateMachine
    if (completionCallback_) {
        try {
            completionCallback_(id_);
            hasNotifiedCompletion_ = true;
            Logger::debug(
                "ConcurrentStateNode::generateDoneStateEvent() - Successfully notified completion via callback");
        } catch (const std::exception &e) {
            Logger::error("ConcurrentStateNode::generateDoneStateEvent() - Exception in completion callback: " +
                          std::string(e.what()));
        }
    } else {
        Logger::warn("ConcurrentStateNode::generateDoneStateEvent() - No completion callback set for parallel state: " +
                     id_);
    }
}

}  // namespace RSM