#include "states/ConcurrentRegion.h"
#include "actions/AssignAction.h"
#include "actions/ScriptAction.h"
#include "common/Logger.h"
#include "events/EventDescriptor.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"
#include "states/StateExitExecutor.h"
#include <algorithm>

namespace RSM {

ConcurrentRegion::ConcurrentRegion(const std::string &id, std::shared_ptr<IStateNode> rootState,
                                   std::shared_ptr<IExecutionContext> executionContext)
    : id_(id), status_(ConcurrentRegionStatus::INACTIVE), rootState_(rootState), executionContext_(executionContext),
      isInFinalState_(false), exitHandler_(std::make_shared<StateExitExecutor>()) {
    // SCXML W3C specification section 3.4: regions must have valid identifiers
    assert(!id_.empty() && "SCXML violation: concurrent region must have non-empty ID");

    LOG_DEBUG("Creating region: {}", id_);

    if (rootState_) {
        LOG_DEBUG("Root state provided: {}", rootState_->getId());
    } else {
        LOG_DEBUG("No root state provided (will be set later)");
    }
}

ConcurrentRegion::~ConcurrentRegion() {
    LOG_DEBUG("Destroying region: {}", id_);

    // Clean deactivation if still active
    if (status_ == ConcurrentRegionStatus::ACTIVE) {
        LOG_DEBUG("Deactivating region during destruction");
        deactivate(nullptr);
    }
}

const std::string &ConcurrentRegion::getId() const {
    return id_;
}

ConcurrentOperationResult ConcurrentRegion::activate() {
    if (status_ == ConcurrentRegionStatus::ACTIVE) {
        LOG_DEBUG("Region {} already active", id_);
        return ConcurrentOperationResult::success(id_);
    }

    // SCXML W3C specification section 3.4: regions must have root states
    if (!rootState_) {
        std::string error = fmt::format("SCXML violation: cannot activate region '{}' without root state. SCXML "
                                        "specification requires regions to have states.",
                                        id_);
        LOG_ERROR("Activate error: {}", error);
        setErrorState(error);
        return ConcurrentOperationResult::failure(id_, error);
    }

    // Validate root state before activation
    if (!validateRootState()) {
        std::string error = fmt::format("Root state validation failed for region: {}", id_);
        LOG_ERROR("Root state validation failed: {}", error);
        setErrorState(error);
        return ConcurrentOperationResult::failure(id_, error);
    }

    LOG_DEBUG("Activating region: {}", id_);

    // Mark region as active before entering initial state to enable final state detection
    status_ = ConcurrentRegionStatus::ACTIVE;

    // Enter initial state according to SCXML semantics
    auto result = enterInitialState();
    if (!result.isSuccess) {
        LOG_ERROR("Failed to enter initial state: {}", result.errorMessage);
        status_ = ConcurrentRegionStatus::ERROR;  // Rollback on failure
        setErrorState(result.errorMessage);
        return result;
    }
    updateCurrentState();

    LOG_DEBUG("Successfully activated region: {}", id_);
    return ConcurrentOperationResult::success(id_);
}

ConcurrentOperationResult ConcurrentRegion::deactivate(std::shared_ptr<IExecutionContext> executionContext) {
    if (status_ == ConcurrentRegionStatus::INACTIVE) {
        LOG_DEBUG("Region {} already inactive", id_);
        return ConcurrentOperationResult::success(id_);
    }

    // W3C SCXML 3.13: If activeStates_ is already empty, region was exited via exit set
    // Skip exitAllStates to avoid duplicate exit action execution (test 504)
    if (activeStates_.empty()) {
        LOG_DEBUG("Region {} activeStates already empty, skipping exitAllStates", id_);
        status_ = ConcurrentRegionStatus::INACTIVE;
        currentState_.clear();
        isInFinalState_ = false;
        LOG_DEBUG("Successfully deactivated region: {}", id_);
        return ConcurrentOperationResult::success(id_);
    }

    LOG_DEBUG("Deactivating region: {}", id_);

    // Exit all active states
    auto result = exitAllStates(executionContext);
    if (!result.isSuccess) {
        LOG_WARN("Warning during state exit: {}", result.errorMessage);
        // Continue with deactivation even if exit has issues
    }

    status_ = ConcurrentRegionStatus::INACTIVE;
    currentState_.clear();
    activeStates_.clear();
    isInFinalState_ = false;

    LOG_DEBUG("Successfully deactivated region: {}", id_);
    return ConcurrentOperationResult::success(id_);
}

bool ConcurrentRegion::isActive() const {
    return status_ == ConcurrentRegionStatus::ACTIVE;
}

bool ConcurrentRegion::isInFinalState() const {
    return isInFinalState_ && status_ == ConcurrentRegionStatus::FINAL;
}

ConcurrentRegionStatus ConcurrentRegion::getStatus() const {
    return status_;
}

ConcurrentRegionInfo ConcurrentRegion::getInfo() const {
    ConcurrentRegionInfo info;
    info.id = id_;
    info.status = status_;
    info.currentState = currentState_;
    info.isInFinalState = isInFinalState_;
    info.activeStates = activeStates_;
    return info;
}

ConcurrentOperationResult ConcurrentRegion::processEvent(const EventDescriptor &event) {
    if (status_ != ConcurrentRegionStatus::ACTIVE) {
        std::string error = fmt::format("Cannot process event in inactive region: {}", id_);
        LOG_WARN("processEvent - {}", error);
        return ConcurrentOperationResult::failure(id_, error);
    }

    if (!rootState_) {
        std::string error = fmt::format("SCXML violation: cannot process event without root state in region: {}", id_);
        LOG_ERROR("Error: {}", error);
        setErrorState(error);
        return ConcurrentOperationResult::failure(id_, error);
    }

    LOG_DEBUG("Processing event '{}' in region: {}", event.eventName, id_);

    // SCXML W3C specification section 3.4: Process event in region's current state
    if (!currentState_.empty()) {
        // Find current state node and check for event transitions
        std::shared_ptr<IStateNode> stateNode = nullptr;
        if (currentState_ == rootState_->getId()) {
            stateNode = rootState_;
        } else {
            // Search in child states
            const auto &children = rootState_->getChildren();
            for (const auto &child : children) {
                if (child->getId() == currentState_) {
                    stateNode = child;
                    break;
                }
            }
        }

        if (stateNode) {
            const auto &transitions = stateNode->getTransitions();

            // SCXML W3C compliant transition processing
            for (const auto &transition : transitions) {
                // W3C SCXML 3.13: Wildcard event matching - "*" matches any event
                std::string transitionEvent = transition->getEvent();
                bool eventMatches = (transitionEvent == event.eventName) || (transitionEvent == "*");

                if (transitionEvent == "*") {
                    LOG_DEBUG("ConcurrentRegion: Wildcard transition found in {} - event='{}' matches '{}'", id_,
                              event.eventName, transitionEvent);
                }

                if (eventMatches) {
                    LOG_DEBUG("ConcurrentRegion: Transition matched in {} - transition event='{}', incoming event='{}'",
                              id_, transitionEvent, event.eventName);
                    // W3C SCXML: Evaluate guard condition before executing transition
                    std::string guard = transition->getGuard();
                    bool conditionResult = true;  // Default to true if no guard condition

                    if (!guard.empty()) {
                        if (conditionEvaluator_) {
                            conditionResult = conditionEvaluator_(guard);
                            LOG_DEBUG(
                                "ConcurrentRegion: Evaluated guard condition '{}' for transition: {} -> result: {}",
                                guard, event.eventName, conditionResult ? "true" : "false");
                        } else {
                            LOG_WARN("ConcurrentRegion: Guard condition '{}' present but no evaluator set, defaulting "
                                     "to true",
                                     guard);
                        }
                    }

                    // Only execute transition if condition is true
                    if (!conditionResult) {
                        LOG_DEBUG("ConcurrentRegion: Skipping transition due to false guard condition: {}", guard);
                        continue;  // Try next transition
                    }

                    // W3C SCXML: Execute transition actions for ALL transitions (with or without targets)
                    // Targetless transitions are internal transitions that execute actions without changing state
                    const auto &actionNodes = transition->getActionNodes();
                    if (!actionNodes.empty()) {
                        // P1 refactoring: Use helper method for DRY principle
                        std::string actionContext = fmt::format("transition event='{}'", transitionEvent);
                        executeActionNodes(actionNodes, actionContext);
                    }

                    const auto &targets = transition->getTargets();
                    if (!targets.empty()) {
                        std::string targetState = targets[0];

                        // W3C SCXML: Check if target is outside this region's scope
                        // Transitions to states outside the region must be handled by parent (StateMachine)
                        // Use recursive descendant check to handle deeply nested states
                        bool isTargetInRegion = isDescendantOf(rootState_, targetState);

                        if (!isTargetInRegion) {
                            LOG_DEBUG("ConcurrentRegion: Transition target '{}' is outside region '{}' - "
                                      "external/cross-region transition",
                                      targetState, id_);

                            // W3C SCXML 3.13: Actions already executed above for ALL transitions
                            // W3C SCXML 3.4: Return external transition info so parent can execute it
                            // Parent (ConcurrentStateNode) will determine if this is cross-region or truly external
                            return ConcurrentOperationResult::externalTransition(id_, targetState, event.eventName,
                                                                                 currentState_);
                        }

                        LOG_DEBUG("Executing transition: {} -> {} on event: {}", currentState_, targetState,
                                  event.eventName);

                        // W3C SCXML: Actions already executed above for ALL transitions
                        // Update current state
                        currentState_ = targetState;
                        LOG_DEBUG("Updated current state to: {}", currentState_);

                        // SCXML W3C compliance: Execute entry actions for target state
                        if (executionContext_) {
                            // Find target state in children and execute entry actions
                            const auto &children = rootState_->getChildren();
                            for (const auto &child : children) {
                                if (child && child->getId() == targetState) {
                                    // W3C SCXML 3.8: Execute entry action blocks
                                    const auto &entryBlocks = child->getEntryActionBlocks();
                                    if (!entryBlocks.empty()) {
                                        LOG_DEBUG(
                                            "W3C SCXML 3.8: Executing {} entry action blocks for target state: {}",
                                            entryBlocks.size(), targetState);

                                        for (const auto &actionBlock : entryBlocks) {
                                            for (const auto &actionNode : actionBlock) {
                                                if (!actionNode) {
                                                    LOG_WARN("Null entry ActionNode "
                                                             "encountered, skipping");
                                                    continue;
                                                }

                                                try {
                                                    LOG_DEBUG("Executing entry "
                                                              "ActionNode: {} (ID: {})",
                                                              actionNode->getActionType(), actionNode->getId());
                                                    if (actionNode->execute(*executionContext_)) {
                                                        LOG_DEBUG("Successfully executed "
                                                                  "entry ActionNode: {}",
                                                                  actionNode->getActionType());
                                                    } else {
                                                        LOG_WARN("ConcurrentRegion::processEvent - Entry ActionNode "
                                                                 "failed: {}",
                                                                 actionNode->getActionType());
                                                    }
                                                } catch (const std::exception &e) {
                                                    LOG_WARN("Entry ActionNode exception: "
                                                             "{} Error: {}",
                                                             actionNode->getActionType(), e.what());
                                                }
                                            }
                                        }
                                    }
                                    break;  // Exit loop since target state found
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    updateCurrentState();

    // Check if we've reached a final state after event processing
    if (determineIfInFinalState()) {
        isInFinalState_ = true;
        status_ = ConcurrentRegionStatus::FINAL;
        LOG_DEBUG("Region {} reached final state", id_);

        // W3C SCXML 3.4 test 570: Generate done.state.{parentId} event when region reaches final state
        // The region ID corresponds to the parent composite state (e.g., p0s1)
        if (doneStateCallback_) {
            doneStateCallback_(id_);
            LOG_DEBUG("Invoked done state callback for region: {}", id_);
        }
    }

    return ConcurrentOperationResult::success(id_);
}

std::shared_ptr<IStateNode> ConcurrentRegion::getRootState() const {
    return rootState_;
}

void ConcurrentRegion::setRootState(std::shared_ptr<IStateNode> rootState) {
    // SCXML W3C specification section 3.4: regions must have states
    assert(rootState && "SCXML violation: concurrent region cannot have null root state");

    if (status_ == ConcurrentRegionStatus::ACTIVE) {
        LOG_WARN(
            "ConcurrentRegion::setRootState - Setting root state on active region {} (consider deactivating first)",
            id_);
    }

    LOG_DEBUG("Setting root state for region {} to: {}", id_, (rootState ? rootState->getId() : "null"));

    rootState_ = rootState;

    // Reset state information when root state changes
    currentState_.clear();
    activeStates_.clear();
    isInFinalState_ = false;

    // Clear any previous error state
    if (status_ == ConcurrentRegionStatus::ERROR) {
        clearErrorState();
    }
}

std::vector<std::string> ConcurrentRegion::getActiveStates() const {
    return activeStates_;
}

ConcurrentOperationResult ConcurrentRegion::reset() {
    LOG_DEBUG("Resetting region: {}", id_);

    // Deactivate if currently active
    if (status_ == ConcurrentRegionStatus::ACTIVE) {
        auto result = deactivate();
        if (!result.isSuccess) {
            LOG_ERROR("Failed to deactivate during reset: {}", result.errorMessage);
            return result;
        }
    }

    // Reset all state
    status_ = ConcurrentRegionStatus::INACTIVE;
    currentState_.clear();
    activeStates_.clear();
    isInFinalState_ = false;
    errorMessage_.clear();

    LOG_DEBUG("Successfully reset region: {}", id_);
    return ConcurrentOperationResult::success(id_);
}

std::vector<std::string> ConcurrentRegion::validate() const {
    std::vector<std::string> errors;

    // SCXML W3C specification section 3.4: regions must have valid IDs
    if (id_.empty()) {
        errors.push_back("SCXML violation: Region has empty ID. SCXML specification requires non-empty identifiers.");
    }

    // SCXML W3C specification section 3.4: regions must have root states
    if (!rootState_) {
        errors.push_back(fmt::format(
            "SCXML violation: Region '{}' has no root state. SCXML specification requires regions to contain states.",
            id_));
    } else {
        // Validate root state
        if (!validateRootState()) {
            errors.push_back(fmt::format("Root state validation failed for region: {}", id_));
        }
    }

    // Validate status consistency
    if (status_ == ConcurrentRegionStatus::FINAL && !isInFinalState_) {
        errors.push_back(fmt::format("Inconsistent final state tracking in region: {}", id_));
    }

    if (status_ == ConcurrentRegionStatus::ACTIVE && currentState_.empty()) {
        errors.push_back(fmt::format("Active region {} has no current state", id_));
    }

    return errors;
}

const std::string &ConcurrentRegion::getCurrentState() const {
    return currentState_;
}

void ConcurrentRegion::setCurrentState(const std::string &stateId) {
    // W3C SCXML 3.3: Validate that state belongs to this region
    // This is called during deep initial target synchronization
    if (!stateId.empty() && rootState_) {
        // Validate the state is within this region's scope
        bool isValidState = isDescendantOf(rootState_, stateId);
        if (!isValidState) {
            LOG_WARN("ConcurrentRegion: Attempting to set currentState to '{}' which is not within region '{}' scope",
                     stateId, id_);
            // Continue anyway - StateHierarchyManager knows best in deep target scenarios
        }
    }

    LOG_DEBUG("ConcurrentRegion: Setting currentState for region {} to: {}", id_, stateId);
    currentState_ = stateId;
}

bool ConcurrentRegion::isInErrorState() const {
    return status_ == ConcurrentRegionStatus::ERROR;
}

void ConcurrentRegion::setErrorState(const std::string &errorMessage) {
    LOG_ERROR("Region {} entering error state: {}", id_, errorMessage);
    status_ = ConcurrentRegionStatus::ERROR;
    errorMessage_ = errorMessage;

    // Clear other state information when in error
    currentState_.clear();
    activeStates_.clear();
    isInFinalState_ = false;
}

void ConcurrentRegion::clearErrorState() {
    if (status_ == ConcurrentRegionStatus::ERROR) {
        LOG_DEBUG("Clearing error state for region: {}", id_);
        status_ = ConcurrentRegionStatus::INACTIVE;
        errorMessage_.clear();
    }
}

void ConcurrentRegion::setExecutionContext(std::shared_ptr<IExecutionContext> executionContext) {
    LOG_DEBUG("Setting ExecutionContext for region: {} - new context is {}", id_, executionContext ? "valid" : "null");
    executionContext_ = executionContext;
    LOG_DEBUG("ExecutionContext set successfully for region: {} - stored context is {}", id_,
              executionContext_ ? "valid" : "null");
}

void ConcurrentRegion::setInvokeCallback(
    std::function<void(const std::string &, const std::vector<std::shared_ptr<IInvokeNode>> &)> callback) {
    invokeCallback_ = callback;
    LOG_DEBUG("ConcurrentRegion: Invoke callback set for region: {} (W3C SCXML 6.4 compliance)", id_);
}

void ConcurrentRegion::setConditionEvaluator(std::function<bool(const std::string &)> evaluator) {
    conditionEvaluator_ = evaluator;
    LOG_DEBUG(
        "ConcurrentRegion: Condition evaluator callback set for region: {} (W3C SCXML transition guard compliance)",
        id_);
}

void ConcurrentRegion::setDoneStateCallback(std::function<void(const std::string &)> callback) {
    doneStateCallback_ = callback;
    LOG_DEBUG("ConcurrentRegion: Done state callback set for region: {} (W3C SCXML 3.4 compliance)", id_);
}

void ConcurrentRegion::setDesiredInitialChild(const std::string &childStateId) {
    desiredInitialChild_ = childStateId;
    LOG_DEBUG("ConcurrentRegion: Region '{}' desiredInitialChild set to '{}'", id_, childStateId);
}

// Private methods

bool ConcurrentRegion::validateRootState() const {
    if (!rootState_) {
        return false;
    }

    // Basic validation - can be extended with more sophisticated checks
    if (rootState_->getId().empty()) {
        LOG_ERROR("Root state has empty ID in region: {}", id_);
        return false;
    }

    return true;
}

void ConcurrentRegion::updateCurrentState() {
    if (!rootState_ || status_ != ConcurrentRegionStatus::ACTIVE) {
        currentState_.clear();
        activeStates_.clear();
        return;
    }

    // SCXML W3C specification section 3.4: Preserve hierarchical state tracking
    // Do not override currentState_ if it's already properly set by enterInitialState()
    // The currentState_ should reflect the actual active state in the hierarchy

    if (currentState_.empty()) {
        // Only set to root state if no current state is tracked
        currentState_ = rootState_->getId();
    }

    // Update active states list to include current state
    activeStates_.clear();
    activeStates_.push_back(currentState_);

    LOG_DEBUG("Region {} current state: {}", id_, currentState_);
}

bool ConcurrentRegion::isDescendantOf(const std::shared_ptr<IStateNode> &root, const std::string &targetId) const {
    if (!root) {
        return false;
    }

    // Check if root itself is the target
    if (root->getId() == targetId) {
        return true;
    }

    // Recursively check all children
    const auto &children = root->getChildren();
    for (const auto &child : children) {
        if (child && isDescendantOf(child, targetId)) {
            return true;
        }
    }

    return false;
}

bool ConcurrentRegion::determineIfInFinalState() const {
    LOG_DEBUG(
        "ConcurrentRegion::determineIfInFinalState - Region {} checking final state. Status: {}, currentState: '{}'",
        id_, static_cast<int>(status_), currentState_);

    if (!rootState_ || status_ != ConcurrentRegionStatus::ACTIVE) {
        LOG_DEBUG("Region {} not active or no root state", id_);
        return false;
    }

    // SCXML W3C specification section 3.4: Check if current state is a final state
    if (currentState_.empty()) {
        return false;
    }

    // Check if the current state is a final state by searching through child states
    const auto &children = rootState_->getChildren();
    for (const auto &child : children) {
        if (child && child->getId() == currentState_) {
            bool isFinal = child->isFinalState();
            LOG_DEBUG("Region {} current state '{}' is {}", id_, currentState_, (isFinal ? "FINAL" : "NOT FINAL"));
            return isFinal;
        }
    }

    // If current state is the root state itself, check if root is final
    if (currentState_ == rootState_->getId()) {
        bool isFinal = rootState_->isFinalState();
        LOG_DEBUG("Region {} root state '{}' is {}", id_, currentState_, (isFinal ? "FINAL" : "NOT FINAL"));
        return isFinal;
    }

    LOG_WARN("Region {} current state '{}' not found in state hierarchy", id_, currentState_);
    return false;
}

ConcurrentOperationResult ConcurrentRegion::enterInitialState() {
    if (!rootState_) {
        std::string error = fmt::format("Cannot enter initial state: no root state in region {}", id_);
        return ConcurrentOperationResult::failure(id_, error);
    }

    LOG_DEBUG("Entering initial state for region: {}", id_);

    // SCXML W3C specification section 3.4: Execute entry actions for the region state
    if (executionContext_) {
        LOG_DEBUG("Executing entry actions for: {}", rootState_->getId());

        // W3C SCXML 3.8: Execute entry action blocks
        const auto &entryBlocks = rootState_->getEntryActionBlocks();
        if (!entryBlocks.empty()) {
            for (const auto &actionBlock : entryBlocks) {
                for (const auto &actionNode : actionBlock) {
                    if (actionNode) {
                        try {
                            LOG_DEBUG("Executing entry action: {}", actionNode->getId());
                            executeActionNode(actionNode, "enterInitialState");
                        } catch (const std::exception &e) {
                            LOG_WARN("Entry action failed: {}", e.what());
                        }
                    }
                }
            }
        }
    } else {
        LOG_DEBUG("No execution context available, skipping entry actions");
    }

    // Set up initial configuration
    currentState_ = rootState_->getId();
    activeStates_.clear();
    activeStates_.push_back(currentState_);

    // W3C SCXML 6.4: Check and defer invoke elements for root state itself
    const auto &rootInvokes = rootState_->getInvoke();
    LOG_INFO("ConcurrentRegion: Root state {} has {} invokes, callback is {}", rootState_->getId(), rootInvokes.size(),
             invokeCallback_ ? "set" : "null");
    if (!rootInvokes.empty() && invokeCallback_) {
        LOG_INFO("ConcurrentRegion: Delegating {} invokes for root state: {} to callback", rootInvokes.size(),
                 currentState_);
        invokeCallback_(currentState_, rootInvokes);
    }

    // Check if we need to enter child states
    const auto &children = rootState_->getChildren();
    if (!children.empty()) {
        // W3C SCXML 3.3: Priority order for initial state selection
        std::string initialChild;

        // Priority 1: Parent state's deep initial target (e.g., s1 initial="s11p112 s11p122")
        if (!desiredInitialChild_.empty()) {
            initialChild = desiredInitialChild_;
            LOG_DEBUG("ConcurrentRegion: Region '{}' using desiredInitialChild: '{}'", id_, initialChild);
        }
        // Priority 2: Region's <initial> element with transition target
        else if (const auto &initialTransition = rootState_->getInitialTransition();
                 initialTransition && !initialTransition->getTargets().empty()) {
            initialChild = initialTransition->getTargets()[0];
            LOG_DEBUG("Found initial transition targeting: {} in region: {}", initialChild, id_);
        }
        // Priority 3: Region's initial attribute
        else if (std::string initialFromAttr = rootState_->getInitialState(); !initialFromAttr.empty()) {
            initialChild = initialFromAttr;
            LOG_DEBUG("ConcurrentRegion: Region '{}' rootState '{}' has initialState='{}'", id_, rootState_->getId(),
                      initialChild);
        }
        // Priority 4: First child in document order (W3C default)
        else if (!children.empty()) {
            initialChild = children[0]->getId();
            LOG_DEBUG("ConcurrentRegion: Region '{}' using first child as fallback: '{}'", id_, initialChild);
        }

        if (!initialChild.empty()) {
            LOG_DEBUG("ConcurrentRegion: Region '{}' entering initial child state: '{}'", id_, initialChild);

            // Find the child state node once for efficiency
            auto childState = std::find_if(children.begin(), children.end(),
                                           [&initialChild](const std::shared_ptr<IStateNode> &child) {
                                               return child && child->getId() == initialChild;
                                           });

            if (childState != children.end() && *childState) {
                // W3C SCXML 3.10: History states never end up part of the configuration
                // If initial child is a history state, it will be handled by StateHierarchyManager
                // Do NOT add history state to activeStates_ - it must remain transparent
                if ((*childState)->getType() == Type::HISTORY) {
                    LOG_DEBUG("ConcurrentRegion: Initial child '{}' is HISTORY state, skipping activeStates addition "
                              "(W3C SCXML 3.10 compliance, test 580)",
                              initialChild);
                    // History restoration will be handled externally by StateHierarchyManager
                    // Do not set currentState_ or add to activeStates_
                    return ConcurrentOperationResult::success(id_);
                }

                // Normal state - add to active configuration
                activeStates_.push_back(initialChild);
                currentState_ = initialChild;

                // Execute entry actions for child state and handle recursive nesting
                if (executionContext_) {
                    // W3C SCXML 3.8: Execute child state's entry action blocks
                    const auto &childEntryBlocks = (*childState)->getEntryActionBlocks();
                    for (const auto &actionBlock : childEntryBlocks) {
                        for (const auto &actionNode : actionBlock) {
                            if (actionNode) {
                                LOG_DEBUG("Executing child entry action: {}", actionNode->getId());
                                if (!executeActionNode(actionNode, "enterInitialState")) {
                                    LOG_WARN("W3C SCXML 3.8: Child entry action failed, stopping remaining actions in "
                                             "THIS block only");
                                    break;  // W3C SCXML 3.8: stop remaining actions in this block
                                }
                            }
                        }
                    }

                    // W3C SCXML 6.4: Invoke elements must be processed after state entry
                    // Delegate to StateHierarchyManager via callback pattern for proper timing
                    const auto &childInvokes = (*childState)->getInvoke();
                    if (!childInvokes.empty() && invokeCallback_) {
                        LOG_INFO("ConcurrentRegion: Delegating {} invokes for child state: {} to callback",
                                 childInvokes.size(), initialChild);
                        invokeCallback_(initialChild, childInvokes);
                    }

                    // SCXML spec: If child state is compound, recursively enter its initial state
                    const auto &grandchildren = (*childState)->getChildren();
                    if (!grandchildren.empty()) {
                        std::string childInitialState = (*childState)->getInitialState();
                        if (childInitialState.empty() && !grandchildren.empty()) {
                            childInitialState = grandchildren[0]->getId();
                        }

                        if (!childInitialState.empty()) {
                            LOG_DEBUG("Child state is compound, entering "
                                      "grandchild: {}",
                                      childInitialState);
                            activeStates_.push_back(childInitialState);
                            currentState_ = childInitialState;

                            // Execute grandchild entry actions
                            auto grandchildState =
                                std::find_if(grandchildren.begin(), grandchildren.end(),
                                             [&childInitialState](const std::shared_ptr<IStateNode> &grandchild) {
                                                 return grandchild && grandchild->getId() == childInitialState;
                                             });

                            if (grandchildState != grandchildren.end() && *grandchildState) {
                                // W3C SCXML 3.8: Execute grandchild entry action blocks
                                const auto &grandchildEntryBlocks = (*grandchildState)->getEntryActionBlocks();
                                for (const auto &actionBlock : grandchildEntryBlocks) {
                                    for (const auto &actionNode : actionBlock) {
                                        if (actionNode) {
                                            LOG_DEBUG("Executing grandchild entry "
                                                      "action: {}",
                                                      actionNode->getId());
                                            if (!executeActionNode(actionNode, "enterInitialState")) {
                                                LOG_WARN("W3C SCXML 3.8: Grandchild entry action failed, stopping "
                                                         "remaining actions in THIS block only");
                                                break;  // W3C SCXML 3.8: stop remaining actions in this block
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    isInFinalState_ = determineIfInFinalState();

    // Update region status to FINAL if we entered a final state immediately
    if (isInFinalState_) {
        status_ = ConcurrentRegionStatus::FINAL;
        LOG_DEBUG(
            "ConcurrentRegion::enterInitialState - Region {} immediately entered final state, updating status to FINAL",
            id_);
    }

    LOG_DEBUG("Successfully entered initial state: {}", currentState_);
    return ConcurrentOperationResult::success(id_);
}

ConcurrentOperationResult ConcurrentRegion::exitAllStates(std::shared_ptr<IExecutionContext> executionContext) {
    LOG_DEBUG("Exiting all states in region: {}", id_);

    try {
        // SCXML W3C Specification compliance: Exit sequence for parallel states

        bool exitActionsSuccess = true;

        if (exitHandler_ && !activeStates_.empty()) {
            // W3C SCXML 3.13: Execute exit actions for all active states in document order
            // Note: activeStates_ already includes rootState_, so no need to execute it separately (test 504)
            LOG_DEBUG("Executing exit actions for active states");

            exitActionsSuccess = exitHandler_->executeMultipleStateExits(activeStates_, rootState_, executionContext);

            if (!exitActionsSuccess) {
                LOG_WARN("Some exit actions failed, continuing with cleanup");
            }
        } else {
            LOG_DEBUG("No exit handler or active states, skipping exit actions");
        }

        // Step 3: Clear the active configuration (always perform cleanup)
        // SCXML spec: Maintain legal state configuration after transition
        LOG_DEBUG("Clearing active configuration");
        activeStates_.clear();
        currentState_.clear();
        isInFinalState_ = false;

        // Step 4: Parent state notification would be handled by orchestrator
        // SOLID: Single Responsibility - ConcurrentRegion only manages its own state

        std::string resultMsg = fmt::format("Successfully exited all states in region: {}", id_);
        if (!exitActionsSuccess) {
            resultMsg += " (with exit action warnings)";
        }

        LOG_DEBUG("{}", resultMsg);
        return ConcurrentOperationResult::success(id_);

    } catch (const std::exception &e) {
        std::string errorMsg = fmt::format("Failed to exit states in region {}: {}", id_, e.what());
        LOG_ERROR("Error: {}", errorMsg);

        // Ensure cleanup even on failure
        activeStates_.clear();
        currentState_.clear();
        isInFinalState_ = false;

        return ConcurrentOperationResult::failure(id_, errorMsg);
    }
}

bool ConcurrentRegion::executeActionNode(const std::shared_ptr<IActionNode> &actionNode, const std::string &context) {
    if (!actionNode) {
        LOG_WARN("{} - Null ActionNode encountered, skipping", context);
        return false;
    }

    try {
        LOG_DEBUG("{} - Executing ActionNode: {} (ID: {})", context, actionNode->getActionType(), actionNode->getId());

        if (actionNode->execute(*executionContext_)) {
            LOG_DEBUG("{} - Successfully executed ActionNode: {}", context, actionNode->getActionType());
            return true;
        } else {
            LOG_WARN("{} - ActionNode failed: {}", context, actionNode->getActionType());
            return false;
        }
    } catch (const std::exception &e) {
        LOG_WARN("{} - ActionNode exception: {} Error: {}", context, actionNode->getActionType(), e.what());
        return false;
    }
}

void ConcurrentRegion::executeActionNodes(const std::vector<std::shared_ptr<IActionNode>> &actionNodes,
                                          const std::string &context) {
    // P1 refactoring: DRY principle - centralized action execution
    if (!executionContext_) {
        LOG_ERROR("ConcurrentRegion::executeActionNodes - Cannot execute actions for '{}': executionContext is null in "
                  "region '{}'",
                  context, id_);
        return;
    }

    if (actionNodes.empty()) {
        return;  // Nothing to execute
    }

    for (const auto &actionNode : actionNodes) {
        if (!actionNode) {
            LOG_WARN("ConcurrentRegion::executeActionNodes - Null ActionNode in '{}', skipping", context);
            continue;
        }

        try {
            if (!actionNode->execute(*executionContext_)) {
                LOG_WARN("ConcurrentRegion::executeActionNodes - ActionNode '{}' failed in '{}'",
                         actionNode->getActionType(), context);
            }
        } catch (const std::exception &e) {
            LOG_WARN("ConcurrentRegion::executeActionNodes - ActionNode '{}' exception in '{}': {}",
                     actionNode->getActionType(), context, e.what());
        }
    }
}

}  // namespace RSM
