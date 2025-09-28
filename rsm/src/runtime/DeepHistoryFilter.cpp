#include "runtime/DeepHistoryFilter.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include <algorithm>

namespace RSM {

DeepHistoryFilter::DeepHistoryFilter(std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider)
    : stateProvider_(std::move(stateProvider)) {
    LOG_DEBUG("DeepHistoryFilter: Initialized deep history filter");
}

std::vector<std::string> DeepHistoryFilter::filterStates(const std::vector<std::string> &activeStateIds,
                                                         const std::string &parentStateId) const {
    LOG_DEBUG("Filtering states for deep history - parent: {}, active states: {}", parentStateId,
              activeStateIds.size());

    if (parentStateId.empty()) {
        LOG_WARN("DeepHistoryFilter: Empty parent state ID provided");
        return {};
    }

    std::vector<std::string> filteredStates;

    // For deep history, we record all descendant states of the parent
    for (const auto &stateId : activeStateIds) {
        if (isDescendant(stateId, parentStateId)) {
            filteredStates.push_back(stateId);
            LOG_DEBUG("Recording descendant state: {}", stateId);
        }
    }

    LOG_INFO("Filtered {} states to {} descendants", activeStateIds.size(), filteredStates.size());

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

    LOG_DEBUG("State {} is {}descendant of {}", stateId, (isDescendant ? "" : "not "), parentStateId);

    return isDescendant;
}

std::vector<std::string> DeepHistoryFilter::getStatePath(const std::string &stateId) const {
    std::vector<std::string> path;

    if (!stateProvider_) {
        LOG_ERROR("DeepHistoryFilter: No state provider available");
        return path;
    }

    auto currentState = stateProvider_(stateId);

    // Build path from state to root
    while (currentState != nullptr) {
        path.insert(path.begin(), currentState->getId());
        currentState = currentState->getParent() ? stateProvider_(currentState->getParent()->getId()) : nullptr;
    }

    LOG_DEBUG("State path for {} has {} levels", stateId, path.size());

    return path;
}

}  // namespace RSM