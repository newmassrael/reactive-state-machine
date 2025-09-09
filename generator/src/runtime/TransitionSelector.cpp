#include "runtime/TransitionSelector.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "model/DocumentModel.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "runtime/RuntimeContext.h"
#include "runtime/StateConfiguration.h"
#include <algorithm>
#include <functional>
#include <regex>
#include <sstream>

namespace SCXML {
namespace Runtime {

TransitionSelector::TransitionSelector() : model_(nullptr) {
    SCXML::Common::Logger::debug("TransitionSelector created");
}

bool TransitionSelector::initialize(std::shared_ptr<Model::DocumentModel> model) {
    if (!model) {
        SCXML::Common::Logger::error("TransitionSelector::initialize - null model provided");
        return false;
    }

    model_ = model;
    SCXML::Common::Logger::debug("TransitionSelector initialized with model");
    return true;
}

// ====== Transition Selection ======

TransitionSelector::SelectionResult
TransitionSelector::selectTransitionsForEvent(Events::EventPtr event, const StateConfiguration &configuration,
                                              Runtime::RuntimeContext &context) {
    SelectionResult result;

    if (!model_) {
        result.errorMessage = "No model available for transition selection";
        return result;
    }

    // Find all enabled transitions for this event
    auto candidates = findEnabledTransitions(event, configuration, context);

    if (candidates.empty()) {
        result.hasEnabledTransitions = false;
        return result;
    }

    // Sort by priority (document order)
    candidates = sortByPriority(candidates);

    // Remove conflicting transitions
    result.selectedTransitions = removeConflictingTransitions(candidates, configuration);
    result.hasEnabledTransitions = !result.selectedTransitions.empty();

    // Log the selection process
    logTransitionSelection(result);

    return result;
}

TransitionSelector::SelectionResult
TransitionSelector::selectEventlessTransitions(const StateConfiguration &configuration,
                                               Runtime::RuntimeContext &context) {
    return selectTransitionsForEvent(nullptr, configuration, context);
}

std::vector<TransitionSelector::TransitionCandidate>
TransitionSelector::findEnabledTransitions(Events::EventPtr event, const StateConfiguration &configuration,
                                           Runtime::RuntimeContext &context) {
    std::vector<TransitionCandidate> candidates;

    if (!model_) {
        return candidates;
    }

    // Get all potentially enabled transitions from active states
    auto potentialTransitions = getPotentialTransitions(configuration);

    for (auto transition : potentialTransitions) {
        if (isTransitionEnabled(transition, event, context)) {
            TransitionCandidate candidate;
            candidate.transition = transition;
            // Transition doesn't have getSource() - we'll track this during construction
            candidate.sourceStateId = "";  // TODO: Need to track source state
            candidate.targetStateIds = getTargetStates(transition);
            candidate.triggeringEvent = event;
            candidate.documentOrder = getDocumentOrder(transition);
            candidate.isEventless = isEventlessTransition(transition);
            candidate.isInternal = isInternalTransition(transition);

            candidates.push_back(candidate);
        }
    }

    return candidates;
}

// ====== Conflict Resolution ======

std::vector<TransitionSelector::TransitionCandidate>
TransitionSelector::removeConflictingTransitions(const std::vector<TransitionCandidate> &candidates,
                                                 const StateConfiguration &configuration) {
    std::vector<TransitionCandidate> filtered;

    for (const auto &candidate : candidates) {
        bool conflicts = false;

        // Check if this candidate conflicts with any already selected transition
        for (const auto &selected : filtered) {
            if (transitionsConflict(candidate, selected, configuration)) {
                conflicts = true;
                break;
            }
        }

        if (!conflicts) {
            filtered.push_back(candidate);
        }
    }

    return filtered;
}

bool TransitionSelector::transitionsConflict(const TransitionCandidate &t1, const TransitionCandidate &t2,
                                             const StateConfiguration & /* configuration */) {
    // Two transitions conflict if they have overlapping exit sets
    // This is a simplified implementation - full SCXML would need more complex analysis

    // If transitions are from the same state, they conflict
    if (t1.sourceStateId == t2.sourceStateId) {
        return true;
    }

    // If one transition exits a state that the other needs to remain in, they conflict
    // This requires computing exit sets, which is complex - for now use simple heuristic

    // Check if source states are related (ancestor/descendant)
    auto ancestors1 = getProperAncestors(t1.sourceStateId);
    auto ancestors2 = getProperAncestors(t2.sourceStateId);

    // If one state is ancestor of another, transitions may conflict
    for (const auto &ancestor : ancestors1) {
        if (ancestor == t2.sourceStateId) {
            return true;
        }
    }

    for (const auto &ancestor : ancestors2) {
        if (ancestor == t1.sourceStateId) {
            return true;
        }
    }

    return false;
}

// ====== Transition Analysis ======

bool TransitionSelector::isTransitionEnabled(std::shared_ptr<Model::ITransitionNode> transition, Events::EventPtr event,
                                             Runtime::RuntimeContext &context) {
    if (!transition) {
        return false;
    }

    // Check event match
    if (event) {
        std::string eventSpec = transition->getEvent();
        if (!eventSpec.empty() && !matchesEventSpec(eventSpec, event->getName())) {
            return false;
        }
    } else {
        // For eventless selection, only consider eventless transitions
        if (!isEventlessTransition(transition)) {
            return false;
        }
    }

    // Check guard condition
    if (!evaluateGuardCondition(transition, context)) {
        return false;
    }

    return true;
}

std::vector<std::shared_ptr<Model::ITransitionNode>>
TransitionSelector::getTransitionsFromState(const std::string &stateId) {
    std::vector<std::shared_ptr<Model::ITransitionNode>> transitions;

    if (!model_) {
        return transitions;
    }

    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return transitions;
    }

    return stateNode->getTransitions();
}

std::vector<std::shared_ptr<Model::ITransitionNode>>
TransitionSelector::getPotentialTransitions(const StateConfiguration &configuration) {
    std::vector<std::shared_ptr<Model::ITransitionNode>> transitions;

    // Get transitions from all active states
    auto activeStates = configuration.getActiveStatesVector();
    for (const auto &stateId : activeStates) {
        auto stateTransitions = getTransitionsFromState(stateId);
        transitions.insert(transitions.end(), stateTransitions.begin(), stateTransitions.end());
    }

    return transitions;
}

// ====== Priority and Ordering ======

std::vector<TransitionSelector::TransitionCandidate>
TransitionSelector::sortByPriority(const std::vector<TransitionCandidate> &transitions) {
    auto sorted = transitions;
    std::sort(sorted.begin(), sorted.end(), [](const TransitionCandidate &a, const TransitionCandidate &b) {
        return a.documentOrder < b.documentOrder;
    });

    return sorted;
}

std::string TransitionSelector::getLeastCommonAncestor(const std::string &sourceStateId,
                                                       const std::vector<std::string> &targetStateIds) {
    if (targetStateIds.empty()) {
        return "";
    }

    if (targetStateIds.size() == 1) {
        return findLCA(sourceStateId, targetStateIds[0]);
    }

    // For multiple targets, find LCA of all states
    std::string lca = findLCA(sourceStateId, targetStateIds[0]);
    for (size_t i = 1; i < targetStateIds.size(); ++i) {
        lca = findLCA(lca, targetStateIds[i]);
        if (lca.empty()) {
            break;
        }
    }

    return lca;
}

// ====== Event Matching ======

bool TransitionSelector::matchesEventSpec(const std::string &eventSpec, const std::string &eventName) {
    if (eventSpec.empty() || eventName.empty()) {
        return false;
    }

    // Wildcard match
    if (eventSpec == "*") {
        return true;
    }

    // Exact match
    if (eventSpec == eventName) {
        return true;
    }

    // Prefix match with dot notation (e.g., "button." matches "button.click")
    if (eventSpec.back() == '.' && eventName.substr(0, eventSpec.length()) == eventSpec) {
        return true;
    }

    // Suffix match with dot notation (e.g., ".click" matches "button.click")
    if (eventSpec.front() == '.' && eventName.length() >= eventSpec.length()) {
        return eventName.substr(eventName.length() - eventSpec.length()) == eventSpec;
    }

    return false;
}

bool TransitionSelector::isEventlessTransition(std::shared_ptr<Model::ITransitionNode> transition) {
    if (!transition) {
        return false;
    }

    std::string eventSpec = transition->getEvent();
    return eventSpec.empty();
}

// ====== Validation and Debugging ======

std::vector<std::string> TransitionSelector::validateSelection(const SelectionResult &result,
                                                               const StateConfiguration &configuration) {
    std::vector<std::string> errors;

    // Check for conflicting transitions in result
    for (size_t i = 0; i < result.selectedTransitions.size(); ++i) {
        for (size_t j = i + 1; j < result.selectedTransitions.size(); ++j) {
            if (transitionsConflict(result.selectedTransitions[i], result.selectedTransitions[j], configuration)) {
                errors.push_back(
                    "Conflicting transitions selected: " + candidateToString(result.selectedTransitions[i]) + " and " +
                    candidateToString(result.selectedTransitions[j]));
            }
        }
    }

    return errors;
}

std::string TransitionSelector::getSelectionDetails(const SelectionResult &result) {
    std::ostringstream oss;

    oss << "Transition Selection Result:\n";
    oss << "  Selected transitions: " << result.selectedTransitions.size() << "\n";

    for (const auto &candidate : result.selectedTransitions) {
        oss << "    - " << candidateToString(candidate) << "\n";
    }

    if (!result.errorMessage.empty()) {
        oss << "  Error: " << result.errorMessage << "\n";
    }

    return oss.str();
}

// ====== Private Helper Methods ======

bool TransitionSelector::evaluateGuardCondition(std::shared_ptr<Model::ITransitionNode> transition,
                                                Runtime::RuntimeContext & /* context */) {
    if (!transition) {
        return false;
    }

    std::string guard = transition->getGuard();
    if (guard.empty()) {
        return true;  // No guard means always enabled
    }

    // Use context to evaluate guard - simplified implementation
    try {
        // For now, simplified evaluation - just check if guard is "true"
        return guard == "true" || guard == "1";
    } catch (const std::exception &e) {
        SCXML::Common::Logger::warning("Guard evaluation failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> TransitionSelector::getTargetStates(std::shared_ptr<Model::ITransitionNode> transition) {
    if (!transition) {
        return {};
    }

    return transition->getTargets();
}

bool TransitionSelector::isInternalTransition(std::shared_ptr<Model::ITransitionNode> transition) {
    if (!transition) {
        return false;
    }

    return transition->isInternal();
}

std::set<std::string> TransitionSelector::computeExitSet(const std::vector<TransitionCandidate> &transitions,
                                                         const StateConfiguration & /* configuration */) {
    std::set<std::string> exitSet;

    // Simplified exit set computation
    for (const auto &candidate : transitions) {
        if (!candidate.isInternal) {
            exitSet.insert(candidate.sourceStateId);

            // Add ancestors that need to be exited
            auto ancestors = getProperAncestors(candidate.sourceStateId);
            for (const auto &ancestor : ancestors) {
                // Check if ancestor should be exited based on LCA computation
                auto lca = getLeastCommonAncestor(candidate.sourceStateId, candidate.targetStateIds);
                if (ancestor != lca) {
                    exitSet.insert(ancestor);
                } else {
                    break;
                }
            }
        }
    }

    return exitSet;
}

std::set<std::string> TransitionSelector::computeEntrySet(const std::vector<TransitionCandidate> &transitions,
                                                          const StateConfiguration &configuration) {
    std::set<std::string> entrySet;

    // Simplified entry set computation
    for (const auto &candidate : transitions) {
        for (const auto &target : candidate.targetStateIds) {
            entrySet.insert(target);

            // Add ancestors that need to be entered
            auto ancestors = getProperAncestors(target);
            for (const auto &ancestor : ancestors) {
                if (!configuration.isActive(ancestor)) {
                    entrySet.insert(ancestor);
                }
            }
        }
    }

    return entrySet;
}

bool TransitionSelector::hasCommonDescendant(const std::string & /* state1 */, const std::string & /* state2 */) {
    // Simplified implementation - in full SCXML this would check the state hierarchy
    return false;
}

std::string TransitionSelector::findLCA(const std::string &state1, const std::string &state2) {
    if (state1.empty() || state2.empty()) {
        return "";
    }

    if (state1 == state2) {
        return state1;
    }

    auto ancestors1 = getProperAncestors(state1);
    auto ancestors2 = getProperAncestors(state2);

    // Add the states themselves
    ancestors1.insert(ancestors1.begin(), state1);
    ancestors2.insert(ancestors2.begin(), state2);

    // Find first common ancestor
    for (const auto &ancestor1 : ancestors1) {
        for (const auto &ancestor2 : ancestors2) {
            if (ancestor1 == ancestor2) {
                return ancestor1;
            }
        }
    }

    return "";
}

std::vector<std::string> TransitionSelector::getProperAncestors(const std::string &stateId) {
    std::vector<std::string> ancestors;

    if (!model_) {
        return ancestors;
    }

    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return ancestors;
    }

    // Walk up the hierarchy
    auto parent = stateNode->getParent();
    while (parent) {
        ancestors.push_back(parent->getId());
        parent = parent->getParent();
    }

    return ancestors;
}

std::shared_ptr<Model::IStateNode> TransitionSelector::getStateNode(const std::string &stateId) {
    if (!model_) {
        return nullptr;
    }

    auto rawPtr = model_->findStateById(stateId);
    if (!rawPtr) {
        return nullptr;
    }

    return std::shared_ptr<Model::IStateNode>(rawPtr, [](Model::IStateNode *) {
        // Don't delete - the model owns the object
    });
}

int TransitionSelector::getDocumentOrder(std::shared_ptr<Model::ITransitionNode> transition) {
    if (!transition) {
        return 0;
    }

    // For now, use address-based ordering as a simple heuristic
    return static_cast<int>(reinterpret_cast<uintptr_t>(transition.get()) % 10000);
}

void TransitionSelector::logTransitionSelection(const SelectionResult &result) {
    if (result.hasEnabledTransitions) {
        SCXML::Common::Logger::debug("TransitionSelector selected " +
                                     std::to_string(result.selectedTransitions.size()) + " transitions");

        for (const auto &candidate : result.selectedTransitions) {
            SCXML::Common::Logger::debug("  - " + candidateToString(candidate));
        }
    } else {
        SCXML::Common::Logger::debug("TransitionSelector found no enabled transitions");
    }
}

std::string TransitionSelector::candidateToString(const TransitionCandidate &candidate) {
    std::ostringstream oss;
    oss << candidate.sourceStateId;

    if (!candidate.targetStateIds.empty()) {
        oss << " -> ";
        for (size_t i = 0; i < candidate.targetStateIds.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << candidate.targetStateIds[i];
        }
    }

    if (candidate.isEventless) {
        oss << " (eventless)";
    }

    if (candidate.isInternal) {
        oss << " (internal)";
    }

    return oss.str();
}

}  // namespace Runtime
}  // namespace SCXML