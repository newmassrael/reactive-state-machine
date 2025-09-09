#include "../../include/runtime/HistoryStateTransitionHandler.h"
#include "common/ErrorCategories.h"
#include "common/Logger.h"
#include <algorithm>
#include <sstream>

using namespace SCXML::Runtime;
using namespace SCXML::Model;

HistoryStateTransitionHandler::HistoryStateTransitionHandler(std::shared_ptr<HistoryStateManager> historyManager)
    : historyManager_(historyManager) {
    if (!historyManager_) {
        SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") + "History manager is required");
        throw SCXML::Common::SCXMLException(SCXML::Common::ErrorCategory::VALIDATION_ERROR,
                                            "History manager cannot be null", "HistoryStateTransitionHandler");
    }

    SCXML::Common::Logger::info(std::string("HistoryStateTransitionHandler: ") +
                                "History state transition handler initialized");
}

bool HistoryStateTransitionHandler::processStateExit(const TransitionContext &context) {
    if (!historyManager_) {
        SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") + "No history manager available");
        return false;
    }

    // Find compound states that need history recording
    auto compoundStates = getCompoundStatesForHistoryRecording(context.exitingStates);

    if (compoundStates.empty()) {
        // No compound states to record history for
        return false;  // Return false when no history was recorded
    }

    // Record history for each compound state
    bool historyRecorded = recordHistoryForCompoundStates(compoundStates, context.currentActiveStates);

    if (historyRecorded) {
        SCXML::Common::Logger::info(std::string("HistoryStateTransitionHandler: ") +
                                    "Processed state exit with history recording for " +
                                    std::to_string(compoundStates.size()) + " compound states");
    }

    return historyRecorded;
}

HistoryStateTransitionHandler::HistoryProcessingResult
HistoryStateTransitionHandler::processStateEntry(const TransitionContext &context) {
    HistoryProcessingResult result;

    // Check if any entering states are history states
    if (!hasHistoryStates(context.enteringStates)) {
        // No history states involved, return original targets
        result.actualTargetStates = context.enteringStates;
        return result;
    }

    // Resolve history state targets
    result = resolveHistoryTargets(context.enteringStates, context.currentActiveStates);
    // Note: historyInvolved is already set correctly by resolveHistoryTargets

    SCXML::Common::Logger::info("HistoryStateTransitionHandler: Processed state entry with history restoration");

    return result;
}

bool HistoryStateTransitionHandler::hasHistoryStates(const std::vector<std::string> &targetStates) const {
    for (const auto &stateId : targetStates) {
        if (isHistoryState(stateId)) {
            return true;
        }
    }
    return false;
}

HistoryStateTransitionHandler::HistoryProcessingResult
HistoryStateTransitionHandler::resolveHistoryTargets(const std::vector<std::string> &targetStates,
                                                     const std::vector<std::string> &currentActiveStates) {
    HistoryProcessingResult result;
    result.historyInvolved = false;
    bool hasValidHistoryState = false;

    for (const auto &targetState : targetStates) {
        if (isHistoryState(targetState)) {
            // Don't mark as history involved yet - wait until we successfully process a valid history state

            // Process history state
            auto resolvedStates = processHistoryStateTarget(targetState, currentActiveStates);

            if (!resolvedStates.empty()) {
                // Successfully processed a valid history state
                hasValidHistoryState = true;

                result.actualTargetStates.insert(result.actualTargetStates.end(), resolvedStates.begin(),
                                                 resolvedStates.end());

                // Determine if these came from history restoration or defaults
                auto historyEntry = historyManager_->getHistoryEntry(getParentCompoundState(targetState));
                if (historyEntry && historyEntry->isValid) {
                    result.restoredStates.insert(result.restoredStates.end(), resolvedStates.begin(),
                                                 resolvedStates.end());
                } else {
                    result.defaultStates.insert(result.defaultStates.end(), resolvedStates.begin(),
                                                resolvedStates.end());
                }
            } else {
                // Invalid history state - treat as regular state for graceful error recovery
                SCXML::Common::Logger::error("HistoryStateTransitionHandler: History restoration failed for " +
                                             targetState + ", treating as regular state");
                result.actualTargetStates.push_back(targetState);
            }
        } else {
            // Regular state - add as-is
            result.actualTargetStates.push_back(targetState);
        }
    }

    // Set historyInvolved based on whether we successfully processed any valid history states
    result.historyInvolved = hasValidHistoryState;

    // Validate resolved states
    if (!validateResolvedStates(result.actualTargetStates, targetStates)) {
        result.errorMessage = "Resolved states validation failed";
        SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") + result.errorMessage);
        return result;
    }

    return result;
}

std::vector<std::string>
HistoryStateTransitionHandler::getStatesToExitFromCompound(const std::string &compoundStateId,
                                                           const std::vector<std::string> &allActiveStates) const {
    std::vector<std::string> statesToExit;

    for (const auto &activeState : allActiveStates) {
        if (activeState == compoundStateId) {
            // Include the compound state itself
            statesToExit.push_back(activeState);
        } else if (activeState.find(compoundStateId + ".") == 0) {
            // State is a child of the compound state (using dot notation)
            statesToExit.push_back(activeState);
        } else if (activeState.find(compoundStateId + "_") == 0) {
            // State is a child of the compound state (using underscore notation)
            statesToExit.push_back(activeState);
        }
    }

    return statesToExit;
}

std::vector<std::string> HistoryStateTransitionHandler::getCompoundStatesForHistoryRecording(
    const std::vector<std::string> &exitingStates) const {
    SCXML::Common::Logger::debug("HistoryStateTransitionHandler: Getting compound states for history recording from " +
                                 std::to_string(exitingStates.size()) + " exiting states");

    std::vector<std::string> compoundStates;
    std::unordered_set<std::string> addedStates;

    for (const auto &stateId : exitingStates) {
        SCXML::Common::Logger::debug("HistoryStateTransitionHandler: Checking exiting state: " + stateId);

        // Check if this state itself is a compound state
        bool isCompound = isCompoundState(stateId);
        SCXML::Common::Logger::debug("HistoryStateTransitionHandler: State " + stateId +
                                     " isCompound: " + (isCompound ? "true" : "false"));

        if (isCompound && addedStates.find(stateId) == addedStates.end()) {
            SCXML::Common::Logger::debug("HistoryStateTransitionHandler: Adding compound state for history: " +
                                         stateId);
            compoundStates.push_back(stateId);
            addedStates.insert(stateId);
        }

        // Also check parent states
        std::string parentState = getParentCompoundState(stateId);
        while (!parentState.empty() && addedStates.find(parentState) == addedStates.end()) {
            if (isCompoundState(parentState)) {
                compoundStates.push_back(parentState);
                addedStates.insert(parentState);
            }
            parentState = getParentCompoundState(parentState);
        }
    }

    return compoundStates;
}

bool HistoryStateTransitionHandler::isCompoundState(const std::string &stateId) const {
    // Check cache first
    auto cacheIt = compoundStateCache_.find(stateId);
    if (cacheIt != compoundStateCache_.end()) {
        return cacheIt->second;
    }

    // In a real implementation, this would query the state model
    // For now, we'll use heuristics: states with children or registered as parent for history states
    bool isCompound = hasChildStates(stateId);

    // Also check if any history state is registered for this state as parent
    if (!isCompound) {
        auto registeredStates = historyManager_->getRegisteredHistoryStates();
        for (const auto &pair : registeredStates) {
            if (pair.second == stateId) {
                isCompound = true;
                break;
            }
        }
    }

    compoundStateCache_[stateId] = isCompound;
    return isCompound;
}

bool HistoryStateTransitionHandler::isHistoryState(const std::string &stateId) const {
    return HistoryStateHelper::isHistoryStateId(stateId) || historyManager_->getHistoryType(stateId).has_value();
}

std::string HistoryStateTransitionHandler::getParentCompoundState(const std::string &stateId) const {
    // Simple heuristic: parent is everything before the last separator
    size_t lastDot = stateId.find_last_of('.');
    if (lastDot != std::string::npos && lastDot > 0) {
        std::string parent = stateId.substr(0, lastDot);
        if (isCompoundState(parent)) {
            return parent;
        }
    }

    size_t lastUnderscore = stateId.find_last_of('_');
    if (lastUnderscore != std::string::npos && lastUnderscore > 0) {
        std::string parent = stateId.substr(0, lastUnderscore);
        if (isCompoundState(parent)) {
            return parent;
        }
    }

    return "";
}

void HistoryStateTransitionHandler::setStateModel(std::shared_ptr<const SCXML::Model::IStateNode> stateModel) {
    stateModel_ = stateModel;

    // Clear caches when state model changes
    compoundStateCache_.clear();
    childStatesCache_.clear();

    SCXML::Common::Logger::info(std::string("HistoryStateTransitionHandler: ") + "State model updated");
}

std::vector<std::string>
HistoryStateTransitionHandler::processHistoryStateTarget(const std::string &historyStateId,
                                                         const std::vector<std::string> &currentActiveStates) {
    if (!historyManager_) {
        SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") + "No history manager available");
        return {};
    }

    // Restore history for this history state
    auto restoration = historyManager_->restoreHistory(historyStateId, currentActiveStates);

    if (!restoration.success) {
        SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") +
                                     "History restoration failed for " + historyStateId + ": " +
                                     restoration.errorMessage);
        return {};
    }

    // Combine restored states and default states
    std::vector<std::string> resolvedStates;
    resolvedStates.insert(resolvedStates.end(), restoration.restoredStates.begin(), restoration.restoredStates.end());
    resolvedStates.insert(resolvedStates.end(), restoration.defaultStates.begin(), restoration.defaultStates.end());

    std::ostringstream statesStr;
    for (size_t i = 0; i < resolvedStates.size(); ++i) {
        if (i > 0) {
            statesStr << ", ";
        }
        statesStr << resolvedStates[i];
    }

    SCXML::Common::Logger::info(std::string("HistoryStateTransitionHandler: ") + "Resolved history state " +
                                historyStateId + " to: [" + statesStr.str() + "]");

    return resolvedStates;
}

bool HistoryStateTransitionHandler::recordHistoryForCompoundStates(const std::vector<std::string> &compoundStates,
                                                                   const std::vector<std::string> &activeStates) {
    bool anyRecorded = false;

    for (const auto &compoundState : compoundStates) {
        bool recorded = historyManager_->recordHistory(compoundState, activeStates);

        if (recorded) {
            anyRecorded = true;
            SCXML::Common::Logger::debug(std::string("HistoryStateTransitionHandler: ") +
                                         "Recorded history for compound state: " + compoundState);
        } else {
            SCXML::Common::Logger::warning(std::string("HistoryStateTransitionHandler: ") +
                                           "Failed to record history for compound state: " + compoundState);
        }
    }

    return anyRecorded;
}

bool HistoryStateTransitionHandler::validateResolvedStates(
    const std::vector<std::string> &resolvedStates, const std::vector<std::string> & /* originalTargets */) const {
    // Basic validation: no empty state IDs
    for (const auto &stateId : resolvedStates) {
        if (stateId.empty()) {
            SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") +
                                         "Resolved state has empty ID");
            return false;
        }
    }

    // Check for duplicate states in resolved list
    std::unordered_set<std::string> uniqueStates;
    for (const auto &stateId : resolvedStates) {
        if (uniqueStates.find(stateId) != uniqueStates.end()) {
            SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") +
                                         "Duplicate state in resolved targets: " + stateId);
            return false;
        }
        uniqueStates.insert(stateId);
    }

    // Validate state ID format (should not contain invalid characters)
    for (const auto &stateId : resolvedStates) {
        if (stateId.find("..") != std::string::npos || stateId.find("//") != std::string::npos ||
            stateId.front() == '.' || stateId.back() == '.' || stateId.front() == '_' || stateId.back() == '_') {
            SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") +
                                         "Invalid state ID format: " + stateId);
            return false;
        }
    }

    // Check for conflicting states (states that cannot be active simultaneously)
    for (size_t i = 0; i < resolvedStates.size(); ++i) {
        for (size_t j = i + 1; j < resolvedStates.size(); ++j) {
            if (statesConflict(resolvedStates[i], resolvedStates[j])) {
                SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") +
                                             "Conflicting states in resolved targets: " + resolvedStates[i] + " and " +
                                             resolvedStates[j]);
                return false;
            }
        }
    }

    // If we have a state model, perform more detailed validation
    if (stateModel_) {
        for (const auto &stateId : resolvedStates) {
            if (!stateExistsInModel(stateId)) {
                SCXML::Common::Logger::error(std::string("HistoryStateTransitionHandler: ") +
                                             "Resolved state does not exist in state model: " + stateId);
                return false;
            }
        }
    }

    SCXML::Common::Logger::debug(std::string("HistoryStateTransitionHandler: ") + "Validated " +
                                 std::to_string(resolvedStates.size()) + " resolved states");

    return true;
}

std::vector<std::string> HistoryStateTransitionHandler::getChildStates(const std::string &compoundStateId) const {
    // Check cache first
    auto cacheIt = childStatesCache_.find(compoundStateId);
    if (cacheIt != childStatesCache_.end()) {
        return cacheIt->second;
    }

    std::vector<std::string> children;

    // In a real implementation, this would query the state model
    // For now, we'll use a heuristic: states that start with compoundStateId + "."
    // This is a simplified approach for demonstration purposes

    // Example: if compoundStateId is "ui", children would be ["ui.login", "ui.dashboard"]
    // Without access to the full state model, we return empty for now
    // In production, this would query the actual SCXML model

    childStatesCache_[compoundStateId] = children;
    return children;
}

bool HistoryStateTransitionHandler::hasChildStates(const std::string &stateId) const {
    return !getChildStates(stateId).empty();
}

bool HistoryStateTransitionHandler::statesConflict(const std::string &state1, const std::string &state2) const {
    // States conflict if they are sibling states that cannot be active simultaneously
    // Parent-child relationships are NOT conflicts - they should be active together

    // Check if one state is a prefix of another (ancestor relationship)
    // In Deep History restoration, parents and children should be active together
    if (state1.find(state2 + ".") == 0 || state1.find(state2 + "_") == 0) {
        return false;  // state2 is ancestor of state1 - this is valid for deep history
    }
    if (state2.find(state1 + ".") == 0 || state2.find(state1 + "_") == 0) {
        return false;  // state1 is ancestor of state2 - this is valid for deep history
    }

    // Check if they're siblings in the same compound state (would conflict)
    std::string parent1 = getParentCompoundState(state1);
    std::string parent2 = getParentCompoundState(state2);

    if (!parent1.empty() && parent1 == parent2) {
        // Same parent - they might conflict if the parent is not parallel
        // For now, assume non-parallel states conflict
        return true;
    }

    return false;
}

bool HistoryStateTransitionHandler::stateExistsInModel(const std::string & /* stateId */) const {
    if (!stateModel_) {
        // Can't validate without state model - assume it exists
        return true;
    }

    // In a real implementation, this would traverse the state model
    // For now, we'll do a simple check
    // Query SCXML model for history state configuration
    return true;
}

// HistoryStateIntegration implementation
std::vector<std::string>
HistoryStateIntegration::resolveTransitionTargets(const std::vector<std::string> &originalTargets,
                                                  HistoryStateTransitionHandler &handler,
                                                  const std::vector<std::string> &currentActiveStates) {
    if (!handler.hasHistoryStates(originalTargets)) {
        // No history states involved
        return originalTargets;
    }

    auto result = handler.resolveHistoryTargets(originalTargets, currentActiveStates);

    if (!result.errorMessage.empty()) {
        SCXML::Common::Logger::error(std::string("HistoryStateIntegration: Failed to resolve transition targets: ") +
                                     result.errorMessage);
        return originalTargets;  // Fallback to original targets
    }

    return result.actualTargetStates;
}

bool HistoryStateIntegration::processTransitionExits(const std::vector<std::string> &exitingStates,
                                                     HistoryStateTransitionHandler &handler,
                                                     const std::vector<std::string> &currentActiveStates) {
    HistoryStateTransitionHandler::TransitionContext context;
    context.currentActiveStates = currentActiveStates;
    context.exitingStates = exitingStates;

    return handler.processStateExit(context);
}

bool HistoryStateIntegration::transitionInvolveHistory(std::shared_ptr<const SCXML::Model::ITransitionNode> transition,
                                                       const HistoryStateTransitionHandler & /* handler */) {
    if (!transition) {
        return false;
    }

    // In a real implementation, this would examine the transition's target states
    // For now, return false as we don't have access to the transition's targets
    return false;
}

std::unique_ptr<HistoryStateTransitionHandler>
HistoryStateIntegration::createHistoryHandler(std::shared_ptr<const SCXML::Model::IStateNode> stateModel) {
    auto historyManager = std::make_shared<HistoryStateManager>();
    auto handler = std::make_unique<HistoryStateTransitionHandler>(historyManager);

    if (stateModel) {
        handler->setStateModel(stateModel);
    }

    return handler;
}

std::set<std::string>
HistoryStateTransitionHandler::findParentWithHistory(const std::vector<std::string> &exitingStates) const {
    std::set<std::string> parentsWithHistory;

    if (!historyManager_) {
        SCXML::Common::Logger::error("HistoryStateTransitionHandler: No history manager available for parent lookup");
        return parentsWithHistory;
    }

    // Find compound states from exiting states
    auto compoundStates = getCompoundStatesForHistoryRecording(exitingStates);

    // Get all registered history states to find which parents have history
    auto registeredHistoryStates = historyManager_->getRegisteredHistoryStates();

    // Check each compound state to see if it has registered history
    for (const auto &compoundState : compoundStates) {
        // Check if any registered history state has this compound state as parent
        // The map is <historyStateId, parentStateId>
        for (const auto &historyEntry : registeredHistoryStates) {
            if (historyEntry.second == compoundState) {
                parentsWithHistory.insert(compoundState);
                SCXML::Common::Logger::debug("HistoryStateTransitionHandler: Found parent with history: " +
                                             compoundState);
                break;  // Found one, no need to check more for this parent
            }
        }
    }

    SCXML::Common::Logger::debug("HistoryStateTransitionHandler: Found " + std::to_string(parentsWithHistory.size()) +
                                 " parents with registered history from " + std::to_string(exitingStates.size()) +
                                 " exiting states");

    return parentsWithHistory;
}