#include "runtime/MicrostepProcessor.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "model/DocumentModel.h"
#include "model/IActionNode.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "runtime/RuntimeContext.h"
#include "runtime/StateConfiguration.h"
#include "runtime/TransitionSelector.h"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace SCXML {
namespace Runtime {

MicrostepProcessor::MicrostepProcessor()
    : model_(nullptr), transitionSelector_(nullptr),
      maxMicrostepsPerMacrostep_(100)  // Default limit to prevent infinite loops
      ,
      maxExecutionTimeMs_(5000)  // Default 5 second limit
{
    SCXML::Common::Logger::debug("MicrostepProcessor created");
}

bool MicrostepProcessor::initialize(std::shared_ptr<Model::DocumentModel> model,
                                    std::shared_ptr<TransitionSelector> transitionSelector) {
    if (!model) {
        SCXML::Common::Logger::error("MicrostepProcessor::initialize - null model provided");
        return false;
    }

    if (!transitionSelector) {
        SCXML::Common::Logger::error("MicrostepProcessor::initialize - null transition selector provided");
        return false;
    }

    model_ = model;
    transitionSelector_ = transitionSelector;

    SCXML::Common::Logger::debug("MicrostepProcessor initialized");
    return true;
}

// ====== Microstep Processing ======

MicrostepProcessor::MicrostepResult MicrostepProcessor::executeMicrostep(Events::EventPtr event,
                                                                         StateConfiguration &configuration,
                                                                         Runtime::RuntimeContext &context) {
    MicrostepResult result;
    result.processedEvent = event;
    auto startTime = getCurrentTimeMs();

    if (!model_ || !transitionSelector_) {
        result.errorMessage = "MicrostepProcessor not properly initialized";
        return result;
    }

    try {
        // Select enabled transitions for this event
        auto selectionResult = transitionSelector_->selectTransitionsForEvent(event, configuration, context);

        if (!selectionResult.hasEnabledTransitions || selectionResult.selectedTransitions.empty()) {
            result.success = true;
            result.transitionsTaken = false;
            return result;
        }

        // Convert to transition nodes
        std::vector<std::shared_ptr<Model::ITransitionNode>> transitions;
        for (const auto &candidate : selectionResult.selectedTransitions) {
            transitions.push_back(candidate.transition);
        }

        // Compute exit and entry sets
        auto exitSet = computeExitSet(transitions, configuration);
        auto entrySet = computeEntrySet(transitions, configuration);

        // Execute the microstep
        bool success = true;

        // 1. Exit states
        if (!exitStates(exitSet, configuration, context)) {
            success = false;
        }
        result.exitedStates = exitSet;

        // 2. Execute transition actions
        if (success && !executeTransitionActions(transitions, context)) {
            success = false;
        }

        // 3. Enter states
        if (success && !enterStates(entrySet, configuration, context)) {
            success = false;
        }
        result.enteredStates = entrySet;

        result.success = success;
        result.transitionsTaken = !transitions.empty();

    } catch (const std::exception &e) {
        result.success = false;
        result.errorMessage = "Microstep execution failed: " + std::string(e.what());
        SCXML::Common::Logger::error(result.errorMessage);
    }

    result.executionTimeMs = getCurrentTimeMs() - startTime;
    updateStatistics(result);
    logMicrostepExecution(result);

    return result;
}

MicrostepProcessor::MicrostepResult MicrostepProcessor::executeEventlessMicrostep(StateConfiguration &configuration,
                                                                                  Runtime::RuntimeContext &context) {
    return executeMicrostep(nullptr, configuration, context);
}

// ====== Macrostep Processing ======

MicrostepProcessor::MacrostepResult MicrostepProcessor::executeMacrostep(Events::EventPtr triggeringEvent,
                                                                         StateConfiguration &configuration,
                                                                         Runtime::RuntimeContext &context) {
    MacrostepResult result;
    result.triggeringEvent = triggeringEvent;
    auto startTime = getCurrentTimeMs();

    if (!model_ || !transitionSelector_) {
        result.errorMessage = "MicrostepProcessor not properly initialized";
        return result;
    }

    try {
        bool success = true;
        int microstepCount = 0;

        // Execute initial microstep with triggering event
        if (triggeringEvent) {
            auto microstepResult = executeMicrostep(triggeringEvent, configuration, context);
            result.microsteps.push_back(microstepResult);
            microstepCount++;

            if (!microstepResult.success) {
                success = false;
                result.errorMessage = "Initial microstep failed: " + microstepResult.errorMessage;
            }
        }

        // Continue with eventless microsteps until no more transitions are enabled
        while (success && microstepCount < maxMicrostepsPerMacrostep_) {
            auto currentTime = getCurrentTimeMs();
            if (maxExecutionTimeMs_ > 0 && (currentTime - startTime) > maxExecutionTimeMs_) {
                success = false;
                result.errorMessage = "Macrostep execution timeout";
                break;
            }

            auto microstepResult = executeEventlessMicrostep(configuration, context);

            if (!microstepResult.success) {
                success = false;
                result.errorMessage = "Eventless microstep failed: " + microstepResult.errorMessage;
                break;
            }

            if (!microstepResult.transitionsTaken) {
                // No more eventless transitions - macrostep complete
                break;
            }

            result.microsteps.push_back(microstepResult);
            microstepCount++;
        }

        if (microstepCount >= maxMicrostepsPerMacrostep_) {
            success = false;
            result.errorMessage = "Macrostep exceeded maximum microstep limit";
        }

        result.success = success;
        result.microstepsExecuted = microstepCount;
        result.finalConfiguration = configuration.getActiveStatesVector();

    } catch (const std::exception &e) {
        result.success = false;
        result.errorMessage = "Macrostep execution failed: " + std::string(e.what());
        SCXML::Common::Logger::error(result.errorMessage);
    }

    result.totalExecutionTimeMs = getCurrentTimeMs() - startTime;
    stats_.totalMacrosteps++;
    logMacrostepExecution(result);

    return result;
}

MicrostepProcessor::MacrostepResult MicrostepProcessor::executeEventlessMacrostep(StateConfiguration &configuration,
                                                                                  Runtime::RuntimeContext &context) {
    return executeMacrostep(nullptr, configuration, context);
}

// ====== State Transition Operations ======

bool MicrostepProcessor::exitStates(const std::vector<std::string> &statesToExit, StateConfiguration &configuration,
                                    Runtime::RuntimeContext &context) {
    bool success = true;

    for (const auto &stateId : statesToExit) {
        if (!configuration.isActive(stateId)) {
            continue;  // State already exited
        }

        // Execute onexit actions
        if (!executeOnExitActions(stateId, context)) {
            SCXML::Common::Logger::warning("Failed to execute onexit actions for state: " + stateId);
            success = false;
        }

        // Remove state from configuration
        configuration.removeState(stateId);
        stats_.totalStatesExited++;

        SCXML::Common::Logger::debug("Exited state: " + stateId);
    }

    return success;
}

bool MicrostepProcessor::executeTransitionActions(
    const std::vector<std::shared_ptr<Model::ITransitionNode>> &transitions, Runtime::RuntimeContext &context) {
    bool success = true;

    for (const auto &transition : transitions) {
        if (!transition) {
            continue;
        }

        auto actions = transition->getExecutableContent();
        if (!executeActions(actions, context)) {
            SCXML::Common::Logger::warning("Failed to execute actions for transition from " + transition->getSource() +
                                           " to " + transition->getTarget());
            success = false;
        }
    }

    return success;
}

bool MicrostepProcessor::enterStates(const std::vector<std::string> &statesToEnter, StateConfiguration &configuration,
                                     Runtime::RuntimeContext &context) {
    bool success = true;

    for (const auto &stateId : statesToEnter) {
        if (configuration.isActive(stateId)) {
            continue;  // State already entered
        }

        // Add state to configuration
        configuration.addState(stateId);
        stats_.totalStatesEntered++;

        // Execute onentry actions
        if (!executeOnEntryActions(stateId, context)) {
            SCXML::Common::Logger::warning("Failed to execute onentry actions for state: " + stateId);
            success = false;
        }

        SCXML::Common::Logger::debug("Entered state: " + stateId);
    }

    return success;
}

// ====== Execution Order Computation ======

std::vector<std::string>
MicrostepProcessor::computeExitSet(const std::vector<std::shared_ptr<Model::ITransitionNode>> &transitions,
                                   const StateConfiguration & /* configuration */) {
    std::set<std::string> exitSet;

    for (const auto &transition : transitions) {
        if (!transition) {
            continue;
        }

        std::string sourceState = transition->getSource();
        if (transition->getType() == "internal") {
            // Internal transitions don't exit the source state
            continue;
        }

        // Add source state to exit set
        exitSet.insert(sourceState);

        // Add ancestors up to LCA
        std::string lca = getLeastCommonAncestor(transition);
        auto ancestors = getProperAncestors(sourceState);

        for (const auto &ancestor : ancestors) {
            if (ancestor == lca) {
                break;
            }
            exitSet.insert(ancestor);
        }
    }

    // Convert to vector and sort in reverse document order
    std::vector<std::string> result(exitSet.begin(), exitSet.end());
    return sortInReverseDocumentOrder(result);
}

std::vector<std::string>
MicrostepProcessor::computeEntrySet(const std::vector<std::shared_ptr<Model::ITransitionNode>> &transitions,
                                    const StateConfiguration &configuration) {
    std::set<std::string> entrySet;

    for (const auto &transition : transitions) {
        if (!transition) {
            continue;
        }

        std::string target = transition->getTarget();
        if (target.empty()) {
            continue;
        }

        // Split multiple targets
        std::istringstream iss(target);
        std::string targetState;
        while (iss >> targetState) {
            // Add target state
            entrySet.insert(targetState);

            // Add ancestors up to LCA (if they're not already active)
            std::string lca = getLeastCommonAncestor(transition);
            auto ancestors = getProperAncestors(targetState);

            for (const auto &ancestor : ancestors) {
                if (ancestor == lca) {
                    break;
                }
                if (!configuration.isActive(ancestor)) {
                    entrySet.insert(ancestor);
                }
            }
        }
    }

    // Convert to vector and sort in document order
    std::vector<std::string> result(entrySet.begin(), entrySet.end());
    return sortInDocumentOrder(result);
}

std::string MicrostepProcessor::getLeastCommonAncestor(std::shared_ptr<Model::ITransitionNode> transition) {
    if (!transition) {
        return "";
    }

    std::string source = transition->getSource();
    std::string target = transition->getTarget();

    if (source.empty() || target.empty()) {
        return "";
    }

    // Simplified LCA computation
    auto sourceAncestors = getProperAncestors(source);
    auto targetAncestors = getProperAncestors(target);

    sourceAncestors.insert(sourceAncestors.begin(), source);
    targetAncestors.insert(targetAncestors.begin(), target);

    // Find first common ancestor
    for (const auto &sourceAncestor : sourceAncestors) {
        for (const auto &targetAncestor : targetAncestors) {
            if (sourceAncestor == targetAncestor) {
                return sourceAncestor;
            }
        }
    }

    return "";
}

// ====== Action Execution ======

bool MicrostepProcessor::executeActions(const std::vector<std::shared_ptr<Model::IActionNode>> &actions,
                                        Runtime::RuntimeContext &context) {
    bool success = true;

    for (const auto &action : actions) {
        if (!action) {
            continue;
        }

        try {
            if (!action->execute(context)) {
                SCXML::Common::Logger::warning("Action execution failed: " + action->getId());
                success = false;
            } else {
                stats_.totalActionsExecuted++;
            }
        } catch (const std::exception &e) {
            SCXML::Common::Logger::warning("Action execution exception: " + std::string(e.what()));
            success = false;
        }
    }

    return success;
}

bool MicrostepProcessor::executeOnEntryActions(const std::string &stateId, Runtime::RuntimeContext &context) {
    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return false;
    }

    auto actions = stateNode->getOnEntryActions();
    return executeActions(actions, context);
}

bool MicrostepProcessor::executeOnExitActions(const std::string &stateId, Runtime::RuntimeContext &context) {
    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return false;
    }

    auto actions = stateNode->getOnExitActions();
    return executeActions(actions, context);
}

// ====== Validation and Configuration ======

void MicrostepProcessor::setMaxMicrostepsPerMacrostep(int maxMicrosteps) {
    maxMicrostepsPerMacrostep_ = maxMicrosteps;
    SCXML::Common::Logger::debug("Set max microsteps per macrostep: " + std::to_string(maxMicrosteps));
}

void MicrostepProcessor::setMaxExecutionTimeMs(uint64_t maxTimeMs) {
    maxExecutionTimeMs_ = maxTimeMs;
    SCXML::Common::Logger::debug("Set max execution time: " + std::to_string(maxTimeMs) + "ms");
}

std::vector<std::string> MicrostepProcessor::validateMicrostepResult(const MicrostepResult &result) {
    std::vector<std::string> errors;

    if (!result.success && result.errorMessage.empty()) {
        errors.push_back("Microstep marked as failed but no error message provided");
    }

    if (result.transitionsTaken && result.exitedStates.empty() && result.enteredStates.empty()) {
        errors.push_back("Transitions taken but no state changes recorded");
    }

    return errors;
}

std::vector<std::string> MicrostepProcessor::validateMacrostepResult(const MacrostepResult &result) {
    std::vector<std::string> errors;

    if (!result.success && result.errorMessage.empty()) {
        errors.push_back("Macrostep marked as failed but no error message provided");
    }

    if (result.microstepsExecuted != static_cast<int>(result.microsteps.size())) {
        errors.push_back("Microstep count mismatch");
    }

    return errors;
}

// ====== Debugging and Statistics ======

std::string MicrostepProcessor::getExecutionDetails(const MacrostepResult &result) {
    std::ostringstream oss;

    oss << "Macrostep Execution Details:\n";
    oss << "  Success: " << (result.success ? "true" : "false") << "\n";
    oss << "  Microsteps: " << result.microstepsExecuted << "\n";
    oss << "  Execution time: " << result.totalExecutionTimeMs << "ms\n";

    if (!result.errorMessage.empty()) {
        oss << "  Error: " << result.errorMessage << "\n";
    }

    if (result.triggeringEvent) {
        oss << "  Triggering event: " << result.triggeringEvent->getName() << "\n";
    }

    oss << "  Final configuration: {";
    for (size_t i = 0; i < result.finalConfiguration.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << result.finalConfiguration[i];
    }
    oss << "}\n";

    return oss.str();
}

MicrostepProcessor::ExecutionStats MicrostepProcessor::getStatistics() const {
    return stats_;
}

void MicrostepProcessor::resetStatistics() {
    stats_ = ExecutionStats();
    SCXML::Common::Logger::debug("MicrostepProcessor statistics reset");
}

// ====== Private Helper Methods ======

std::shared_ptr<Model::IStateNode> MicrostepProcessor::getStateNode(const std::string &stateId) {
    if (!model_) {
        return nullptr;
    }

    auto rawPtr = model_->findStateById(stateId);
    if (!rawPtr) {
        return nullptr;
    }
    // TODO: Convert raw pointer to shared_ptr properly
    return std::shared_ptr<Model::IStateNode>(rawPtr, [](Model::IStateNode *) {});
}

std::vector<std::string> MicrostepProcessor::getProperAncestors(const std::string &stateId) {
    std::vector<std::string> ancestors;

    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return ancestors;
    }

    auto parent = stateNode->getParent();
    while (parent) {
        ancestors.push_back(parent->getId());
        parent = parent->getParent();
    }

    return ancestors;
}

std::vector<std::string> MicrostepProcessor::getDescendants(const std::string &stateId) {
    std::vector<std::string> descendants;

    auto stateNode = getStateNode(stateId);
    if (!stateNode) {
        return descendants;
    }

    // Recursively collect all descendant states
    std::function<void(std::shared_ptr<Model::IStateNode>)> collectDescendants =
        [&descendants, &collectDescendants](std::shared_ptr<Model::IStateNode> node) {
            if (!node) {
                return;
            }

            auto children = node->getChildren();
            for (const auto &child : children) {
                descendants.push_back(child->getId());
                collectDescendants(child);
            }
        };

    collectDescendants(stateNode);
    return descendants;
}

bool MicrostepProcessor::isAncestor(const std::string &ancestor, const std::string &descendant) {
    auto ancestors = getProperAncestors(descendant);
    return std::find(ancestors.begin(), ancestors.end(), ancestor) != ancestors.end();
}

std::vector<std::string> MicrostepProcessor::sortInDocumentOrder(const std::vector<std::string> &stateIds) {
    auto sorted = stateIds;
    std::sort(sorted.begin(), sorted.end(), [this](const std::string &a, const std::string &b) {
        auto nodeA = getStateNode(a);
        auto nodeB = getStateNode(b);
        if (!nodeA || !nodeB) {
            return a < b;  // Fallback to lexical order
        }
        return nodeA->getDocumentOrder() < nodeB->getDocumentOrder();
    });
    return sorted;
}

std::vector<std::string> MicrostepProcessor::sortInReverseDocumentOrder(const std::vector<std::string> &stateIds) {
    auto sorted = sortInDocumentOrder(stateIds);
    std::reverse(sorted.begin(), sorted.end());
    return sorted;
}

uint64_t MicrostepProcessor::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

void MicrostepProcessor::updateStatistics(const MicrostepResult &result) {
    stats_.totalMicrosteps++;
    stats_.totalStatesEntered += result.enteredStates.size();
    stats_.totalStatesExited += result.exitedStates.size();
    stats_.totalActionsExecuted += result.executedActions.size();
    stats_.totalExecutionTimeMs += result.executionTimeMs;
}

void MicrostepProcessor::logMicrostepExecution(const MicrostepResult &result) {
    if (result.success) {
        SCXML::Common::Logger::debug("Microstep completed: " + microstepResultToString(result));
    } else {
        SCXML::Common::Logger::error("Microstep failed: " + result.errorMessage);
    }
}

void MicrostepProcessor::logMacrostepExecution(const MacrostepResult &result) {
    if (result.success) {
        SCXML::Common::Logger::debug("Macrostep completed: " + macrostepResultToString(result));
    } else {
        SCXML::Common::Logger::error("Macrostep failed: " + result.errorMessage);
    }
}

std::string MicrostepProcessor::microstepResultToString(const MicrostepResult &result) {
    std::ostringstream oss;
    oss << "transitions=" << (result.transitionsTaken ? "yes" : "no");
    oss << ", exited=" << result.exitedStates.size();
    oss << ", entered=" << result.enteredStates.size();
    oss << ", time=" << result.executionTimeMs << "ms";
    return oss.str();
}

std::string MicrostepProcessor::macrostepResultToString(const MacrostepResult &result) {
    std::ostringstream oss;
    oss << "microsteps=" << result.microstepsExecuted;
    oss << ", time=" << result.totalExecutionTimeMs << "ms";
    oss << ", final_states=" << result.finalConfiguration.size();
    return oss.str();
}

}  // namespace Runtime
}  // namespace SCXML