#include "runtime/StateHierarchyManager.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "model/SCXMLModel.h"
#include "runtime/HistoryManager.h"
#include "states/ConcurrentRegion.h"
#include "states/ConcurrentStateNode.h"
#include <algorithm>

namespace RSM {

StateHierarchyManager::StateHierarchyManager(std::shared_ptr<SCXMLModel> model)
    : model_(model), historyManager_(nullptr) {
    LOG_DEBUG("StateHierarchyManager: Initialized with SCXML model");
}

bool StateHierarchyManager::enterState(const std::string &stateId) {
    if (!model_ || stateId.empty()) {
        LOG_WARN("Invalid parameters");
        return false;
    }

    auto stateNode = model_->findStateById(stateId);
    if (!stateNode) {
        LOG_WARN("enterState - State not found: {}", stateId);
        return false;
    }

    LOG_DEBUG("enterState - Entering state: {}", stateId);

    // W3C SCXML 3.10: History states must be restored, not entered directly
    if (stateNode->getType() == Type::HISTORY) {
        if (!historyManager_) {
            LOG_ERROR("enterState - History state {} requires historyManager but it's not set", stateId);
            return false;
        }

        LOG_DEBUG("enterState - Restoring history state: {}", stateId);

        // W3C SCXML 3.10: Restore history and enter target states
        auto restorationResult = historyManager_->restoreHistory(stateId);
        if (!restorationResult.success || restorationResult.targetStateIds.empty()) {
            LOG_ERROR("enterState - History restoration failed for: {}", stateId);
            return false;
        }

        LOG_DEBUG("enterState - History restoration successful, entering {} target states",
                  restorationResult.targetStateIds.size());

        // W3C SCXML 3.10 (test 579): Execute default transition actions ONLY if no stored history
        bool hasRecordedHistory = restorationResult.isRestoredFromRecording;
        if (!hasRecordedHistory && model_) {
            auto historyStateNode = model_->findStateById(stateId);
            if (historyStateNode) {
                const auto &transitions = historyStateNode->getTransitions();
                if (!transitions.empty()) {
                    const auto &defaultTransition = transitions[0];
                    const auto &actions = defaultTransition->getActionNodes();
                    if (!actions.empty() && initialTransitionCallback_) {
                        LOG_DEBUG("enterState - Executing {} default transition actions for history state {}",
                                  actions.size(), stateId);
                        initialTransitionCallback_(actions);
                    }
                }
            }
        }

        // Enter all target states from history restoration
        bool allEntered = true;
        for (const auto &targetStateId : restorationResult.targetStateIds) {
            if (!enterState(targetStateId)) {
                LOG_ERROR("enterState - Failed to enter history target state: {}", targetStateId);
                allEntered = false;
            }
        }

        return allEntered;
    }

    // SCXML W3C specification section 3.4: parallel states behave differently from compound states
    if (stateNode->getType() == Type::PARALLEL) {
        // Add state to active configuration (parallel states are always added)
        addStateToConfiguration(stateId);
        // SCXML W3C specification section 3.4: ALL child regions MUST be activated when entering parallel state
        auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
        assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

        LOG_DEBUG("enterState - Entering parallel state with region activation: {}", stateId);

        // W3C SCXML 6.4: Set invoke callback for all regions BEFORE activation
        // This is critical because enterParallelState() will activate regions,
        // which will call enterInitialState() where invokes are processed
        const auto &regions = parallelState->getRegions();
        assert(!regions.empty() && "SCXML violation: parallel state must have at least one region");

        if (invokeDeferCallback_) {
            for (const auto &region : regions) {
                if (region) {
                    region->setInvokeCallback(invokeDeferCallback_);
                    LOG_DEBUG("StateHierarchyManager: Set invoke callback for region: {} BEFORE activation",
                              region->getId());
                }
            }
        }

        // W3C SCXML: Set condition evaluator callback for all regions BEFORE activation
        // This allows regions to evaluate transition guard conditions using StateMachine's JS engine
        if (conditionEvaluator_) {
            for (const auto &region : regions) {
                if (region) {
                    region->setConditionEvaluator(conditionEvaluator_);
                    LOG_DEBUG("StateHierarchyManager: Set condition evaluator for region '{}' BEFORE activation",
                              region->getId());
                }
            }
        }

        // W3C SCXML 403c: Set execution context for all regions BEFORE activation (DRY refactoring)
        updateRegionExecutionContexts(parallelState);

        auto result = parallelState->enterParallelState();
        if (!result.isSuccess) {
            LOG_ERROR("enterState - Failed to enter parallel state '{}': {}", stateId, result.errorMessage);
            return false;
        }

        // SCXML W3C macrostep compliance: Check if transition occurred during parallel state entry
        // This handles done.state events that cause immediate transitions during parallel state activation
        std::string currentStateAfterEntry = getCurrentState();
        if (currentStateAfterEntry != stateId && !currentStateAfterEntry.empty()) {
            LOG_DEBUG("SCXML macrostep: Parallel state entry triggered transition (expected: {}, actual: {})", stateId,
                      currentStateAfterEntry);
            LOG_DEBUG(
                "Skipping region activation - parallel state was exited during entry (e.g., done.state transition)");
            return true;  // Exit early - the transition has already completed
        }

        for (const auto &region : regions) {
            assert(region && "SCXML violation: parallel state cannot have null regions");

            // Add region's root state to active configuration
            auto rootState = region->getRootState();
            assert(rootState && "SCXML violation: region must have root state");

            std::string regionStateId = rootState->getId();
            addStateToConfiguration(regionStateId);
            LOG_DEBUG("enterState - Added region state to configuration: {}", regionStateId);

            // SCXML W3C specification: Enter initial child state of each region
            const auto &children = rootState->getChildren();
            if (!children.empty()) {
                std::string initialChild = rootState->getInitialState();
                if (initialChild.empty()) {
                    // SCXML W3C: Use first child as default initial state
                    initialChild = children[0]->getId();
                }

                addStateToConfiguration(initialChild);

                // W3C SCXML 6.4: Invoke defer is handled by ConcurrentRegion via callback
                // No need to defer here - Region already processes invokes in enterInitialState()
            }
        }

        LOG_DEBUG("enterState - Successfully activated all regions in parallel state: {}", stateId);
    } else if (isCompoundState(stateNode)) {
        // SCXML W3C specification: For compound states, add parent to configuration AND enter initial child
        addStateToConfiguration(stateId);

        // W3C SCXML 6.4: Defer invoke execution for compound states before entering child
        // Compound states can have invokes that should be started when the state is entered
        const auto &invokes = stateNode->getInvoke();
        if (!invokes.empty() && invokeDeferCallback_) {
            LOG_DEBUG("StateHierarchyManager: Deferring {} invokes for compound state: {}", invokes.size(), stateId);
            invokeDeferCallback_(stateId, invokes);
        }

        // W3C SCXML 3.3: Enter initial child state(s) - supports space-separated list for deep targets
        std::string initialChildren = findInitialChildState(stateNode);
        if (!initialChildren.empty()) {
            // W3C SCXML 3.13: Execute initial transition's executable content
            // This must happen AFTER parent onentry and BEFORE child state entry
            // IMPORTANT: Must be executed via callback to StateMachine for proper immediate mode control
            auto initialTransition = stateNode->getInitialTransition();
            if (initialTransition) {
                const auto &actionNodes = initialTransition->getActionNodes();
                if (!actionNodes.empty()) {
                    if (!initialTransitionCallback_) {
                        LOG_WARN("StateHierarchyManager: Initial transition has {} action(s) but callback not set - "
                                 "W3C SCXML 3.13 violation for state: {}",
                                 actionNodes.size(), stateId);
                    } else {
                        LOG_DEBUG("StateHierarchyManager: Executing {} actions from <initial> transition for state: {}",
                                  actionNodes.size(), stateId);
                        initialTransitionCallback_(actionNodes);
                    }
                }
            }

            // W3C SCXML 3.3: Pre-process deep initial targets to set desired initial children for parallel regions
            // This implements the algorithm: if descendant already in statesToEnter, skip default entry
            // Performance optimization: Track processed parallel states to avoid duplicate ancestor traversal O(n²) →
            // O(n)
            std::unordered_set<std::string> processedParallelStates;

            std::istringstream issPreprocess(initialChildren);
            std::string targetId;
            while (issPreprocess >> targetId) {
                auto targetState = model_->findStateById(targetId);
                if (!targetState) {
                    LOG_ERROR("enterState - Initial target state not found: {}", targetId);
                    continue;
                }

                // Traverse ancestors to find parallel regions
                IStateNode *current = targetState->getParent();
                while (current && current != stateNode) {
                    // Check if current's parent is a parallel state
                    if (current->getParent() && current->getParent()->getType() == Type::PARALLEL) {
                        const std::string &parallelStateId = current->getParent()->getId();

                        // Optimization: Skip if we already processed this parallel state
                        if (processedParallelStates.count(parallelStateId)) {
                            current = current->getParent();
                            continue;
                        }
                        processedParallelStates.insert(parallelStateId);

                        // current is a region root (direct child of parallel)
                        auto parallelState = dynamic_cast<ConcurrentStateNode *>(current->getParent());
                        if (parallelState) {
                            // Find the region corresponding to current
                            const auto &regions = parallelState->getRegions();
                            for (const auto &region : regions) {
                                if (region && region->getRootState() &&
                                    region->getRootState()->getId() == current->getId()) {
                                    // Found the region - set desired initial child
                                    region->setDesiredInitialChild(targetId);
                                    LOG_DEBUG("StateHierarchyManager: Set region '{}' desiredInitialChild='{}' from "
                                              "parent state '{}' initial attribute",
                                              region->getId(), targetId, stateId);
                                    break;
                                }
                            }
                        }
                    }
                    current = current->getParent();
                }
            }

            // Parse space-separated initial state list
            std::istringstream iss(initialChildren);
            std::string initialChild;
            bool allEntered = true;
            std::vector<std::string> statesForDeferredOnEntry;

            while (iss >> initialChild) {
                LOG_DEBUG("enterState - Compound state: {} entering initial child: {}", stateId, initialChild);

                auto childState = model_->findStateById(initialChild);

                // W3C SCXML 3.3: For deep initial targets (not direct children), enter all ancestors
                if (childState && childState->getParent() != stateNode) {
                    // Deep target - need to enter intermediate ancestors
                    LOG_DEBUG("enterState - Deep initial target detected, entering ancestors for: {}", initialChild);
                    if (!enterStateWithAncestors(initialChild, stateNode, &statesForDeferredOnEntry)) {
                        LOG_ERROR("enterState - Failed to enter ancestors for: {}", initialChild);
                        allEntered = false;
                    }
                } else {
                    // Direct child - use recursive entry (history states handled by early delegation)
                    if (!enterState(initialChild)) {
                        LOG_ERROR("enterState - Failed to enter initial child: {}", initialChild);
                        allEntered = false;
                    }
                }
            }

            // W3C SCXML 3.3: Update ALL active parallel states' regions' currentState for deep initial targets
            updateParallelRegionCurrentStates();

            // W3C SCXML: Execute ALL deferred onentry callbacks after ALL children are entered
            for (const auto &stateId : statesForDeferredOnEntry) {
                if (onEntryCallback_) {
                    onEntryCallback_(stateId);
                }
            }

            return allEntered;
        } else {
            LOG_WARN("enterState - No initial child found for compound state: {}", stateId);
        }
    } else {
        // SCXML W3C specification: Atomic and final states are always added to active configuration
        addStateToConfiguration(stateId);

        // W3C SCXML 6.4: Defer invoke execution for atomic and final states (non-parallel, non-compound)
        // Final states can also have invokes per W3C spec
        Type stateType = stateNode->getType();
        if (stateType == Type::ATOMIC || stateType == Type::FINAL) {
            const auto &invokes = stateNode->getInvoke();
            if (!invokes.empty() && invokeDeferCallback_) {
                LOG_DEBUG("StateHierarchyManager: Deferring {} invokes for {} state: {}", invokes.size(),
                          stateType == Type::ATOMIC ? "atomic" : "final", stateId);
                invokeDeferCallback_(stateId, invokes);
            }
        }
    }

    LOG_DEBUG("enterState - Successfully entered: {}", stateId);
    return true;
}

std::string StateHierarchyManager::getCurrentState() const {
    if (activeStates_.empty()) {
        LOG_WARN("No active states");
        return "";
    }

    // SCXML W3C specification: parallel states define the current state context
    if (model_) {
        for (const auto &stateId : activeStates_) {
            auto stateNode = model_->findStateById(stateId);
            if (stateNode && stateNode->getType() == Type::PARALLEL) {
                return stateId;  // Return the parallel state as current state
            }
        }
    }

    // SCXML W3C specification: For compound states, return the most specific (atomic) state
    for (auto it = activeStates_.rbegin(); it != activeStates_.rend(); ++it) {
        const std::string &stateId = *it;
        if (model_) {
            auto stateNode = model_->findStateById(stateId);
            if (stateNode) {
                auto stateType = stateNode->getType();
                if (stateType == Type::ATOMIC || stateType == Type::FINAL) {
                    return stateId;
                }
            }
        }
    }

    // Fallback: return the last (most specific) state in the active configuration
    return activeStates_.back();
}

std::vector<std::string> StateHierarchyManager::getActiveStates() const {
    return activeStates_;
}

bool StateHierarchyManager::isStateActive(const std::string &stateId) const {
    return activeSet_.find(stateId) != activeSet_.end();
}

void StateHierarchyManager::exitState(const std::string &stateId, std::shared_ptr<IExecutionContext> executionContext) {
    if (stateId.empty()) {
        return;
    }

    LOG_DEBUG("exitState - Exiting state: {}", stateId);

    // W3C SCXML 3.13: Parallel states need conditional region deactivation (test 504)
    // Check if regions are already exited (in exit set) to avoid duplicate exit actions
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && stateNode->getType() == Type::PARALLEL) {
            auto parallelState = static_cast<ConcurrentStateNode *>(stateNode);
            const auto &regions = parallelState->getRegions();

            // Check if any region still has active states (not yet in exit set)
            bool hasActiveRegions = false;
            for (const auto &region : regions) {
                if (region && !region->getActiveStates().empty()) {
                    hasActiveRegions = true;
                    break;
                }
            }

            // Only deactivate regions if they're still active
            // If regions already exited (via exit set), skip to avoid duplicate actions
            if (hasActiveRegions) {
                LOG_DEBUG("exitState - Deactivating active regions in parallel state: {}", stateId);
                auto result = parallelState->exitParallelState(executionContext);
                if (!result.isSuccess) {
                    LOG_WARN("exitState - Warning during parallel state exit '{}': {}", stateId, result.errorMessage);
                }
            } else {
                LOG_DEBUG("exitState - Parallel state '{}' regions already exited via exit set", stateId);
            }

            // SCXML W3C: Remove the parallel state and ALL its descendants from active states
            exitParallelStateAndDescendants(stateId);
            return;
        }
    }

    // SCXML W3C: For non-parallel states, use traditional hierarchical cleanup
    exitHierarchicalState(stateId);
}

void StateHierarchyManager::reset() {
    LOG_DEBUG("Clearing all active states");
    activeStates_.clear();
    activeSet_.clear();
}

bool StateHierarchyManager::isHierarchicalModeNeeded() const {
    // Hierarchical mode is needed if there are 2 or more active states
    return activeStates_.size() > 1;
}

// Exit a parallel state by removing it and all its descendant regions
void StateHierarchyManager::exitParallelStateAndDescendants(const std::string &parallelStateId) {
    std::vector<std::string> statesToRemove;

    // SCXML W3C: Remove the parallel state itself
    auto it = std::find(activeStates_.begin(), activeStates_.end(), parallelStateId);
    if (it != activeStates_.end()) {
        statesToRemove.push_back(parallelStateId);
    } else {
        LOG_WARN(
            "StateHierarchyManager::exitParallelStateAndDescendants - Parallel state '{}' not in active configuration",
            parallelStateId);
    }

    // SCXML W3C: Remove all descendant states of the parallel state
    if (model_) {
        auto parallelNode = model_->findStateById(parallelStateId);
        if (parallelNode && parallelNode->getType() == Type::PARALLEL) {
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(parallelNode);
            if (parallelState) {
                const auto &regions = parallelState->getRegions();
                for (const auto &region : regions) {
                    if (region && region->getRootState()) {
                        std::string regionId = region->getRootState()->getId();
                        collectDescendantStates(regionId, statesToRemove);
                    }
                }
            }
        } else {
            LOG_WARN(
                "StateHierarchyManager::exitParallelStateAndDescendants - Parallel node not found or wrong type: '{}'",
                parallelStateId);
        }
    }

    // Remove all collected states
    for (const auto &state : statesToRemove) {
        removeStateFromConfiguration(state);
    }
}

// Exit a hierarchical state by removing it and all child states
void StateHierarchyManager::exitHierarchicalState(const std::string &stateId) {
    // Log current active states before exit
    std::string activeStatesStr;
    for (const auto &state : activeStates_) {
        if (!activeStatesStr.empty()) {
            activeStatesStr += ", ";
        }
        activeStatesStr += state;
    }
    LOG_DEBUG("exitHierarchicalState - Current active states: [{}]", activeStatesStr);
    LOG_DEBUG("exitHierarchicalState - Requested to exit state: {}", stateId);

    std::vector<std::string> statesToRemove;

    bool foundState = false;
    for (auto it = activeStates_.begin(); it != activeStates_.end(); ++it) {
        if (*it == stateId) {
            foundState = true;
        }
        if (foundState) {
            statesToRemove.push_back(*it);
        }
    }

    // Log what will be removed
    std::string toRemoveStr;
    for (const auto &state : statesToRemove) {
        if (!toRemoveStr.empty()) {
            toRemoveStr += ", ";
        }
        toRemoveStr += state;
    }
    LOG_DEBUG("exitHierarchicalState - Will remove {} states: [{}]", statesToRemove.size(), toRemoveStr);

    for (const auto &state : statesToRemove) {
        removeStateFromConfiguration(state);
    }

    LOG_DEBUG("exitHierarchicalState - Removed {} hierarchical states", statesToRemove.size());
}

// Recursively find all child states of a parent in the active configuration
void StateHierarchyManager::collectDescendantStates(const std::string &parentId, std::vector<std::string> &collector) {
    LOG_DEBUG("collectDescendantStates - Collecting descendants for parent: {}", parentId);

    // Add the parent state itself if it's in active states
    auto it = std::find(activeStates_.begin(), activeStates_.end(), parentId);
    if (it != activeStates_.end()) {
        collector.push_back(parentId);
        LOG_DEBUG("collectDescendantStates - Added parent state: {}", parentId);
    } else {
        LOG_DEBUG("collectDescendantStates - Parent state {} not in active states", parentId);
    }

    // Find and add all child states recursively
    if (model_) {
        auto parentNode = model_->findStateById(parentId);
        if (parentNode) {
            const auto &children = parentNode->getChildren();
            LOG_DEBUG("collectDescendantStates - Parent {} has {} children", parentId, children.size());
            for (const auto &child : children) {
                if (child) {
                    LOG_DEBUG("collectDescendantStates - Processing child: {}", child->getId());
                    collectDescendantStates(child->getId(), collector);
                }
            }
        } else {
            LOG_WARN("collectDescendantStates - Parent node not found: {}", parentId);
        }
    } else {
        LOG_WARN("collectDescendantStates - No model available");
    }

    // Find and add all child states recursively
    if (model_) {
        auto parentNode = model_->findStateById(parentId);
        if (parentNode) {
            const auto &children = parentNode->getChildren();
            for (const auto &child : children) {
                if (child) {
                    collectDescendantStates(child->getId(), collector);
                }
            }
        }
    }
}

void StateHierarchyManager::addStateToConfiguration(const std::string &stateId) {
    if (stateId.empty() || activeSet_.find(stateId) != activeSet_.end()) {
        return;  // Already active or empty ID
    }

    activeStates_.push_back(stateId);
    activeSet_.insert(stateId);

    // W3C SCXML 405: Synchronize parallel region state tracking
    // This ensures ConcurrentRegion knows about StateMachine-driven state changes
    synchronizeParallelRegionState(stateId);

    // W3C SCXML: Execute onentry actions after adding state to active configuration
    if (onEntryCallback_) {
        onEntryCallback_(stateId);

        // W3C SCXML: Check if state is still in configuration after onentry
        // Onentry actions can trigger eventless transitions that exit the state
        if (activeSet_.find(stateId) == activeSet_.end()) {
            return;  // State already removed, don't continue
        }
    } else {
        LOG_WARN("StateHierarchyManager::addStateToConfiguration - No onentry callback set for state '{}'", stateId);
    }
}

void StateHierarchyManager::addStateToConfigurationWithoutOnEntry(const std::string &stateId) {
    if (stateId.empty() || activeSet_.find(stateId) != activeSet_.end()) {
        return;  // Already active or empty ID
    }

    activeStates_.push_back(stateId);
    activeSet_.insert(stateId);
}

bool StateHierarchyManager::enterStateWithAncestors(const std::string &targetStateId, IStateNode *stopAtParent,
                                                    std::vector<std::string> *deferredOnEntryStates) {
    if (targetStateId.empty()) {
        return false;
    }

    auto targetState = model_->findStateById(targetStateId);
    if (!targetState) {
        LOG_ERROR("enterStateWithAncestors - Target state not found: {}", targetStateId);
        return false;
    }

    // W3C SCXML 3.3: Build ancestor chain from target up to (but not including) stopAtParent
    std::vector<IStateNode *> ancestorsToEnter;
    IStateNode *current = targetState;

    while (current && current != stopAtParent) {
        ancestorsToEnter.push_back(current);
        current = current->getParent();
    }

    // W3C SCXML: Collect states for deferred onentry execution
    // This prevents raised events from being processed before all states are entered
    std::vector<std::string> localStatesForOnEntry;
    std::vector<std::string> *statesForOnEntry = deferredOnEntryStates ? deferredOnEntryStates : &localStatesForOnEntry;

    // Enter ancestors from top to bottom (parent before child)
    for (auto it = ancestorsToEnter.rbegin(); it != ancestorsToEnter.rend(); ++it) {
        IStateNode *stateToEnter = *it;
        std::string stateId = stateToEnter->getId();

        // Skip if already active
        if (isStateActive(stateId)) {
            LOG_DEBUG("enterStateWithAncestors - State already active, skipping: {}", stateId);
            continue;
        }

        // W3C SCXML 3.3: Handle parallel states specially - need to activate regions
        Type stateType = stateToEnter->getType();
        if (stateType == Type::PARALLEL) {
            // Add parallel state to configuration without onentry
            addStateToConfigurationWithoutOnEntry(stateId);
            LOG_DEBUG("enterStateWithAncestors - Entered parallel ancestor: {}", stateId);

            // W3C SCXML 3.4: Activate parallel state regions
            // This is essential for event processing to work correctly
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateToEnter);
            assert(parallelState && "SCXML violation: PARALLEL type state must be ConcurrentStateNode");

            // Set callbacks for regions before activation
            const auto &regions = parallelState->getRegions();
            if (invokeDeferCallback_) {
                for (const auto &region : regions) {
                    if (region) {
                        region->setInvokeCallback(invokeDeferCallback_);
                    }
                }
            }
            if (conditionEvaluator_) {
                for (const auto &region : regions) {
                    if (region) {
                        region->setConditionEvaluator(conditionEvaluator_);
                    }
                }
            }

            // Activate regions (but don't enter initial states - deep targets will handle that)
            auto result = parallelState->enterParallelState();
            if (!result.isSuccess) {
                LOG_ERROR("enterStateWithAncestors - Failed to activate parallel state regions: {}", stateId);
                return false;
            }

            // W3C SCXML: Defer onentry execution
            statesForOnEntry->push_back(stateId);
        } else {
            // For non-parallel states, just add to configuration
            addStateToConfigurationWithoutOnEntry(stateId);
            LOG_DEBUG("enterStateWithAncestors - Entered ancestor/target: {}", stateId);

            // W3C SCXML: Defer onentry execution
            statesForOnEntry->push_back(stateId);

            // W3C SCXML 6.4: Defer invoke execution for compound/atomic/final states
            if (stateType == Type::COMPOUND || stateType == Type::ATOMIC || stateType == Type::FINAL) {
                const auto &invokes = stateToEnter->getInvoke();
                if (!invokes.empty() && invokeDeferCallback_) {
                    LOG_DEBUG("enterStateWithAncestors: Deferring {} invokes for {} state: {}", invokes.size(),
                              stateType == Type::COMPOUND ? "compound"
                                                          : (stateType == Type::ATOMIC ? "atomic" : "final"),
                              stateId);
                    invokeDeferCallback_(stateId, invokes);
                }
            }

            // W3C SCXML 3.3: If target state is compound, recursively enter its initial child
            // This implements the W3C algorithm: compound states MUST enter their initial children
            if (stateType == Type::COMPOUND && stateToEnter == targetState) {
                std::string initialChild = findInitialChildState(stateToEnter);
                if (!initialChild.empty()) {
                    LOG_DEBUG("W3C SCXML 3.3: Target {} is compound, recursively entering initial child: {}", stateId,
                              initialChild);
                    // Recursive call - automatically handles nested compound states
                    if (!enterStateWithAncestors(initialChild, stateToEnter, statesForOnEntry)) {
                        LOG_ERROR("enterStateWithAncestors - Failed to enter initial child {} of compound target {}",
                                  initialChild, stateId);
                        return false;
                    }
                }
            }
            // W3C SCXML 3.4: If target state is parallel, enter ALL children
            // Parallel states require all child regions to be active simultaneously
            else if (stateType == Type::PARALLEL && stateToEnter == targetState) {
                const auto &children = stateToEnter->getChildren();
                LOG_DEBUG("W3C SCXML 3.4: Target {} is parallel, entering all {} children", stateId, children.size());

                for (const auto &child : children) {
                    if (child) {
                        const std::string &childId = child->getId();
                        LOG_DEBUG("W3C SCXML 3.4: Parallel state {} entering child: {}", stateId, childId);

                        // Recursive call - each child may itself be compound or parallel
                        if (!enterStateWithAncestors(childId, stateToEnter, statesForOnEntry)) {
                            LOG_ERROR("enterStateWithAncestors - Failed to enter parallel child {} of target {}",
                                      childId, stateId);
                            return false;
                        }
                    }
                }
            }
        }
    }

    // W3C SCXML 3.3: Update ALL active parallel states' regions' currentState for deep initial targets
    updateParallelRegionCurrentStates();

    // W3C SCXML: Execute onentry actions AFTER all states are entered (only if not deferring to caller)
    // This ensures raised events are processed only when all states are in configuration
    if (!deferredOnEntryStates) {
        for (const auto &stateId : localStatesForOnEntry) {
            if (onEntryCallback_) {
                onEntryCallback_(stateId);
            }
        }
    }

    return true;
}

void StateHierarchyManager::removeStateFromConfiguration(const std::string &stateId) {
    if (stateId.empty()) {
        return;
    }

    // Remove from vector
    auto it = std::find(activeStates_.begin(), activeStates_.end(), stateId);
    if (it != activeStates_.end()) {
        activeStates_.erase(it);
    }

    // Remove from set
    activeSet_.erase(stateId);

    LOG_DEBUG("removeStateFromConfiguration - Removed: {}", stateId);
}

std::string StateHierarchyManager::findInitialChildState(IStateNode *stateNode) const {
    if (!stateNode) {
        return "";
    }

    // 1. Check explicit initial attribute
    std::string explicitInitial = stateNode->getInitialState();
    if (!explicitInitial.empty()) {
        LOG_DEBUG("findInitialChildState - Found explicit initial: {}", explicitInitial);
        return explicitInitial;
    }

    // 2. Use first child state (default)
    const auto &children = stateNode->getChildren();
    if (!children.empty() && children[0]) {
        std::string defaultInitial = children[0]->getId();
        LOG_DEBUG("findInitialChildState - Using default initial: {}", defaultInitial);
        return defaultInitial;
    }

    LOG_DEBUG("No child states found");
    return "";
}

bool StateHierarchyManager::isCompoundState(IStateNode *stateNode) const {
    if (!stateNode) {
        return false;
    }

    // SCXML W3C specification: only COMPOUND types are compound states, not PARALLEL
    // Parallel states have different semantics and should not auto-enter children
    return stateNode->getType() == Type::COMPOUND;
}

bool StateHierarchyManager::isStateDescendantOf(IStateNode *rootState, const std::string &stateId) const {
    if (!rootState) {
        return false;
    }

    // Check if root itself is the target
    if (rootState->getId() == stateId) {
        return true;
    }

    // Recursively check all children
    const auto &children = rootState->getChildren();
    for (const auto &child : children) {
        if (child && isStateDescendantOf(child.get(), stateId)) {
            return true;
        }
    }

    return false;
}

void StateHierarchyManager::synchronizeParallelRegionState(const std::string &stateId) {
    // W3C SCXML 405: Synchronize ConcurrentRegion state tracking after StateMachine transitions
    // When StateMachine processes eventless transitions inside parallel regions,
    // the regions don't know about the state changes and keep stale activeStates_.
    // This causes duplicate onexit execution during parallel state exit.

    if (!model_ || stateId.empty()) {
        return;
    }

    auto stateNode = model_->findStateById(stateId);
    if (!stateNode || !stateNode->getParent()) {
        return;
    }

    // Find the parallel state ancestor (if any)
    IStateNode *current = stateNode->getParent();
    while (current) {
        if (current->getType() == Type::PARALLEL) {
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(current);
            if (parallelState) {
                const auto &regions = parallelState->getRegions();
                for (const auto &region : regions) {
                    if (region && region->getRootState()) {
                        // Check if stateId belongs to this region
                        if (isStateDescendantOf(region->getRootState().get(), stateId)) {
                            region->setCurrentState(stateId);
                            LOG_DEBUG("W3C SCXML 405: Synchronized region '{}' currentState to '{}'", region->getId(),
                                      stateId);
                            break;  // Found the region, no need to check others
                        }
                    }
                }
            }
            // Only synchronize the immediate parallel parent, not ancestors
            break;
        }
        current = current->getParent();
    }
}

void StateHierarchyManager::setOnEntryCallback(std::function<void(const std::string &)> callback) {
    onEntryCallback_ = callback;
    LOG_DEBUG("OnEntry callback set for StateHierarchyManager");
}

void StateHierarchyManager::setInvokeDeferCallback(
    std::function<void(const std::string &, const std::vector<std::shared_ptr<IInvokeNode>> &)> callback) {
    invokeDeferCallback_ = callback;
    LOG_DEBUG("StateHierarchyManager: Invoke defer callback set for W3C SCXML 6.4 compliance");
}

void StateHierarchyManager::setConditionEvaluator(std::function<bool(const std::string &)> evaluator) {
    conditionEvaluator_ = evaluator;
    LOG_DEBUG("StateHierarchyManager: Condition evaluator callback set for W3C SCXML transition guard compliance");
}

void StateHierarchyManager::setExecutionContext(std::shared_ptr<IExecutionContext> context) {
    executionContext_ = context;

    if (!executionContext_) {
        LOG_WARN("StateHierarchyManager: ExecutionContext set to null - parallel state regions will not be able to "
                 "execute transition actions (W3C SCXML 403c may fail)");
        return;
    }

    // W3C SCXML 403c: Update executionContext for ALL existing parallel state regions
    // This handles the case where parallel states were created before executionContext was available
    if (model_) {
        const auto &allStates = model_->getAllStates();
        for (const auto &state : allStates) {
            if (state && state->getType() == Type::PARALLEL) {
                auto parallelState = dynamic_cast<ConcurrentStateNode *>(state.get());
                if (parallelState) {
                    updateRegionExecutionContexts(parallelState);
                }
            }
        }
    }

    LOG_DEBUG("StateHierarchyManager: ExecutionContext set for concurrent region action execution (W3C SCXML 403c "
              "compliance)");
}

void StateHierarchyManager::setInitialTransitionCallback(
    std::function<void(const std::vector<std::shared_ptr<IActionNode>> &)> callback) {
    initialTransitionCallback_ = callback;
    LOG_DEBUG("StateHierarchyManager: Initial transition callback set for W3C SCXML 3.13 compliance");
}

void StateHierarchyManager::setEnterStateCallback(std::function<bool(const std::string &)> callback) {
    enterStateCallback_ = callback;
    LOG_DEBUG("StateHierarchyManager: Enter state callback set for W3C SCXML 3.10 history compliance");
}

void StateHierarchyManager::setHistoryManager(HistoryManager *historyManager) {
    historyManager_ = historyManager;
    LOG_DEBUG("StateHierarchyManager: History manager set for W3C SCXML 3.10 direct restoration");
}

void StateHierarchyManager::updateRegionExecutionContexts(ConcurrentStateNode *parallelState) {
    // W3C SCXML 403c: DRY principle - centralized executionContext management for parallel state regions
    if (!parallelState || !executionContext_) {
        if (!executionContext_) {
            LOG_WARN("StateHierarchyManager: Cannot update region executionContexts - executionContext is null");
        }
        return;
    }

    const auto &regions = parallelState->getRegions();
    for (const auto &region : regions) {
        if (region) {
            region->setExecutionContext(executionContext_);
            LOG_DEBUG(
                "StateHierarchyManager: Set executionContext for region '{}' in parallel state '{}' (W3C SCXML 403c)",
                region->getId(), parallelState->getId());
        }
    }
}

void StateHierarchyManager::updateParallelRegionCurrentStates() {
    // W3C SCXML 3.3: Update parallel region currentState for deep initial targets
    // When deep targets bypass normal region initialization, we must sync region state
    //
    // Performance optimization: Single-pass algorithm O(n*depth) instead of O(n²*depth)
    // We traverse active states once, building a map of region -> deepest state

    if (!model_) {
        return;
    }

    // Map: region ID -> deepest active state ID within that region
    std::unordered_map<std::string, std::string> regionDeepestState;

    // Single pass through active states (reverse order to find deepest first)
    for (auto it = activeStates_.rbegin(); it != activeStates_.rend(); ++it) {
        const std::string &stateId = *it;
        auto stateNode = model_->findStateById(stateId);
        if (!stateNode) {
            continue;
        }

        // Walk up the parent chain to find which region(s) this state belongs to
        IStateNode *current = stateNode;
        while (current) {
            IStateNode *parent = current->getParent();
            if (!parent) {
                break;
            }

            // Check if parent is a parallel state
            if (parent->getType() == Type::PARALLEL) {
                auto parallelState = dynamic_cast<ConcurrentStateNode *>(parent);
                if (parallelState) {
                    // Find which region 'current' belongs to
                    const auto &regions = parallelState->getRegions();
                    for (const auto &region : regions) {
                        if (region && region->getRootState()) {
                            IStateNode *regionRoot = region->getRootState().get();

                            // Check if stateNode is the region's root or is descended from it
                            // Walk up from stateNode to see if we reach regionRoot
                            bool isInRegion = false;
                            IStateNode *check = stateNode;
                            while (check) {
                                if (check == regionRoot) {
                                    isInRegion = true;
                                    break;
                                }
                                check = check->getParent();
                            }

                            if (isInRegion) {
                                const std::string &regionId = region->getId();
                                // Only update if not already set (we're iterating deepest-first)
                                if (regionDeepestState.find(regionId) == regionDeepestState.end()) {
                                    regionDeepestState[regionId] = stateId;
                                }
                                break;  // Found the region, no need to check others
                            }
                        }
                    }
                }
            }
            current = parent;
        }
    }

    // Now update all region currentStates based on collected data
    for (const auto &activeStateId : activeStates_) {
        auto activeStateNode = model_->findStateById(activeStateId);
        if (activeStateNode && activeStateNode->getType() == Type::PARALLEL) {
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(activeStateNode);
            if (parallelState) {
                const auto &regions = parallelState->getRegions();
                for (const auto &region : regions) {
                    if (region) {
                        const std::string &regionId = region->getId();
                        auto it = regionDeepestState.find(regionId);
                        if (it != regionDeepestState.end()) {
                            const std::string &deepestState = it->second;
                            if (deepestState != region->getCurrentState()) {
                                region->setCurrentState(deepestState);
                                LOG_DEBUG("Updated region {} currentState to deep target: {}", regionId, deepestState);
                            }
                        }
                    }
                }
            }
        }
    }
}

}  // namespace RSM