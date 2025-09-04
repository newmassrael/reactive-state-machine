#include "core/HistoryNode.h"
#include "core/DataModel.h"
#include "runtime/RuntimeContext.h"
#include <algorithm>
#include <regex>
#include <sstream>

namespace SCXML {
namespace Core {

// ============================================================================
// Static utility functions
// ============================================================================

std::string IHistoryNode::historyTypeToString(HistoryType type) {
    switch (type) {
    case HistoryType::SHALLOW:
        return "shallow";
    case HistoryType::DEEP:
        return "deep";
    default:
        return "unknown";
    }
}

HistoryType IHistoryNode::stringToHistoryType(const std::string &typeStr) {
    if (typeStr == "deep") {
        return HistoryType::DEEP;
    } else {
        return HistoryType::SHALLOW;  // Default to shallow
    }
}

}  // namespace Model

namespace Core {

// ============================================================================
// HistoryNode Implementation
// ============================================================================

HistoryNode::HistoryNode(const std::string &id, HistoryType type)
    : id_(id), type_(type), parentState_(""), defaultTarget_("") {}

HistoryType HistoryNode::getType() const {
    return type_;
}

void HistoryNode::setType(HistoryType type) {
    type_ = type;
}

const std::string &HistoryNode::getId() const {
    return id_;
}

void HistoryNode::setId(const std::string &id) {
    id_ = id;
}

const std::string &HistoryNode::getParentState() const {
    return parentState_;
}

void HistoryNode::setParentState(const std::string &parentState) {
    parentState_ = parentState;
}

const std::string &HistoryNode::getDefaultTarget() const {
    return defaultTarget_;
}

void HistoryNode::setDefaultTarget(const std::string &defaultTarget) {
    defaultTarget_ = defaultTarget;
}

Common::Result<void> HistoryNode::recordHistory(SCXML::Model::IExecutionContext &context,
                                                       const std::set<std::string> &activeStates) {
    try {
        std::set<std::string> statesToRecord;

        if (type_ == HistoryType::SHALLOW) {
            statesToRecord = filterShallowHistory(activeStates);
        } else {
            statesToRecord = filterDeepHistory(activeStates);
        }

        // Store history in data model
        std::string storageKey = getStorageKey();
        std::string serializedStates = serializeStateSet(statesToRecord);

        auto result = context.setDataValue(storageKey, serializedStates);

        if (!result.isSuccess()) {
            return Common::Result<void>::failure("Failed to store history: " + result.getErrors()[0].message);
        }

        // Also store timestamp for debugging
        auto timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();

        context.setDataValue(storageKey + "_timestamp", std::to_string(timestamp));

        return Common::Result<void>::success();

    } catch (const std::exception &e) {
        return Common::Result<void>::failure("Exception recording history: " + std::string(e.what()));
    }
}

Common::Result<std::set<std::string>>
HistoryNode::getStoredHistory(SCXML::Model::IExecutionContext &context) const {
    try {
        std::string storageKey = getStorageKey();

        if (!context.hasDataValue(storageKey)) {
            return Common::Result<std::set<std::string>>::success(std::set<std::string>());
        }

        auto result = context.getDataValue(storageKey);
        if (!result.isSuccess()) {
            return Common::Result<std::set<std::string>>::failure("Failed to retrieve history: " +
                                                                         result.getErrors()[0].message);
        }

        std::set<std::string> storedStates = deserializeStateSet(result.getValue());
        return Common::Result<std::set<std::string>>::success(storedStates);

    } catch (const std::exception &e) {
        return Common::Result<std::set<std::string>>::failure("Exception getting stored history: " +
                                                                     std::string(e.what()));
    }
}

Common::Result<void> HistoryNode::clearHistory(SCXML::Model::IExecutionContext &context) {
    try {
        std::string storageKey = getStorageKey();

        // Remove history data and timestamp by setting to empty
        context.setDataValue(storageKey, "");
        context.setDataValue(storageKey + "_timestamp", "");

        return Common::Result<void>::success();

    } catch (const std::exception &e) {
        return Common::Result<void>::failure("Exception clearing history: " + std::string(e.what()));
    }
}

bool HistoryNode::hasHistory(SCXML::Model::IExecutionContext &context) const {
    std::string storageKey = getStorageKey();
    return context.hasDataValue(storageKey);
}

Common::Result<std::set<std::string>>
HistoryNode::resolveHistoryTransition(SCXML::Model::IExecutionContext &context) const {
    try {
        if (hasHistory(context)) {
            // Return stored history
            return getStoredHistory(context);
        } else if (!defaultTarget_.empty()) {
            // Return default target
            std::set<std::string> defaultTargets;
            defaultTargets.insert(defaultTarget_);
            return Common::Result<std::set<std::string>>::success(defaultTargets);
        } else {
            // No history and no default - this is an error condition
            return Common::Result<std::set<std::string>>::failure(
                "History node " + id_ + " has no stored history and no default target");
        }

    } catch (const std::exception &e) {
        return Common::Result<std::set<std::string>>::failure("Exception resolving history transition: " +
                                                                     std::string(e.what()));
    }
}

std::vector<std::string> HistoryNode::validate() const {
    std::vector<std::string> errors;

    // History ID is required
    if (id_.empty()) {
        errors.push_back("History node must have an ID");
    }

    // Parent state is required
    if (parentState_.empty()) {
        errors.push_back("History node must have a parent state");
    }

    // W3C SCXML Compliance: History pseudostates MUST have exactly one default transition
    // Section 3.10: "It [history state] MUST contain exactly one transition child"
    if (defaultTarget_.empty()) {
        errors.push_back("History node must have exactly one default transition (W3C SCXML Section 3.10)");
    }

    // History ID should be unique (would need access to other nodes to validate this)

    // Validate ID format (should be valid identifier)
    std::regex idPattern(R"([a-zA-Z_][a-zA-Z0-9_.]*)");
    if (!id_.empty() && !std::regex_match(id_, idPattern)) {
        errors.push_back("Invalid history node ID format: " + id_);
    }

    return errors;
}

std::shared_ptr<IHistoryNode> HistoryNode::clone() const {
    auto cloned = std::make_shared<HistoryNode>(id_, type_);
    cloned->parentState_ = parentState_;
    cloned->defaultTarget_ = defaultTarget_;
    return cloned;
}

std::string HistoryNode::getStorageKey() const {
    return "_history_" + id_;
}

std::set<std::string> HistoryNode::filterShallowHistory(const std::set<std::string> &activeStates) const {
    std::set<std::string> filteredStates;

    for (const auto &stateId : activeStates) {
        // For shallow history, only record direct children of the parent state
        if (isChildOfParent(stateId) && getStateDepth(stateId) == 0) {
            filteredStates.insert(stateId);
        }
    }

    return filteredStates;
}

std::set<std::string> HistoryNode::filterDeepHistory(const std::set<std::string> &activeStates) const {
    std::set<std::string> filteredStates;

    for (const auto &stateId : activeStates) {
        // For deep history, record all descendants of the parent state
        if (isChildOfParent(stateId)) {
            filteredStates.insert(stateId);
        }
    }

    return filteredStates;
}

bool HistoryNode::isChildOfParent(const std::string &stateId) const {
    if (parentState_.empty()) {
        return false;
    }

    // Simple check: state ID should start with parent state ID followed by a dot
    return stateId.length() > parentState_.length() && stateId.substr(0, parentState_.length()) == parentState_ &&
           (stateId[parentState_.length()] == '.' || stateId.length() == parentState_.length());
}

int HistoryNode::getStateDepth(const std::string &stateId) const {
    if (!isChildOfParent(stateId) || parentState_.empty()) {
        return -1;  // Not a child
    }

    if (stateId.length() == parentState_.length()) {
        return -1;  // This is the parent itself
    }

    // Count dots after the parent state prefix
    std::string suffix = stateId.substr(parentState_.length() + 1);
    return std::count(suffix.begin(), suffix.end(), '.');
}

std::string HistoryNode::serializeStateSet(const std::set<std::string> &states) const {
    if (states.empty()) {
        return "";
    }

    std::ostringstream oss;
    bool first = true;
    for (const auto &state : states) {
        if (!first) {
            oss << ",";
        }
        oss << state;
        first = false;
    }

    return oss.str();
}

std::set<std::string> HistoryNode::deserializeStateSet(const std::string &serialized) const {
    std::set<std::string> states;

    if (serialized.empty()) {
        return states;
    }

    std::stringstream ss(serialized);
    std::string state;

    while (std::getline(ss, state, ',')) {
        if (!state.empty()) {
            states.insert(state);
        }
    }

    return states;
}

// ============================================================================
// HistoryStateManager Implementation
// ============================================================================

HistoryStateManager::HistoryStateManager() {}

Common::Result<void> HistoryStateManager::registerHistoryNode(std::shared_ptr<IHistoryNode> historyNode) {
    if (!historyNode) {
        return Common::Result<void>::failure("Cannot register null history node");
    }

    const std::string &id = historyNode->getId();
    if (id.empty()) {
        return Common::Result<void>::failure("History node must have an ID");
    }

    if (historyNodes_.count(id) > 0) {
        return Common::Result<void>::failure("History node ID already registered: " + id);
    }

    // Validate the history node
    auto validationErrors = historyNode->validate();
    if (!validationErrors.empty()) {
        std::string errorMsg = "History node validation failed: ";
        for (const auto &error : validationErrors) {
            errorMsg += error + "; ";
        }
        return Common::Result<void>::failure(errorMsg);
    }

    historyNodes_[id] = historyNode;
    return Common::Result<void>::success();
}

Common::Result<void> HistoryStateManager::recordHistoryOnExit(SCXML::Model::IExecutionContext &context,
                                                                     const std::set<std::string> &exitingStates,
                                                                     const std::set<std::string> &activeStates) {
    try {
        // Find all history nodes that need to record history when these states are exited
        auto affectedHistories = findAffectedHistories(exitingStates);

        for (auto &historyNode : affectedHistories) {
            auto result = historyNode->recordHistory(context, activeStates);
            if (!result.isSuccess()) {
                return Common::Result<void>::failure("Failed to record history for " + historyNode->getId() +
                                                            ": " + result.getErrors()[0].message);
            }
        }

        return Common::Result<void>::success();

    } catch (const std::exception &e) {
        return Common::Result<void>::failure("Exception recording history on exit: " + std::string(e.what()));
    }
}

Common::Result<std::set<std::string>>
HistoryStateManager::getHistoryTargets(SCXML::Model::IExecutionContext &context, const std::string &historyId) const {
    auto historyNode = findHistoryNode(historyId);
    if (!historyNode) {
        return Common::Result<std::set<std::string>>::failure("History node not found: " + historyId);
    }

    return historyNode->resolveHistoryTransition(context);
}

Common::Result<void> HistoryStateManager::clearHistoryForParent(SCXML::Model::IExecutionContext &context,
                                                                       const std::string &parentStateId) {
    try {
        for (const auto &pair : historyNodes_) {
            const auto &historyNode = pair.second;
            if (historyNode->getParentState() == parentStateId) {
                auto result = historyNode->clearHistory(context);
                if (!result.isSuccess()) {
                    return Common::Result<void>::failure("Failed to clear history for " + historyNode->getId() +
                                                                ": " + result.getErrors()[0].message);
                }
            }
        }

        return Common::Result<void>::success();

    } catch (const std::exception &e) {
        return Common::Result<void>::failure("Exception clearing history for parent: " + std::string(e.what()));
    }
}

std::vector<std::shared_ptr<IHistoryNode>> HistoryStateManager::getAllHistoryNodes() const {
    std::vector<std::shared_ptr<IHistoryNode>> nodes;
    for (const auto &pair : historyNodes_) {
        nodes.push_back(pair.second);
    }
    return nodes;
}

std::shared_ptr<IHistoryNode> HistoryStateManager::findHistoryNode(const std::string &historyId) const {
    auto it = historyNodes_.find(historyId);
    return it != historyNodes_.end() ? it->second : nullptr;
}

std::vector<std::string> HistoryStateManager::validateAllHistories() const {
    std::vector<std::string> allErrors;

    for (const auto &pair : historyNodes_) {
        auto errors = pair.second->validate();
        for (const auto &error : errors) {
            allErrors.push_back(pair.first + ": " + error);
        }
    }

    // Check for duplicate parent states with multiple histories
    std::map<std::string, std::vector<std::string>> parentToHistories;
    for (const auto &pair : historyNodes_) {
        const std::string &parentState = pair.second->getParentState();
        parentToHistories[parentState].push_back(pair.first);
    }

    for (const auto &parentPair : parentToHistories) {
        if (parentPair.second.size() > 1) {
            std::string historyList;
            for (const auto &histId : parentPair.second) {
                if (!historyList.empty()) {
                    historyList += ", ";
                }
                historyList += histId;
            }
            allErrors.push_back("Multiple history nodes for parent state " + parentPair.first + ": " + historyList);
        }
    }

    return allErrors;
}

std::string HistoryStateManager::exportHistoryState(SCXML::Model::IExecutionContext &context) const {
    std::ostringstream json;
    json << "{\"historyStates\":[";

    bool first = true;
    for (const auto &pair : historyNodes_) {
        if (!first) {
            json << ",";
        }
        first = false;

        const auto &historyNode = pair.second;
        json << "{";
        json << "\"id\":\"" << historyNode->getId() << "\",";
        json << "\"type\":\"" << IHistoryNode::historyTypeToString(historyNode->getType()) << "\",";
        json << "\"parentState\":\"" << historyNode->getParentState() << "\",";
        json << "\"defaultTarget\":\"" << historyNode->getDefaultTarget() << "\",";
        json << "\"hasHistory\":" << (historyNode->hasHistory(context) ? "true" : "false");

        if (historyNode->hasHistory(context)) {
            auto storedResult = historyNode->getStoredHistory(context);
            if (storedResult.isSuccess()) {
                json << ",\"storedStates\":[";
                bool firstState = true;
                for (const auto &state : storedResult.getValue()) {
                    if (!firstState) {
                        json << ",";
                    }
                    json << "\"" << state << "\"";
                    firstState = false;
                }
                json << "]";
            }
        }

        json << "}";
    }

    json << "]}";
    return json.str();
}

std::vector<std::shared_ptr<IHistoryNode>>
HistoryStateManager::findAffectedHistories(const std::set<std::string> &exitingStates) const {
    std::vector<std::shared_ptr<IHistoryNode>> affected;

    for (const auto &pair : historyNodes_) {
        if (shouldRecordHistory(pair.second, exitingStates)) {
            affected.push_back(pair.second);
        }
    }

    return affected;
}

std::set<std::string> HistoryStateManager::getAncestorStates(const std::string &stateId) const {
    std::set<std::string> ancestors;

    size_t pos = stateId.find_last_of('.');
    while (pos != std::string::npos) {
        std::string ancestor = stateId.substr(0, pos);
        ancestors.insert(ancestor);
        pos = ancestor.find_last_of('.');
    }

    return ancestors;
}

bool HistoryStateManager::shouldRecordHistory(std::shared_ptr<IHistoryNode> historyNode,
                                              const std::set<std::string> &exitingStates) const {
    const std::string &parentState = historyNode->getParentState();

    // Check if the parent state itself is being exited
    if (exitingStates.count(parentState) > 0) {
        return true;
    }

    // Check if any ancestor of the parent state is being exited
    auto ancestors = getAncestorStates(parentState);
    for (const auto &ancestor : ancestors) {
        if (exitingStates.count(ancestor) > 0) {
            return true;
        }
    }

    return false;
}

}  // namespace Core
} // namespace Core
} // namespace SCXML
