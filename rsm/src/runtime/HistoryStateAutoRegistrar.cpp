#include "runtime/HistoryStateAutoRegistrar.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include "model/SCXMLModel.h"
#include "runtime/IHistoryManager.h"
#include "types.h"
#include <algorithm>

namespace RSM {

HistoryStateAutoRegistrar::HistoryStateAutoRegistrar(
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider)
    : stateProvider_(std::move(stateProvider)) {
    Logger::debug("HistoryStateAutoRegistrar: Initialized with SOLID architecture");
}

bool HistoryStateAutoRegistrar::autoRegisterHistoryStates(const std::shared_ptr<SCXMLModel> &model,
                                                          IHistoryManager *historyManager) {
    if (!autoRegistrationEnabled_) {
        Logger::debug("HistoryStateAutoRegistrar: Auto-registration is disabled");
        return true;
    }

    if (!model) {
        Logger::error("HistoryStateAutoRegistrar: Cannot register - null model");
        return false;
    }

    if (!historyManager) {
        Logger::error("HistoryStateAutoRegistrar: Cannot register - null history manager");
        return false;
    }

    Logger::info("HistoryStateAutoRegistrar: Starting SCXML W3C compliant auto-registration");

    // Extract history states from model
    auto historyStates = extractHistoryStatesFromModel(model);

    if (historyStates.empty()) {
        Logger::debug("HistoryStateAutoRegistrar: No history states found in model");
        registeredHistoryStateCount_ = 0;
        return true;
    }

    size_t successCount = 0;
    for (const auto &historyInfo : historyStates) {
        if (registerSingleHistoryState(historyInfo.historyStateId, historyInfo.parentStateId, historyInfo.historyType,
                                       historyInfo.defaultStateId, historyManager)) {
            successCount++;
            Logger::debug("HistoryStateAutoRegistrar: Successfully registered history state: " +
                          historyInfo.historyStateId);
        } else {
            Logger::warn("HistoryStateAutoRegistrar: Failed to register history state: " + historyInfo.historyStateId);
        }
    }

    registeredHistoryStateCount_ = successCount;

    if (successCount == historyStates.size()) {
        Logger::info("HistoryStateAutoRegistrar: Successfully registered all " + std::to_string(successCount) +
                     " history states");
        return true;
    } else {
        Logger::warn("HistoryStateAutoRegistrar: Registered " + std::to_string(successCount) + " out of " +
                     std::to_string(historyStates.size()) + " history states");
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
    Logger::debug("HistoryStateAutoRegistrar: Auto-registration " + std::string(enabled ? "enabled" : "disabled"));
}

bool HistoryStateAutoRegistrar::registerSingleHistoryState(const std::string &historyStateId,
                                                           const std::string &parentStateId, HistoryType historyType,
                                                           const std::string &defaultStateId,
                                                           IHistoryManager *historyManager) {
    // Validate history type
    if (historyType == HistoryType::NONE) {
        Logger::error("HistoryStateAutoRegistrar: Invalid history type NONE for state '" + historyStateId + "'");
        return false;
    }

    // Convert enum to string for logging
    std::string historyTypeStr;
    if (historyType == HistoryType::SHALLOW) {
        historyTypeStr = "shallow";
    } else if (historyType == HistoryType::DEEP) {
        historyTypeStr = "deep";
    } else {
        Logger::error("HistoryStateAutoRegistrar: Invalid history type for state '" + historyStateId + "'");
        return false;
    }

    // Register with history manager (using the enum directly)
    bool success = historyManager->registerHistoryState(historyStateId, parentStateId, historyType, defaultStateId);

    if (success) {
        Logger::debug("HistoryStateAutoRegistrar: Registered " + historyTypeStr + " history state '" + historyStateId +
                      "' in parent '" + parentStateId + "' with default '" + defaultStateId + "'");
    } else {
        Logger::error("HistoryStateAutoRegistrar: Failed to register history state '" + historyStateId + "'");
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

            // Get default state (from initial state or first child)
            info.defaultStateId = stateNode->getInitialState();
            if (info.defaultStateId.empty() && stateNode->getParent()) {
                // If no default specified, use parent's initial state
                info.defaultStateId = stateNode->getParent()->getInitialState();
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

    Logger::warn("HistoryStateAutoRegistrar: Could not find parent for history state: " + historyStateId);
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

    Logger::debug("HistoryStateAutoRegistrar: No default state found for history state: " + historyState->getId());
    return "";
}

}  // namespace RSM