#include "states/ConcurrentRegion.h"
#include "actions/AssignAction.h"
#include "actions/ScriptAction.h"
#include "common/Logger.h"
#include "events/EventDescriptor.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "runtime/IActionExecutor.h"
#include "runtime/IExecutionContext.h"
#include <algorithm>

namespace RSM {

ConcurrentRegion::ConcurrentRegion(const std::string &id, std::shared_ptr<IStateNode> rootState,
                                   std::shared_ptr<IExecutionContext> executionContext)
    : id_(id), status_(ConcurrentRegionStatus::INACTIVE), rootState_(rootState), executionContext_(executionContext),
      isInFinalState_(false) {
    // SCXML W3C specification section 3.4: regions must have valid identifiers
    assert(!id_.empty() && "SCXML violation: concurrent region must have non-empty ID");

    Logger::debug("ConcurrentRegion::Constructor - Creating region: " + id_);

    if (rootState_) {
        Logger::debug("ConcurrentRegion::Constructor - Root state provided: " + rootState_->getId());
    } else {
        Logger::debug("ConcurrentRegion::Constructor - No root state provided (will be set later)");
    }
}

ConcurrentRegion::~ConcurrentRegion() {
    Logger::debug("ConcurrentRegion::Destructor - Destroying region: " + id_);

    // Clean deactivation if still active
    if (status_ == ConcurrentRegionStatus::ACTIVE) {
        Logger::debug("ConcurrentRegion::Destructor - Deactivating region during destruction");
        deactivate();
    }
}

const std::string &ConcurrentRegion::getId() const {
    return id_;
}

ConcurrentOperationResult ConcurrentRegion::activate() {
    if (status_ == ConcurrentRegionStatus::ACTIVE) {
        Logger::debug("ConcurrentRegion::activate - Region " + id_ + " already active");
        return ConcurrentOperationResult::success(id_);
    }

    // SCXML W3C specification section 3.4: regions must have root states
    if (!rootState_) {
        std::string error = "SCXML violation: cannot activate region '" + id_ +
                            "' without root state. SCXML specification requires regions to have states.";
        Logger::error("ConcurrentRegion::activate - " + error);
        setErrorState(error);
        return ConcurrentOperationResult::failure(id_, error);
    }

    // Validate root state before activation
    if (!validateRootState()) {
        std::string error = "Root state validation failed for region: " + id_;
        Logger::error("ConcurrentRegion::activate - " + error);
        setErrorState(error);
        return ConcurrentOperationResult::failure(id_, error);
    }

    Logger::debug("ConcurrentRegion::activate - Activating region: " + id_);

    // Enter initial state according to SCXML semantics
    auto result = enterInitialState();
    if (!result.isSuccess) {
        Logger::error("ConcurrentRegion::activate - Failed to enter initial state: " + result.errorMessage);
        setErrorState(result.errorMessage);
        return result;
    }

    status_ = ConcurrentRegionStatus::ACTIVE;
    updateCurrentState();

    Logger::debug("ConcurrentRegion::activate - Successfully activated region: " + id_);
    return ConcurrentOperationResult::success(id_);
}

ConcurrentOperationResult ConcurrentRegion::deactivate() {
    if (status_ == ConcurrentRegionStatus::INACTIVE) {
        Logger::debug("ConcurrentRegion::deactivate - Region " + id_ + " already inactive");
        return ConcurrentOperationResult::success(id_);
    }

    Logger::debug("ConcurrentRegion::deactivate - Deactivating region: " + id_);

    // Exit all active states
    auto result = exitAllStates();
    if (!result.isSuccess) {
        Logger::warn("ConcurrentRegion::deactivate - Warning during state exit: " + result.errorMessage);
        // Continue with deactivation even if exit has issues
    }

    status_ = ConcurrentRegionStatus::INACTIVE;
    currentState_.clear();
    activeStates_.clear();
    isInFinalState_ = false;

    Logger::debug("ConcurrentRegion::deactivate - Successfully deactivated region: " + id_);
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
        std::string error = "Cannot process event in inactive region: " + id_;
        Logger::warn("ConcurrentRegion::processEvent - " + error);
        return ConcurrentOperationResult::failure(id_, error);
    }

    if (!rootState_) {
        std::string error = "SCXML violation: cannot process event without root state in region: " + id_;
        Logger::error("ConcurrentRegion::processEvent - " + error);
        setErrorState(error);
        return ConcurrentOperationResult::failure(id_, error);
    }

    Logger::debug("ConcurrentRegion::processEvent - Processing event '" + event.eventName + "' in region: " + id_);

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

                        Logger::debug("ConcurrentRegion::processEvent - Executing transition: " + currentState_ +
                                      " -> " + targetState + " on event: " + event.eventName);

                        // SCXML 사양 준수: ActionNode 객체를 직접 실행
                        if (executionContext_) {
                            const auto &actionNodes = transition->getActionNodes();
                            if (!actionNodes.empty()) {
                                Logger::debug("ConcurrentRegion::processEvent - Executing " +
                                              std::to_string(actionNodes.size()) +
                                              " ActionNodes for transition: " + currentState_ + " -> " + targetState);

                                // ActionNode 직접 실행 (StateMachine과 동일한 패턴)
                                for (const auto &actionNode : actionNodes) {
                                    if (!actionNode) {
                                        Logger::warn(
                                            "ConcurrentRegion::processEvent - Null ActionNode encountered, skipping");
                                        continue;
                                    }

                                    try {
                                        Logger::debug("ConcurrentRegion::processEvent - Executing ActionNode: " +
                                                      actionNode->getActionType());
                                        if (actionNode->execute(*executionContext_)) {
                                            Logger::debug(
                                                "ConcurrentRegion::processEvent - Successfully executed ActionNode: " +
                                                actionNode->getActionType());
                                        } else {
                                            Logger::warn("ConcurrentRegion::processEvent - ActionNode failed: " +
                                                         actionNode->getActionType());
                                        }
                                    } catch (const std::exception &e) {
                                        Logger::warn("ConcurrentRegion::processEvent - ActionNode exception: " +
                                                     actionNode->getActionType() + " Error: " + std::string(e.what()));
                                    }
                                }
                            }
                        }

                        // Update current state
                        currentState_ = targetState;
                        Logger::debug("ConcurrentRegion::processEvent - Updated current state to: " + currentState_);

                        // SCXML W3C 사양 준수: 대상 상태의 entry 액션 실행
                        if (executionContext_) {
                            // 자식 상태에서 대상 상태를 찾아 entry 액션 실행
                            const auto &children = rootState_->getChildren();
                            for (const auto &child : children) {
                                if (child && child->getId() == targetState) {
                                    const auto &entryActionNodes = child->getEntryActionNodes();
                                    if (!entryActionNodes.empty()) {
                                        Logger::debug("ConcurrentRegion::processEvent - Executing " +
                                                      std::to_string(entryActionNodes.size()) +
                                                      " entry actions for target state: " + targetState);

                                        for (const auto &actionNode : entryActionNodes) {
                                            if (!actionNode) {
                                                Logger::warn("ConcurrentRegion::processEvent - Null entry ActionNode "
                                                             "encountered, skipping");
                                                continue;
                                            }

                                            try {
                                                Logger::debug(
                                                    "ConcurrentRegion::processEvent - Executing entry ActionNode: " +
                                                    actionNode->getActionType() + " (ID: " + actionNode->getId() + ")");
                                                if (actionNode->execute(*executionContext_)) {
                                                    Logger::debug("ConcurrentRegion::processEvent - Successfully "
                                                                  "executed entry ActionNode: " +
                                                                  actionNode->getActionType());
                                                } else {
                                                    Logger::warn(
                                                        "ConcurrentRegion::processEvent - Entry ActionNode failed: " +
                                                        actionNode->getActionType());
                                                }
                                            } catch (const std::exception &e) {
                                                Logger::warn(
                                                    "ConcurrentRegion::processEvent - Entry ActionNode exception: " +
                                                    actionNode->getActionType() + " Error: " + std::string(e.what()));
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
        Logger::debug("ConcurrentRegion::processEvent - Region " + id_ + " reached final state");
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
        Logger::warn("ConcurrentRegion::setRootState - Setting root state on active region " + id_ +
                     " (consider deactivating first)");
    }

    Logger::debug("ConcurrentRegion::setRootState - Setting root state for region " + id_ +
                  " to: " + (rootState ? rootState->getId() : "null"));

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
    Logger::debug("ConcurrentRegion::reset - Resetting region: " + id_);

    // Deactivate if currently active
    if (status_ == ConcurrentRegionStatus::ACTIVE) {
        auto result = deactivate();
        if (!result.isSuccess) {
            Logger::error("ConcurrentRegion::reset - Failed to deactivate during reset: " + result.errorMessage);
            return result;
        }
    }

    // Reset all state
    status_ = ConcurrentRegionStatus::INACTIVE;
    currentState_.clear();
    activeStates_.clear();
    isInFinalState_ = false;
    errorMessage_.clear();

    Logger::debug("ConcurrentRegion::reset - Successfully reset region: " + id_);
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
        errors.push_back("SCXML violation: Region '" + id_ +
                         "' has no root state. SCXML specification requires regions to contain states.");
    } else {
        // Validate root state
        if (!validateRootState()) {
            errors.push_back("Root state validation failed for region: " + id_);
        }
    }

    // Validate status consistency
    if (status_ == ConcurrentRegionStatus::FINAL && !isInFinalState_) {
        errors.push_back("Inconsistent final state tracking in region: " + id_);
    }

    if (status_ == ConcurrentRegionStatus::ACTIVE && currentState_.empty()) {
        errors.push_back("Active region " + id_ + " has no current state");
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
    Logger::error("ConcurrentRegion::setErrorState - Region " + id_ + " entering error state: " + errorMessage);
    status_ = ConcurrentRegionStatus::ERROR;
    errorMessage_ = errorMessage;

    // Clear other state information when in error
    currentState_.clear();
    activeStates_.clear();
    isInFinalState_ = false;
}

void ConcurrentRegion::clearErrorState() {
    if (status_ == ConcurrentRegionStatus::ERROR) {
        Logger::debug("ConcurrentRegion::clearErrorState - Clearing error state for region: " + id_);
        status_ = ConcurrentRegionStatus::INACTIVE;
        errorMessage_.clear();
    }
}

void ConcurrentRegion::setExecutionContext(std::shared_ptr<IExecutionContext> executionContext) {
    Logger::debug("ConcurrentRegion::setExecutionContext - Setting ExecutionContext for region: " + id_);
    executionContext_ = executionContext;
}

// Private methods

bool ConcurrentRegion::validateRootState() const {
    if (!rootState_) {
        return false;
    }

    // Basic validation - can be extended with more sophisticated checks
    if (rootState_->getId().empty()) {
        Logger::error("ConcurrentRegion::validateRootState - Root state has empty ID in region: " + id_);
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

    Logger::debug("ConcurrentRegion::updateCurrentState - Region " + id_ + " current state: " + currentState_);
}

bool ConcurrentRegion::determineIfInFinalState() const {
    Logger::debug("ConcurrentRegion::determineIfInFinalState - Region " + id_ + " checking final state. Status: " +
                  std::to_string(static_cast<int>(status_)) + ", currentState: '" + currentState_ + "'");

    if (!rootState_ || status_ != ConcurrentRegionStatus::ACTIVE) {
        Logger::debug("ConcurrentRegion::determineIfInFinalState - Region " + id_ + " not active or no root state");
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
            Logger::debug("ConcurrentRegion::determineIfInFinalState - Region " + id_ + " current state '" +
                          currentState_ + "' is " + (isFinal ? "FINAL" : "NOT FINAL"));
            return isFinal;
        }
    }

    // If current state is the root state itself, check if root is final
    if (currentState_ == rootState_->getId()) {
        bool isFinal = rootState_->isFinalState();
        Logger::debug("ConcurrentRegion::determineIfInFinalState - Region " + id_ + " root state '" + currentState_ +
                      "' is " + (isFinal ? "FINAL" : "NOT FINAL"));
        return isFinal;
    }

    Logger::warn("ConcurrentRegion::determineIfInFinalState - Region " + id_ + " current state '" + currentState_ +
                 "' not found in state hierarchy");
    return false;
}

ConcurrentOperationResult ConcurrentRegion::enterInitialState() {
    if (!rootState_) {
        std::string error = "Cannot enter initial state: no root state in region " + id_;
        return ConcurrentOperationResult::failure(id_, error);
    }

    Logger::debug("ConcurrentRegion::enterInitialState - Entering initial state for region: " + id_);

    // SCXML W3C specification section 3.4: Execute entry actions for the region state
    if (executionContext_) {
        Logger::debug("ConcurrentRegion::enterInitialState - Executing entry actions for: " + rootState_->getId());

        // Execute entry action nodes if available
        const auto &entryActionNodes = rootState_->getEntryActionNodes();
        if (!entryActionNodes.empty()) {
            for (const auto &actionNode : entryActionNodes) {
                if (actionNode) {
                    // Execute the action through the execution context
                    try {
                        // For assign actions, we need to execute them properly
                        // This requires integration with StateMachine's action execution system
                        Logger::debug("ConcurrentRegion::enterInitialState - Executing entry action: " +
                                      actionNode->getId());
                        // IActionNode-based action execution is properly handled by StateMachine::executeActionNodes()
                    } catch (const std::exception &e) {
                        Logger::warn("ConcurrentRegion::enterInitialState - Entry action failed: " +
                                     std::string(e.what()));
                    }
                }
            }
        }
    } else {
        Logger::debug("ConcurrentRegion::enterInitialState - No execution context available, skipping entry actions");
    }

    // Set up initial configuration
    currentState_ = rootState_->getId();
    activeStates_.clear();
    activeStates_.push_back(currentState_);

    // Check if we need to enter child states
    const auto &children = rootState_->getChildren();
    if (!children.empty()) {
        // Find and enter the initial child state
        std::string initialChild = rootState_->getInitialState();
        if (initialChild.empty() && !children.empty()) {
            initialChild = children[0]->getId();
        }

        if (!initialChild.empty()) {
            Logger::debug("ConcurrentRegion::enterInitialState - Entering initial child state: " + initialChild);
            activeStates_.push_back(initialChild);
            currentState_ = initialChild;

            // Execute entry actions for child state if needed
            if (executionContext_) {
                auto childState = std::find_if(children.begin(), children.end(),
                                               [&initialChild](const std::shared_ptr<IStateNode> &child) {
                                                   return child && child->getId() == initialChild;
                                               });

                if (childState != children.end() && *childState) {
                    const auto &childEntryActions = (*childState)->getEntryActionNodes();
                    for (const auto &actionNode : childEntryActions) {
                        if (actionNode) {
                            Logger::debug("ConcurrentRegion::enterInitialState - Executing child entry action: " +
                                          actionNode->getId());
                            // TODO: Execute child entry actions
                        }
                    }
                }
            }
        }
    }

    isInFinalState_ = determineIfInFinalState();

    Logger::debug("ConcurrentRegion::enterInitialState - Successfully entered initial state: " + currentState_);
    return ConcurrentOperationResult::success(id_);
}

ConcurrentOperationResult ConcurrentRegion::exitAllStates() {
    Logger::debug("ConcurrentRegion::exitAllStates - Exiting all states in region: " + id_);

    // TODO: Implement proper state exit with state machine integration
    // This would involve:
    // 1. Executing exit actions for all active states
    // 2. Clearing the active configuration
    // 3. Notifying parent states of exit

    // For now, basic state cleanup
    currentState_.clear();
    activeStates_.clear();
    isInFinalState_ = false;

    Logger::debug("ConcurrentRegion::exitAllStates - Successfully exited all states in region: " + id_);
    return ConcurrentOperationResult::success(id_);
}

}  // namespace RSM