#include "runtime/HistoryStateAutoRegistrar.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "model/SCXMLModel.h"
#include "runtime/HistoryManager.h"
#include "types.h"
#include <algorithm>
#include <cassert>

namespace RSM {

HistoryStateAutoRegistrar::HistoryStateAutoRegistrar(
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider)
    : stateProvider_(std::move(stateProvider)) {
    LOG_DEBUG("HistoryStateAutoRegistrar: Initialized with SOLID architecture");
}

bool HistoryStateAutoRegistrar::autoRegisterHistoryStates(const std::shared_ptr<SCXMLModel> &model,
                                                          HistoryManager *historyManager) {
    if (!autoRegistrationEnabled_) {
        LOG_DEBUG("HistoryStateAutoRegistrar: Auto-registration is disabled");
        return true;
    }

    if (!model) {
        LOG_ERROR("HistoryStateAutoRegistrar: Cannot register - null model");
        return false;
    }

    if (!historyManager) {
        LOG_ERROR("HistoryStateAutoRegistrar: Cannot register - null history manager");
        return false;
    }

    LOG_INFO("HistoryStateAutoRegistrar: Starting SCXML W3C compliant auto-registration");

    // Extract history states from model
    auto historyStates = extractHistoryStatesFromModel(model);

    if (historyStates.empty()) {
        LOG_DEBUG("HistoryStateAutoRegistrar: No history states found in model");
        registeredHistoryStateCount_ = 0;
        return true;
    }

    size_t successCount = 0;
    for (const auto &historyInfo : historyStates) {
        if (registerSingleHistoryState(historyInfo.historyStateId, historyInfo.parentStateId, historyInfo.historyType,
                                       historyInfo.defaultStateId, historyManager)) {
            successCount++;
            LOG_DEBUG("Successfully registered history state: {}", historyInfo.historyStateId);
        } else {
            LOG_WARN("Failed to register history state: {}", historyInfo.historyStateId);
        }
    }

    registeredHistoryStateCount_ = successCount;

    if (successCount == historyStates.size()) {
        LOG_INFO("Successfully registered all {} history states", successCount);
        return true;
    } else {
        LOG_WARN("Registered {} out of {} history states", successCount, historyStates.size());
        return false;
    }
}

size_t HistoryStateAutoRegistrar::getRegisteredHistoryStateCount() const {
    return registeredHistoryStateCount_;
}

bool HistoryStateAutoRegistrar::isAutoRegistrationEnabled() const {
    return autoRegistrationEnabled_;
}

void HistoryStateAutoRegistrar::setAutoRegistrationEnabled(bool enabled) {
    autoRegistrationEnabled_ = enabled;
    LOG_DEBUG("Auto-registration {}", (enabled ? "enabled" : "disabled"));
}

bool HistoryStateAutoRegistrar::registerSingleHistoryState(const std::string &historyStateId,
                                                           const std::string &parentStateId, HistoryType historyType,
                                                           const std::string &defaultStateId,
                                                           HistoryManager *historyManager) {
    // Validate history type
    if (historyType == HistoryType::NONE) {
        LOG_ERROR("Invalid history type NONE for state '{}'", historyStateId);
        return false;
    }

    // Convert enum to string for logging
    std::string historyTypeStr;
    if (historyType == HistoryType::SHALLOW) {
        historyTypeStr = "shallow";
    } else if (historyType == HistoryType::DEEP) {
        historyTypeStr = "deep";
    } else {
        LOG_ERROR("Invalid history type for state '{}'", historyStateId);
        return false;
    }

    // Register with history manager (using the enum directly)
    bool success = historyManager->registerHistoryState(historyStateId, parentStateId, historyType, defaultStateId);

    if (success) {
        LOG_DEBUG("Registered {} history state '{}' in parent '{}' with default '{}'", historyTypeStr, historyStateId,
                  parentStateId, defaultStateId);
    } else {
        LOG_ERROR("Failed to register history state '{}'", historyStateId);
    }

    return success;
}

std::vector<HistoryStateAutoRegistrar::HistoryStateInfo>
HistoryStateAutoRegistrar::extractHistoryStatesFromModel(const std::shared_ptr<SCXMLModel> &model) {
    std::vector<HistoryStateInfo> historyStates;

    // Get all states from model
    const auto &allStates = model->getAllStates();

    for (const auto &stateNode : allStates) {
        // Check if this is a history state
        if (stateNode->getType() == Type::HISTORY) {
            HistoryStateInfo info;
            info.historyStateId = stateNode->getId();

            // Get parent state ID
            if (stateNode->getParent()) {
                info.parentStateId = stateNode->getParent()->getId();
            }

            // Get history type from state node
            info.historyType = stateNode->getHistoryType();

            // W3C SCXML 3.6: Get default state from history state's transition
            // History states specify their default via <transition target="..."/>
            // Note: initial attribute is NOT valid for history states per W3C spec

            // Look for eventless transition in history state
            const auto &transitions = stateNode->getTransitions();
            if (!transitions.empty()) {
                // W3C SCXML: First eventless transition's target is the default
                for (const auto &trans : transitions) {
                    if (trans->getEvent().empty() && !trans->getTargets().empty()) {
                        info.defaultStateId = trans->getTargets()[0];
                        LOG_DEBUG("HistoryStateAutoRegistrar: Found default '{}' from transition in history '{}'",
                                  info.defaultStateId, info.historyStateId);
                        break;
                    }
                }
            }

            // W3C SCXML 3.6: History states without default transition will trigger error.platform at runtime
            // This is allowed by the spec - default is optional, error handling is runtime responsibility
            if (info.defaultStateId.empty()) {
                LOG_WARN("HistoryStateAutoRegistrar: History state '{}' has no default transition - will generate "
                         "error.platform if history is empty",
                         info.historyStateId);
            }

            historyStates.push_back(info);
        }
    }

    return historyStates;
}

std::string HistoryStateAutoRegistrar::findParentStateId(const std::string &historyStateId,
                                                         const std::shared_ptr<SCXMLModel> &model) {
    // Search through all states to find the one that contains this history state
    const auto &allStates = model->getAllStates();

    for (const auto &stateNode : allStates) {
        // Check if this state contains the history state as a child
        const auto &children = stateNode->getChildren();
        for (const auto &child : children) {
            if (child->getId() == historyStateId) {
                return stateNode->getId();
            }
        }
    }

    LOG_WARN("Could not find parent for history state: {}", historyStateId);
    return "";
}

std::string HistoryStateAutoRegistrar::extractDefaultStateId(const std::shared_ptr<IStateNode> &historyState) {
    // For SCXML compliance, the default should be specified in the parent's initial state
    // or can be explicitly defined in SCXML. For now, we'll rely on the parent state's
    // initial state or return empty string if not available.
    if (historyState->getParent()) {
        const std::string &parentInitial = historyState->getParent()->getInitialState();
        if (!parentInitial.empty()) {
            return parentInitial;
        }
    }

    LOG_DEBUG("No default state found for history state: {}", historyState->getId());
    return "";
}

}  // namespace RSM