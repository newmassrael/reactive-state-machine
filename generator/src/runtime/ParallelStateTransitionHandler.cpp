#include "runtime/ParallelStateTransitionHandler.h"
#include "Event.h"
#include "common/Logger.h"
#include "core/ParallelStateNode.h"
#include "model/ITransitionNode.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>

namespace SCXML {

ParallelStateTransitionHandler::ParallelStateTransitionHandler() {
    Logger::debug("ParallelStateTransitionHandler::Constructor - Initialized parallel transition handler");
}

ParallelStateTransitionHandler::TransitionResult
ParallelStateTransitionHandler::executeParallelTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                                          SCXML::Events::EventPtr event,
                                                          SCXML::Runtime::RuntimeContext &context) {
    TransitionResult result;

    if (!transition || !event) {
        result.errorMessages.push_back("Null transition or event provided");
        return result;
    }

    Logger::debug("ParallelStateTransitionHandler::executeParallelTransition - Executing transition for event: " +
                  event->getName());

    try {
        // Check if transition can be taken
        if (!canTakeParallelTransition(transition, event, context)) {
            result.errorMessages.push_back("Transition conditions not met");
            return result;
        }

        // Create transition context
        std::string currentState = context.getCurrentState();
        ParallelTransitionContext transitionCtx = createTransitionContext(currentState, transition, event, context);

        // Execute appropriate transition type
        if (transitionCtx.isInternalTransition()) {
            // Internal transition within parallel state
            auto parallelState = findContainingParallelState(currentState, context);
            if (parallelState) {
                result = executeInternalParallelTransition(parallelState, currentState, transition, event, context);
            } else {
                result.errorMessages.push_back("Could not find containing parallel state for internal transition");
            }
        } else if (transitionCtx.isEnteringParallelState()) {
            // Transition into parallel state
            auto targetState = findTargetState(transition, context);
            auto parallelState = std::dynamic_pointer_cast<ParallelStateNode>(targetState);
            if (parallelState) {
                result = enterParallelState(parallelState, context);
            } else {
                result.errorMessages.push_back("Target state is not a parallel state");
            }
        } else if (transitionCtx.isExitingParallelState()) {
            // Transition out of parallel state
            auto parallelState = findContainingParallelState(currentState, context);
            if (parallelState) {
                result = exitParallelState(parallelState, context);

                // Continue with regular transition to target
                if (result.success) {
                    auto targetState = findTargetState(transition, context);
                    if (targetState) {
                        result.targetState = targetState->getId();
                        result.enteredStates.push_back(targetState->getId());
                        result.stateChanged = true;
                    }
                }
            } else {
                result.errorMessages.push_back("Could not find source parallel state");
            }
        } else if (transitionCtx.isParallelToParallelTransition()) {
            // Transition between parallel states
            auto sourceParallel = findContainingParallelState(currentState, context);
            auto targetState = findTargetState(transition, context);
            auto targetParallel = std::dynamic_pointer_cast<ParallelStateNode>(targetState);

            if (sourceParallel && targetParallel) {
                // Exit source parallel state
                auto exitResult = exitParallelState(sourceParallel, context);

                // Enter target parallel state
                if (exitResult.success) {
                    auto enterResult = enterParallelState(targetParallel, context);
                    result = mergeTransitionResults({exitResult, enterResult});
                    result.targetState = targetParallel->getId();
                } else {
                    result = exitResult;
                }
            } else {
                result.errorMessages.push_back("Could not find source or target parallel state");
            }
        } else {
            // Regular transition - execute actions
            if (executeTransitionActions(transition, context)) {
                auto targetState = findTargetState(transition, context);
                if (targetState) {
                    result.success = true;
                    result.stateChanged = true;
                    result.targetState = targetState->getId();
                    result.enteredStates.push_back(targetState->getId());
                }
            }
        }

    } catch (const std::exception &e) {
        result.errorMessages.push_back("Exception executing parallel transition: " + std::string(e.what()));
        Logger::error("ParallelStateTransitionHandler::executeParallelTransition - Exception: " +
                      std::string(e.what()));
    }

    return result;
}

ParallelStateTransitionHandler::TransitionResult
ParallelStateTransitionHandler::enterParallelState(std::shared_ptr<ParallelStateNode> parallelState,
                                                   SCXML::Runtime::RuntimeContext &context) {
    TransitionResult result;

    if (!parallelState) {
        result.errorMessages.push_back("Null parallel state provided");
        return result;
    }

    std::string parallelStateId = parallelState->getId();
    Logger::info("ParallelStateTransitionHandler::enterParallelState - Entering parallel state: " + parallelStateId);

    try {
        // Reset parallel state for entry
        parallelState->resetRegionStates();

        // Get initial states for all regions
        auto regionInitialStates = parallelState->getRegionInitialStates();

        // Enter all regions
        const auto &regions = parallelState->getParallelRegions();
        for (const auto &region : regions) {
            if (region) {
                std::string regionId = region->getId();
                std::string initialState;

                auto it = regionInitialStates.find(regionId);
                if (it != regionInitialStates.end()) {
                    initialState = it->second;
                } else {
                    initialState = regionId;  // Use region ID as fallback
                }

                // Mark region as active with initial state
                std::unordered_set<std::string> initialStates = {initialState};
                parallelState->setRegionActiveStates(regionId, initialStates);
                parallelState->markRegionActive(regionId);

                result.enteredStates.push_back(regionId);
                result.enteredStates.push_back(initialState);

                Logger::debug("ParallelStateTransitionHandler::enterParallelState - Entered region: " + regionId +
                              " with initial state: " + initialState);
            }
        }

        result.success = !result.enteredStates.empty();
        result.stateChanged = result.success;
        result.targetState = parallelStateId;
        result.allParallelRegionsComplete = false;  // Just entered, so not complete

        if (result.success) {
            // Update runtime context
            context.setCurrentState(parallelStateId);

            // Activate all region states
            for (const auto &stateId : result.enteredStates) {
                context.activateState(stateId);
            }
        }

    } catch (const std::exception &e) {
        result.errorMessages.push_back("Exception entering parallel state: " + std::string(e.what()));
        Logger::error("ParallelStateTransitionHandler::enterParallelState - Exception: " + std::string(e.what()));
    }

    return result;
}

ParallelStateTransitionHandler::TransitionResult
ParallelStateTransitionHandler::exitParallelState(std::shared_ptr<ParallelStateNode> parallelState,
                                                  SCXML::Runtime::RuntimeContext &context) {
    TransitionResult result;

    if (!parallelState) {
        result.errorMessages.push_back("Null parallel state provided");
        return result;
    }

    std::string parallelStateId = parallelState->getId();
    Logger::info("ParallelStateTransitionHandler::exitParallelState - Exiting parallel state: " + parallelStateId);

    try {
        // Get all currently active states across regions
        auto allActiveStates = parallelState->getAllActiveStates();

        // Exit all regions
        const auto &regions = parallelState->getParallelRegions();
        for (const auto &region : regions) {
            if (region) {
                std::string regionId = region->getId();
                auto regionActiveStates = parallelState->getRegionActiveStates(regionId);

                // Add region states to exit list
                for (const auto &stateId : regionActiveStates) {
                    result.exitedStates.push_back(stateId);
                }
                result.exitedStates.push_back(regionId);

                // Mark region as complete (exited)
                parallelState->markRegionComplete(regionId);
                parallelState->setRegionActiveStates(regionId, {});

                Logger::debug("ParallelStateTransitionHandler::exitParallelState - Exited region: " + regionId);
            }
        }

        result.success = true;
        result.stateChanged = true;
        result.allParallelRegionsComplete = true;
        result.exitedStates.push_back(parallelStateId);

        // Update runtime context
        for (const auto &stateId : result.exitedStates) {
            context.deactivateState(stateId);
        }

        Logger::info("ParallelStateTransitionHandler::exitParallelState - Successfully exited " +
                     std::to_string(regions.size()) + " regions");

    } catch (const std::exception &e) {
        result.errorMessages.push_back("Exception exiting parallel state: " + std::string(e.what()));
        Logger::error("ParallelStateTransitionHandler::exitParallelState - Exception: " + std::string(e.what()));
    }

    return result;
}

ParallelStateTransitionHandler::TransitionResult ParallelStateTransitionHandler::executeInternalParallelTransition(
    std::shared_ptr<ParallelStateNode> parallelState, const std::string &sourceRegion,
    std::shared_ptr<SCXML::Model::ITransitionNode> transition, SCXML::Events::EventPtr event,
    SCXML::Runtime::RuntimeContext &context) {
    TransitionResult result;

    if (!parallelState || !transition || !event) {
        result.errorMessages.push_back("Null parameter provided for internal parallel transition");
        return result;
    }

    Logger::debug("ParallelStateTransitionHandler::executeInternalParallelTransition - Executing internal transition "
                  "in region: " +
                  sourceRegion);

    try {
        // Execute actions if any
        if (!executeTransitionActions(transition, context)) {
            result.errorMessages.push_back("Failed to execute transition actions");
            return result;
        }

        // Find target states
        auto targets = computeParallelTransitionTargets(transition, context);

        if (!targets.empty()) {
            // Update region active states
            parallelState->setRegionActiveStates(sourceRegion, targets);

            result.success = true;
            result.stateChanged = true;
            result.enteredStates.assign(targets.begin(), targets.end());
            result.regionTransitions[sourceRegion] = {event->getName()};

            // Check if this region is now complete
            bool regionComplete = std::any_of(targets.begin(), targets.end(), [](const std::string &stateId) {
                return stateId.find("final") != std::string::npos;
            });

            if (regionComplete) {
                parallelState->markRegionComplete(sourceRegion);
                result.completedRegions.insert(sourceRegion);
            }

            // Check if all regions are now complete
            result.allParallelRegionsComplete = parallelState->areAllRegionsComplete();

            Logger::debug(std::string("ParallelStateTransitionHandler::executeInternalParallelTransition - ") +
                          "Internal transition successful, region complete: " + (regionComplete ? "true" : "false"));
        } else {
            result.errorMessages.push_back("No target states computed for internal transition");
        }

    } catch (const std::exception &e) {
        result.errorMessages.push_back("Exception in internal parallel transition: " + std::string(e.what()));
        Logger::error("ParallelStateTransitionHandler::executeInternalParallelTransition - Exception: " +
                      std::string(e.what()));
    }

    return result;
}

bool ParallelStateTransitionHandler::canTakeParallelTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                                               SCXML::Events::EventPtr event,
                                                               SCXML::Runtime::RuntimeContext &context) {
    if (!transition || !event) {
        return false;
    }

    try {
        // Check event match
        std::string transitionEvent = transition->getEvent();
        if (!transitionEvent.empty() && transitionEvent != event->getName()) {
            return false;
        }

        // Evaluate guard conditions
        return evaluateTransitionConditions(transition, event, context);

    } catch (const std::exception &e) {
        Logger::error("ParallelStateTransitionHandler::canTakeParallelTransition - Exception: " +
                      std::string(e.what()));
        return false;
    }
}

std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>>
ParallelStateTransitionHandler::getAvailableParallelTransitions(std::shared_ptr<ParallelStateNode> parallelState,
                                                                SCXML::Events::EventPtr event,
                                                                SCXML::Runtime::RuntimeContext &context) {
    std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>> availableTransitions;

    if (!parallelState || !event) {
        return availableTransitions;
    }

    try {
        // Check transitions from parallel state itself
        const auto &parallelTransitions = parallelState->getTransitions();
        for (const auto &transition : parallelTransitions) {
            if (transition && canTakeParallelTransition(transition, event, context)) {
                availableTransitions.push_back(transition);
            }
        }

        // Check transitions from active regions
        const auto &regions = parallelState->getParallelRegions();
        for (const auto &region : regions) {
            if (region && !parallelState->isRegionComplete(region->getId())) {
                const auto &regionTransitions = region->getTransitions();
                for (const auto &transition : regionTransitions) {
                    if (transition && canTakeParallelTransition(transition, event, context)) {
                        availableTransitions.push_back(transition);
                    }
                }
            }
        }

    } catch (const std::exception &e) {
        Logger::error("ParallelStateTransitionHandler::getAvailableParallelTransitions - Exception: " +
                      std::string(e.what()));
    }

    return availableTransitions;
}

std::unordered_set<std::string> ParallelStateTransitionHandler::computeParallelTransitionTargets(
    std::shared_ptr<SCXML::Model::ITransitionNode> transition, SCXML::Runtime::RuntimeContext &context) {
    std::unordered_set<std::string> targets;

    if (!transition) {
        return targets;
    }

    try {
        const auto &transitionTargets = transition->getTargets();
        targets.insert(transitionTargets.begin(), transitionTargets.end());

    } catch (const std::exception &e) {
        Logger::error("ParallelStateTransitionHandler::computeParallelTransitionTargets - Exception: " +
                      std::string(e.what()));
    }

    return targets;
}

ParallelStateTransitionHandler::ParallelTransitionContext ParallelStateTransitionHandler::createTransitionContext(
    const std::string &sourceStateId, std::shared_ptr<SCXML::Model::ITransitionNode> transition,
    SCXML::Events::EventPtr event, SCXML::Runtime::RuntimeContext &context) {
    ParallelTransitionContext ctx;
    ctx.triggerEvent = event;

    try {
        // Find source parallel state
        auto sourceParallel = findContainingParallelState(sourceStateId, context);
        if (sourceParallel) {
            ctx.sourceParallelState = sourceParallel->getId();

            // Get source regions
            const auto &regions = sourceParallel->getParallelRegions();
            for (const auto &region : regions) {
                if (region) {
                    ctx.sourceRegions.push_back(region->getId());

                    auto regionStates = sourceParallel->getRegionActiveStates(region->getId());
                    if (!regionStates.empty()) {
                        ctx.regionStates[region->getId()] = *regionStates.begin();
                    }
                }
            }
        }

        // Find target parallel state
        if (transition) {
            auto targetState = findTargetState(transition, context);
            auto targetParallel = std::dynamic_pointer_cast<ParallelStateNode>(targetState);
            if (targetParallel) {
                ctx.targetParallelState = targetParallel->getId();

                const auto &regions = targetParallel->getParallelRegions();
                for (const auto &region : regions) {
                    if (region) {
                        ctx.targetRegions.push_back(region->getId());
                    }
                }
            }
        }

    } catch (const std::exception &e) {
        Logger::error("ParallelStateTransitionHandler::createTransitionContext - Exception: " + std::string(e.what()));
    }

    return ctx;
}

bool ParallelStateTransitionHandler::checkParallelCompletionConditions(std::shared_ptr<ParallelStateNode> parallelState,
                                                                       SCXML::Runtime::RuntimeContext &context) {
    if (!parallelState) {
        return false;
    }

    try {
        return parallelState->areAllRegionsComplete();
    } catch (const std::exception &e) {
        Logger::error("ParallelStateTransitionHandler::checkParallelCompletionConditions - Exception: " +
                      std::string(e.what()));
        return false;
    }
}

std::vector<std::string>
ParallelStateTransitionHandler::validateParallelTransition(std::shared_ptr<SCXML::Model::ITransitionNode> transition) {
    std::vector<std::string> errors;

    if (!transition) {
        errors.push_back("Null transition provided");
        return errors;
    }

    try {
        // Validate transition has targets
        if (transition->getTargets().empty()) {
            errors.push_back("Parallel transition has no targets");
        }

        // Validate event specification
        if (transition->getEvent().empty()) {
            errors.push_back("Parallel transition should specify event (or use eventless transition explicitly)");
        }

    } catch (const std::exception &e) {
        errors.push_back("Exception validating parallel transition: " + std::string(e.what()));
    }

    return errors;
}

// Protected method implementations

std::unordered_map<std::string, ParallelStateTransitionHandler::TransitionResult>
ParallelStateTransitionHandler::executeRegionTransitions(std::shared_ptr<ParallelStateNode> parallelState,
                                                         SCXML::Events::EventPtr event,
                                                         SCXML::Runtime::RuntimeContext &context) {
    std::unordered_map<std::string, TransitionResult> regionResults;

    if (!parallelState || !event) {
        return regionResults;
    }

    try {
        const auto &regions = parallelState->getParallelRegions();

        for (const auto &region : regions) {
            if (region && !parallelState->isRegionComplete(region->getId())) {
                std::string regionId = region->getId();

                // Find and execute available transitions for this region
                const auto &transitions = region->getTransitions();
                for (const auto &transition : transitions) {
                    if (transition && canTakeParallelTransition(transition, event, context)) {
                        TransitionResult regionResult =
                            executeInternalParallelTransition(parallelState, regionId, transition, event, context);
                        regionResults[regionId] = regionResult;
                        break;  // Execute only first matching transition per region
                    }
                }
            }
        }

    } catch (const std::exception &e) {
        Logger::error("ParallelStateTransitionHandler::executeRegionTransitions - Exception: " + std::string(e.what()));
    }

    return regionResults;
}

std::shared_ptr<SCXML::Model::IStateNode>
ParallelStateTransitionHandler::findTargetState(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                                SCXML::Runtime::RuntimeContext &context) {
    if (!transition) {
        return nullptr;
    }

    try {
        const auto &targets = transition->getTargets();
        if (!targets.empty()) {
            // For simplicity, return first target
            // In a full implementation, this would lookup the state in the model
            std::string targetId = targets[0];

            // This would be replaced with actual state lookup from context/model
            Logger::debug("ParallelStateTransitionHandler::findTargetState - Target: " + targetId);

            // Return nullptr for now - actual implementation would return the found state
            return nullptr;
        }
    } catch (const std::exception &e) {
        Logger::error("ParallelStateTransitionHandler::findTargetState - Exception: " + std::string(e.what()));
    }

    return nullptr;
}

bool ParallelStateTransitionHandler::isParallelState(std::shared_ptr<SCXML::Model::IStateNode> state) const {
    return state && state->getType() == Type::PARALLEL;
}

std::shared_ptr<ParallelStateNode>
ParallelStateTransitionHandler::findContainingParallelState(const std::string &stateId,
                                                            SCXML::Runtime::RuntimeContext &context) {
    // This would be implemented by traversing the state hierarchy
    // For now, return nullptr - full implementation would search the model
    Logger::debug("ParallelStateTransitionHandler::findContainingParallelState - Searching for: " + stateId);
    return nullptr;
}

bool ParallelStateTransitionHandler::executeTransitionActions(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                                              SCXML::Runtime::RuntimeContext &context) {
    if (!transition) {
        return true;  // No actions to execute
    }

    try {
        // Execute transition actions using action processor
        Logger::debug("ParallelStateTransitionHandler::executeTransitionActions - Executing actions");
        return true;

    } catch (const std::exception &e) {
        Logger::error("ParallelStateTransitionHandler::executeTransitionActions - Exception: " + std::string(e.what()));
        return false;
    }
}

bool ParallelStateTransitionHandler::evaluateTransitionConditions(
    std::shared_ptr<SCXML::Model::ITransitionNode> transition, SCXML::Events::EventPtr event,
    SCXML::Runtime::RuntimeContext &context) {
    if (!transition) {
        return true;  // No conditions to check
    }

    try {
        // Evaluate guard conditions - get from transition interface
        std::string condition = "";
        if (transition) {
            // Get condition from transition's guard nodes
            auto guardNodes = transition->getGuardNodes();
            if (!guardNodes.empty()) {
                // Use first guard condition (most common case)
                condition = guardNodes[0]->getCondition();
            }
        }
        if (condition.empty()) {
            return true;  // No condition means always true
        }

        Logger::debug("ParallelStateTransitionHandler::evaluateTransitionConditions - Evaluating: " + condition);

        // Full implementation would use expression evaluator
        return true;

    } catch (const std::exception &e) {
        Logger::error("ParallelStateTransitionHandler::evaluateTransitionConditions - Exception: " +
                      std::string(e.what()));
        return false;
    }
}

// Private method implementations

ParallelStateTransitionHandler::TransitionResult
ParallelStateTransitionHandler::mergeTransitionResults(const std::vector<TransitionResult> &results) {
    TransitionResult merged;

    if (results.empty()) {
        return merged;
    }

    merged.success = std::all_of(results.begin(), results.end(), [](const TransitionResult &r) { return r.success; });

    merged.stateChanged =
        std::any_of(results.begin(), results.end(), [](const TransitionResult &r) { return r.stateChanged; });

    for (const auto &result : results) {
        merged.enteredStates.insert(merged.enteredStates.end(), result.enteredStates.begin(),
                                    result.enteredStates.end());
        merged.exitedStates.insert(merged.exitedStates.end(), result.exitedStates.begin(), result.exitedStates.end());
        merged.errorMessages.insert(merged.errorMessages.end(), result.errorMessages.begin(),
                                    result.errorMessages.end());

        // Merge region transitions
        for (const auto &pair : result.regionTransitions) {
            auto &targetVector = merged.regionTransitions[pair.first];
            targetVector.insert(targetVector.end(), pair.second.begin(), pair.second.end());
        }

        // Merge completed regions
        merged.completedRegions.insert(result.completedRegions.begin(), result.completedRegions.end());

        if (!result.targetState.empty()) {
            merged.targetState = result.targetState;
        }
    }

    merged.allParallelRegionsComplete = std::all_of(
        results.begin(), results.end(), [](const TransitionResult &r) { return r.allParallelRegionsComplete; });

    return merged;
}

std::string
ParallelStateTransitionHandler::generateTransitionId(std::shared_ptr<SCXML::Model::ITransitionNode> transition) const {
    if (!transition) {
        return "null_transition";
    }

    std::string id = "transition_" + transition->getEvent() + "_";
    const auto &targets = transition->getTargets();
    if (!targets.empty()) {
        id += targets[0];
    }

    return id;
}

}  // namespace SCXML