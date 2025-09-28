#include "runtime/SCXMLModelExtractor.h"
#include "common/Logger.h"
#include "model/IDataModelItem.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "model/SCXMLModel.h"
#include <unordered_set>

namespace RSM {

SCXMLModelExtractor::ExtractedModel SCXMLModelExtractor::extractModel(std::shared_ptr<SCXMLModel> model) {
    ExtractedModel extracted;

    if (!model) {
        LOG_ERROR("SCXMLModelExtractor: Null model provided");
        return extracted;
    }

    // Set model metadata
    extracted.name = model->getName();
    extracted.initialState = model->getInitialState();
    extracted.version = "1.0";  // Default version

    // Extract data model items
    const auto &dataModelItems = model->getDataModelItems();
    for (const auto &item : dataModelItems) {
        DataItem dataItem;
        dataItem.id = item->getId();
        dataItem.type = "";          // Type not available in current API
        dataItem.initialValue = "";  // Not available in current API
        dataItem.expression = item->getExpr();
        extracted.dataItems.push_back(dataItem);
    }

    // Extract states and transitions recursively
    const auto &allStates = model->getAllStates();
    for (const auto &state : allStates) {
        extractRecursively(state, "", extracted);
    }

    // Validate extracted model
    validateExtractedModel(extracted);

    return extracted;
}

SCXMLModelExtractor::StateInfo SCXMLModelExtractor::extractState(std::shared_ptr<IStateNode> stateNode,
                                                                 const std::string &parentPath) {
    StateInfo info;
    info.id = stateNode->getId();
    info.fullPath = parentPath.empty() ? info.id : parentPath + "." + info.id;
    info.isInitial = false;  // Will be set by parent logic
    info.isFinal = stateNode->isFinalState();
    info.parent = parentPath;

    // Extract onEntry actions
    const auto &entryActions = stateNode->getEntryActions();
    info.onEntry = entryActions;

    // Extract onExit actions
    const auto &exitActions = stateNode->getExitActions();
    info.onExit = exitActions;

    return info;
}

SCXMLModelExtractor::TransitionInfo
SCXMLModelExtractor::extractTransition(std::shared_ptr<ITransitionNode> transitionNode, const std::string &fromState) {
    TransitionInfo info;
    info.fromState = fromState;
    info.event = transitionNode->getEvent();

    // Handle multiple targets - use first target if available
    auto targets = transitionNode->getTargets();
    if (!targets.empty()) {
        info.toState = targets[0];  // Use first target
    }

    // Get guard condition
    info.condition = transitionNode->getGuard();

    // Extract actions
    const auto &actionNodes = transitionNode->getActionNodes();
    // ActionNode에서 스크립트 추출 (임시 구현)
    std::string actionScript;
    for (const auto &actionNode : actionNodes) {
        if (actionNode) {
            if (!actionScript.empty()) {
                actionScript += "; ";
            }
            actionScript += actionNode->getActionType();
        }
    }
    info.action = actionScript;

    info.priority = 0;  // Default priority
    info.type = transitionNode->isInternal() ? "internal" : "external";

    return info;
}

std::string SCXMLModelExtractor::extractScriptFromActions(const std::vector<std::string> &actionIds) {
    // Simple implementation - concatenate action IDs
    // In a full implementation, this would resolve action IDs to their script content
    std::string result;
    for (size_t i = 0; i < actionIds.size(); ++i) {
        if (i > 0) {
            result += "; ";
        }
        result += actionIds[i];
    }
    return result;
}

void SCXMLModelExtractor::extractRecursively(std::shared_ptr<IStateNode> stateNode, const std::string &parentPath,
                                             ExtractedModel &extracted) {
    if (!stateNode) {
        return;
    }

    // Extract current state
    StateInfo stateInfo = extractState(stateNode, parentPath);
    extracted.states.push_back(stateInfo);

    // Extract transitions from this state
    const auto &transitions = stateNode->getTransitions();
    for (const auto &transition : transitions) {
        TransitionInfo transInfo = extractTransition(transition, stateInfo.id);
        extracted.transitions.push_back(transInfo);
    }

    // Recursively extract child states
    const auto &childStates = stateNode->getChildren();
    for (const auto &child : childStates) {
        extractRecursively(child, stateInfo.fullPath, extracted);
    }
}

bool SCXMLModelExtractor::validateExtractedModel(ExtractedModel &extracted) {
    bool isValid = true;
    std::unordered_set<std::string> stateIds;

    // Collect all state IDs
    for (const auto &state : extracted.states) {
        stateIds.insert(state.id);
    }

    // Validate transition targets
    for (const auto &transition : extracted.transitions) {
        if (!transition.toState.empty() && stateIds.find(transition.toState) == stateIds.end()) {
            LOG_WARN("Transition target '{}' not found in extracted states", transition.toState);
            isValid = false;
        }
    }

    return isValid;
}

}  // namespace RSM