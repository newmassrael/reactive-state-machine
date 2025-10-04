#include "runtime/StateHierarchyManager.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include "model/SCXMLModel.h"
#include "states/ConcurrentStateNode.h"
#include <algorithm>

namespace RSM {

StateHierarchyManager::StateHierarchyManager(std::shared_ptr<SCXMLModel> model) : model_(model) {
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

    // SCXML W3C specification section 3.4: parallel states behave differently from compound states
    if (stateNode->getType() == Type::PARALLEL) {
        // 상태를 활성 구성에 추가 (parallel states are always added)
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
                    LOG_DEBUG("StateHierarchyManager: Set condition evaluator for region: {} BEFORE activation",
                              region->getId());
                }
            }
        }

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
                LOG_WARN("TEST364: Parallel state '{}' region '{}' rootState initialState='{}'", stateId, regionStateId,
                         initialChild);
                if (initialChild.empty()) {
                    // SCXML W3C: Use first child as default initial state
                    initialChild = children[0]->getId();
                    LOG_WARN("TEST364: Parallel state '{}' region '{}' using first child fallback: '{}'", stateId,
                             regionStateId, initialChild);
                }

                addStateToConfiguration(initialChild);
                LOG_WARN("TEST364: Parallel state '{}' region '{}' added initial child to configuration: '{}'", stateId,
                         regionStateId, initialChild);

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
            // W3C SCXML 3.3: Pre-process deep initial targets to set desired initial children for parallel regions
            // This implements the algorithm: if descendant already in statesToEnter, skip default entry
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
                                    LOG_WARN("TEST364: Set region '{}' desiredInitialChild='{}' from parent state '{}' "
                                             "initial attribute",
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

                // W3C SCXML 3.3: For deep initial targets (not direct children), enter all ancestors
                auto childState = model_->findStateById(initialChild);
                if (childState && childState->getParent() != stateNode) {
                    // Deep target - need to enter intermediate ancestors
                    LOG_DEBUG("enterState - Deep initial target detected, entering ancestors for: {}", initialChild);
                    if (!enterStateWithAncestors(initialChild, stateNode, &statesForDeferredOnEntry)) {
                        LOG_ERROR("enterState - Failed to enter ancestors for: {}", initialChild);
                        allEntered = false;
                    }
                } else {
                    // Direct child - use normal recursive entry
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
    LOG_DEBUG("getCurrentState - Active states count: {}", std::to_string(activeStates_.size()));

    if (activeStates_.empty()) {
        LOG_DEBUG("No active states, returning empty");
        return "";
    }

    // Debug output: log each active state in the configuration with more detail
    LOG_DEBUG("getCurrentState - DETAILED ACTIVE STATES DUMP:");
    for (size_t i = 0; i < activeStates_.size(); ++i) {
        std::string stateId = activeStates_[i];
        std::string typeInfo = "unknown";
        if (model_) {
            auto stateNode = model_->findStateById(stateId);
            if (stateNode) {
                auto stateType = stateNode->getType();
                switch (stateType) {
                case Type::PARALLEL:
                    typeInfo = "PARALLEL";
                    break;
                case Type::COMPOUND:
                    typeInfo = "COMPOUND";
                    break;
                case Type::ATOMIC:
                    typeInfo = "ATOMIC";
                    break;
                case Type::FINAL:
                    typeInfo = "FINAL";
                    break;
                default:
                    typeInfo = "OTHER(" + std::to_string(static_cast<int>(stateType)) + ")";
                    break;
                }
            }
        }
        LOG_DEBUG("getCurrentState - Active state[{}]: {} (type: {})", i, stateId, typeInfo);
    }

    // SCXML W3C specification: parallel states define the current state context
    // Find the first parallel state in the active configuration
    if (model_) {
        for (const auto &stateId : activeStates_) {
            auto stateNode = model_->findStateById(stateId);
            if (stateNode) {
                auto stateType = stateNode->getType();
                LOG_DEBUG("getCurrentState - State: {}, Type: {} (PARALLEL={})", stateId, static_cast<int>(stateType),
                          static_cast<int>(Type::PARALLEL));

                if (stateType == Type::PARALLEL) {
                    LOG_DEBUG("getCurrentState - Found parallel state, returning: {}", stateId);
                    return stateId;  // Return the parallel state as current state
                }
            } else {
                LOG_WARN("getCurrentState - State node not found for: {}", stateId);
            }
        }
    }

    // SCXML W3C specification: For compound states, return the most specific (atomic) state
    // Skip compound states and return the deepest atomic state in the configuration
    for (auto it = activeStates_.rbegin(); it != activeStates_.rend(); ++it) {
        const std::string &stateId = *it;
        if (model_) {
            auto stateNode = model_->findStateById(stateId);
            if (stateNode) {
                auto stateType = stateNode->getType();
                // Return atomic states (leaf states) or final states as current state
                if (stateType == Type::ATOMIC || stateType == Type::FINAL) {
                    LOG_DEBUG("getCurrentState - Found atomic/final state, returning: {}", stateId);
                    return stateId;
                }
            }
        }
    }

    // Fallback: return the last (most specific) state in the active configuration
    std::string result = activeStates_.back();
    LOG_DEBUG("getCurrentState - No atomic state found, returning last state: {}", result);
    return result;
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

    // SCXML W3C specification section 3.4: parallel states need special exit handling
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && stateNode->getType() == Type::PARALLEL) {
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(stateNode);
            if (parallelState) {
                LOG_DEBUG("exitState - Exiting parallel state with region deactivation: {}", stateId);
                auto result = parallelState->exitParallelState(executionContext);
                if (!result.isSuccess) {
                    LOG_WARN("exitState - Warning during parallel state exit '{}': {}", stateId, result.errorMessage);
                }
            }
        }
    }

    // Use specialized exit logic for parallel states vs hierarchical states
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode && stateNode->getType() == Type::PARALLEL) {
            // SCXML W3C: For parallel states, remove the parallel state and ALL its descendants
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
    // 활성 상태가 2개 이상이면 계층적 모드가 필요
    return activeStates_.size() > 1;
}

// Exit a parallel state by removing it and all its descendant regions
void StateHierarchyManager::exitParallelStateAndDescendants(const std::string &parallelStateId) {
    LOG_DEBUG("exitParallelStateAndDescendants - Starting removal of parallel state: {} and descendants",
              parallelStateId);
    LOG_DEBUG("exitParallelStateAndDescendants - Active states before removal: {} total", activeStates_.size());

    std::vector<std::string> statesToRemove;

    // SCXML W3C: Remove the parallel state itself
    auto it = std::find(activeStates_.begin(), activeStates_.end(), parallelStateId);
    if (it != activeStates_.end()) {
        statesToRemove.push_back(parallelStateId);
        LOG_DEBUG("exitParallelStateAndDescendants - Added parallel state to removal list: {}", parallelStateId);
    } else {
        LOG_WARN("exitParallelStateAndDescendants - Parallel state {} not found in active states", parallelStateId);
    }

    // SCXML W3C: Remove all descendant states of the parallel state
    if (model_) {
        auto parallelNode = model_->findStateById(parallelStateId);
        if (parallelNode && parallelNode->getType() == Type::PARALLEL) {
            LOG_DEBUG("exitParallelStateAndDescendants - Found parallel node, collecting regions");
            auto parallelState = dynamic_cast<ConcurrentStateNode *>(parallelNode);
            if (parallelState) {
                const auto &regions = parallelState->getRegions();
                LOG_DEBUG("exitParallelStateAndDescendants - Processing {} regions", regions.size());
                for (const auto &region : regions) {
                    if (region && region->getRootState()) {
                        // Remove region root state and its children
                        std::string regionId = region->getRootState()->getId();
                        LOG_DEBUG("exitParallelStateAndDescendants - Collecting descendants for region: {}", regionId);
                        size_t beforeSize = statesToRemove.size();
                        collectDescendantStates(regionId, statesToRemove);
                        LOG_DEBUG("exitParallelStateAndDescendants - Added {} states from region {}",
                                  statesToRemove.size() - beforeSize, regionId);
                    }
                }
            }
        } else {
            LOG_WARN("exitParallelStateAndDescendants - Parallel node not found or wrong type for: {}",
                     parallelStateId);
        }
    }

    LOG_DEBUG("exitParallelStateAndDescendants - Total states to remove: {}", statesToRemove.size());
    for (const auto &state : statesToRemove) {
        LOG_DEBUG("exitParallelStateAndDescendants - Removing state: {}", state);
    }

    // Remove all collected states
    for (const auto &state : statesToRemove) {
        removeStateFromConfiguration(state);
    }

    LOG_DEBUG("exitParallelStateAndDescendants - Active states after removal: {} total", activeStates_.size());
    LOG_DEBUG("exitParallelStateAndDescendants - Removed {} parallel states", statesToRemove.size());
}

// Exit a hierarchical state by removing it and all child states
void StateHierarchyManager::exitHierarchicalState(const std::string &stateId) {
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
        return;  // 이미 활성 상태이거나 빈 ID
    }

    activeStates_.push_back(stateId);
    activeSet_.insert(stateId);

    // W3C SCXML: Execute onentry actions after adding state to active configuration
    if (onEntryCallback_) {
        LOG_DEBUG("addStateToConfiguration - Calling onentry callback for state: {}", stateId);
        onEntryCallback_(stateId);
        LOG_DEBUG("addStateToConfiguration - Onentry callback completed for state: {}", stateId);
    } else {
        LOG_WARN("addStateToConfiguration - No onentry callback set for state: {}", stateId);
    }

    // Check state type for debugging
    std::string typeInfo = "unknown";
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode) {
            auto stateType = stateNode->getType();
            typeInfo = std::to_string(static_cast<int>(stateType));
            if (stateType == Type::PARALLEL) {
                typeInfo += "(PARALLEL)";
            } else if (stateType == Type::FINAL) {
                typeInfo += "(FINAL)";
            } else if (stateType == Type::COMPOUND) {
                typeInfo += "(COMPOUND)";
            } else if (stateType == Type::ATOMIC) {
                typeInfo += "(ATOMIC)";
            }
        }
    }

    LOG_DEBUG("addStateToConfiguration - Added: {} type={} (total active: {})", stateId, typeInfo,
              activeStates_.size());

    // Log current state order for debugging
    std::string stateOrder = "Current order: [";
    for (size_t i = 0; i < activeStates_.size(); ++i) {
        if (i > 0) {
            stateOrder += ", ";
        }
        stateOrder += activeStates_[i];
    }
    stateOrder += "]";
    LOG_DEBUG("addStateToConfiguration - {}", stateOrder);
    LOG_WARN("TEST364: Configuration updated - {}", stateOrder);
}

void StateHierarchyManager::addStateToConfigurationWithoutOnEntry(const std::string &stateId) {
    if (stateId.empty() || activeSet_.find(stateId) != activeSet_.end()) {
        return;  // Already active or empty ID
    }

    activeStates_.push_back(stateId);
    activeSet_.insert(stateId);

    // Check state type for debugging
    std::string typeInfo = "unknown";
    if (model_) {
        auto stateNode = model_->findStateById(stateId);
        if (stateNode) {
            auto stateType = stateNode->getType();
            typeInfo = std::to_string(static_cast<int>(stateType));
            if (stateType == Type::PARALLEL) {
                typeInfo += "(PARALLEL)";
            } else if (stateType == Type::FINAL) {
                typeInfo += "(FINAL)";
            } else if (stateType == Type::COMPOUND) {
                typeInfo += "(COMPOUND)";
            } else if (stateType == Type::ATOMIC) {
                typeInfo += "(ATOMIC)";
            }
        }
    }

    LOG_DEBUG("addStateToConfigurationWithoutOnEntry - Added: {} type={} (total active: {})", stateId, typeInfo,
              activeStates_.size());
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

    // 벡터에서 제거
    auto it = std::find(activeStates_.begin(), activeStates_.end(), stateId);
    if (it != activeStates_.end()) {
        activeStates_.erase(it);
    }

    // 세트에서 제거
    activeSet_.erase(stateId);

    LOG_DEBUG("removeStateFromConfiguration - Removed: {}", stateId);
}

std::string StateHierarchyManager::findInitialChildState(IStateNode *stateNode) const {
    if (!stateNode) {
        return "";
    }

    // 1. 명시적 initial 속성 확인
    std::string explicitInitial = stateNode->getInitialState();
    if (!explicitInitial.empty()) {
        LOG_DEBUG("findInitialChildState - Found explicit initial: {}", explicitInitial);
        return explicitInitial;
    }

    // 2. 첫 번째 자식 상태 사용 (기본값)
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