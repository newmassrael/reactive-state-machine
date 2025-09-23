#include "runtime/DeepHistoryFilter.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include <algorithm>

namespace RSM {

DeepHistoryFilter::DeepHistoryFilter(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider)
    : stateProvider_(std::move(stateProvider)) {
    Logger::debug("DeepHistoryFilter: Initialized deep history filter");
}

std::vector<std::string> DeepHistoryFilter::filterStates(const std::vector<std::string> &activeStateIds,
                                                         const std::string &parentStateId) const {
    Logger::debug("DeepHistoryFilter: Filtering states for deep history - parent: " + parentStateId +
                  ", active states: " + std::to_string(activeStateIds.size()));

    if (parentStateId.empty()) {
        Logger::warn("DeepHistoryFilter: Empty parent state ID provided");
        return {};
    }

    std::vector<std::string> filteredStates;

    // For deep history, we record all descendant states of the parent
    for (const auto &stateId : activeStateIds) {
        if (isDescendant(stateId, parentStateId)) {
            filteredStates.push_back(stateId);
            Logger::debug("DeepHistoryFilter: Recording descendant state: " + stateId);
        }
    }

    Logger::info("DeepHistoryFilter: Filtered " + std::to_string(activeStateIds.size()) + " states to " +
                 std::to_string(filteredStates.size()) + " descendants");

    return filteredStates;
}

bool DeepHistoryFilter::isDescendant(const std::string &stateId, const std::string &parentStateId) const {
    if (stateId == parentStateId) {
        // A state is not a descendant of itself for history purposes
        return false;
    }

    auto statePath = getStatePath(stateId);

    // Check if parentStateId is in the path (ancestor)
    auto it = std::find(statePath.begin(), statePath.end(), parentStateId);
    bool isDescendant = (it != statePath.end());

    Logger::debug("DeepHistoryFilter: State " + stateId + " is " + (isDescendant ? "" : "not ") + "descendant of " +
                  parentStateId);

    return isDescendant;
}

std::vector<std::string> DeepHistoryFilter::getStatePath(const std::string &stateId) const {
    std::vector<std::string> path;

    if (!stateProvider_) {
        Logger::error("DeepHistoryFilter: No state provider available");
        return path;
    }

    auto currentState = stateProvider_(stateId);

    // Build path from state to root
    while (currentState != nullptr) {
        path.insert(path.begin(), currentState->getId());
        currentState = currentState->getParent() ? stateProvider_(currentState->getParent()->getId()) : nullptr;
    }

    Logger::debug("DeepHistoryFilter: State path for " + stateId + " has " + std::to_string(path.size()) + " levels");

    return path;
}

}  // namespace RSM