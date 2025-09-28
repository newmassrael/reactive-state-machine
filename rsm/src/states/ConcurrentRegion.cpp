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
                if (transition->getEvent() == event.eventName) {
                    const auto &targets = transition->getTargets();
                    if (!targets.empty()) {
                        std::string targetState = targets[0];

                        LOG_DEBUG("Executing transition: {} -> {} on event: {}", currentState_, targetState,
                                  event.eventName);

                        // SCXML 사양 준수: ActionNode 객체를 직접 실행
                        LOG_DEBUG("DEBUG: executionContext_ is {}", executionContext_ ? "available" : "null");
                        if (executionContext_) {
                            const auto &actionNodes = transition->getActionNodes();
                            LOG_DEBUG("DEBUG: Found {} ActionNodes in transition", actionNodes.size());
                            if (!actionNodes.empty()) {
                                LOG_DEBUG("Executing {} ActionNodes for transition: "
                                          "{} -> {}",
                                          actionNodes.size(), currentState_, targetState);

                                // ActionNode 직접 실행 (StateMachine과 동일한 패턴)
                                for (const auto &actionNode : actionNodes) {
                                    if (!actionNode) {
                                        LOG_WARN(
                                            "ConcurrentRegion::processEvent - Null ActionNode encountered, skipping");
                                        continue;
                                    }

                                    try {
                                        LOG_DEBUG("Executing ActionNode: {}", actionNode->getActionType());
                                        if (actionNode->execute(*executionContext_)) {
                                            LOG_DEBUG(
                                                "ConcurrentRegion::processEvent - Successfully executed ActionNode: {}",
                                                actionNode->getActionType());
                                        } else {
                                            LOG_WARN("ActionNode failed: {}", actionNode->getActionType());
                                        }
                                    } catch (const std::exception &e) {
                                        LOG_WARN("ActionNode exception: {} Error: {}", actionNode->getActionType(),
                                                 e.what());
                                    }
                                }
                            }
                        }

                        // Update current state
                        currentState_ = targetState;
                        LOG_DEBUG("Updated current state to: {}", currentState_);

                        // SCXML W3C 사양 준수: 대상 상태의 entry 액션 실행
                        if (executionContext_) {
                            // 자식 상태에서 대상 상태를 찾아 entry 액션 실행
                            const auto &children = rootState_->getChildren();
                            for (const auto &child : children) {
                                if (child && child->getId() == targetState) {
                                    const auto &entryActionNodes = child->getEntryActionNodes();
                                    if (!entryActionNodes.empty()) {
                                        LOG_DEBUG("Executing {} entry actions for "
                                                  "target state: {}",
                                                  entryActionNodes.size(), targetState);

                                        for (const auto &actionNode : entryActionNodes) {
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
                                                    LOG_WARN(
                                                        "ConcurrentRegion::processEvent - Entry ActionNode failed: {}",
                                                        actionNode->getActionType());
                                                }
                                            } catch (const std::exception &e) {
                                                LOG_WARN("Entry ActionNode exception: "
                                                         "{} Error: {}",
                                                         actionNode->getActionType(), e.what());
                                            }
                                        }
                                    }
                                    break;  // 대상 상태를 찾았으므로 루프 종료
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

        // Execute entry action nodes if available
        const auto &entryActionNodes = rootState_->getEntryActionNodes();
        if (!entryActionNodes.empty()) {
            for (const auto &actionNode : entryActionNodes) {
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
    } else {
        LOG_DEBUG("No execution context available, skipping entry actions");
    }

    // Set up initial configuration
    currentState_ = rootState_->getId();
    activeStates_.clear();
    activeStates_.push_back(currentState_);

    // Check if we need to enter child states
    const auto &children = rootState_->getChildren();
    if (!children.empty()) {
        // SCXML W3C specification: Handle initial transitions
        // Check if root state has initial transition that targets a specific state
        const auto &initialTransition = rootState_->getInitialTransition();
        std::string initialChild;

        if (initialTransition && !initialTransition->getTargets().empty()) {
            // SCXML initial transition: <initial><transition target="final_state"/></initial>
            initialChild = initialTransition->getTargets()[0];
            LOG_DEBUG("Found initial transition targeting: {} in region: {}", initialChild, id_);
        } else {
            // Fallback: Use getInitialState() or first child
            initialChild = rootState_->getInitialState();
            if (initialChild.empty() && !children.empty()) {
                initialChild = children[0]->getId();
            }
            LOG_DEBUG("Using fallback initial state: {} in region: {}", initialChild, id_);
        }

        if (!initialChild.empty()) {
            LOG_DEBUG("Entering initial child state: {}", initialChild);
            activeStates_.push_back(initialChild);
            currentState_ = initialChild;

            // Execute entry actions for child state and handle recursive nesting
            if (executionContext_) {
                auto childState = std::find_if(children.begin(), children.end(),
                                               [&initialChild](const std::shared_ptr<IStateNode> &child) {
                                                   return child && child->getId() == initialChild;
                                               });

                if (childState != children.end() && *childState) {
                    // Execute child state's entry actions
                    const auto &childEntryActions = (*childState)->getEntryActionNodes();
                    for (const auto &actionNode : childEntryActions) {
                        if (actionNode) {
                            LOG_DEBUG("Executing child entry action: {}", actionNode->getId());
                            executeActionNode(actionNode, "enterInitialState");
                        }
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
                                const auto &grandchildEntryActions = (*grandchildState)->getEntryActionNodes();
                                for (const auto &actionNode : grandchildEntryActions) {
                                    if (actionNode) {
                                        LOG_DEBUG("Executing grandchild entry "
                                                  "action: {}",
                                                  actionNode->getId());
                                        executeActionNode(actionNode, "enterInitialState");
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
            // Step 1: Execute exit actions for all active states in document order
            LOG_DEBUG("Executing exit actions for active states");

            exitActionsSuccess = exitHandler_->executeMultipleStateExits(activeStates_, rootState_, executionContext);

            if (!exitActionsSuccess) {
                LOG_WARN("Some exit actions failed, continuing with cleanup");
            }

            // Step 2: Execute parent region's exit actions (root state exit actions)
            if (rootState_) {
                LOG_DEBUG("Executing root state exit actions");
                bool rootExitSuccess = exitHandler_->executeStateExitActions(rootState_, executionContext);
                if (!rootExitSuccess) {
                    LOG_WARN("Root state exit actions failed");
                    exitActionsSuccess = false;
                }
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

}  // namespace RSM
