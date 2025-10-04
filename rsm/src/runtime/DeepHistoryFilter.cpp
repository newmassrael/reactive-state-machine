#include "runtime/DeepHistoryFilter.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include <algorithm>
#include <unordered_set>

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

    // W3C SCXML 3.11: Deep history records the deepest active descendant configuration
    // We need to filter out intermediate compound states and keep only the leaf states
    // (atomic or final states) that are descendants of the parent

    // Optimization: Convert activeStateIds to set for O(1) lookup instead of O(n)
    std::unordered_set<std::string> activeStateSet(activeStateIds.begin(), activeStateIds.end());

    for (const auto &stateId : activeStateIds) {
        if (!isDescendant(stateId, parentStateId)) {
            continue;
        }

        // Check if this is a leaf state (atomic or final)
        // Optimized: Check only direct children instead of all states - O(children) vs O(n)
        bool isLeaf = true;

        if (stateProvider_) {
            auto stateNode = stateProvider_(stateId);
            if (stateNode) {
                // Check if any direct child of this state is also active
                const auto &children = stateNode->getChildren();
                for (const auto &child : children) {
                    if (child && activeStateSet.count(child->getId())) {
                        // Found an active child, so this is not a leaf
                        isLeaf = false;
                        LOG_DEBUG("State {} is not a leaf (has active child {})", stateId, child->getId());
                        break;
                    }
                }
            }
        }

        if (isLeaf) {
            filteredStates.push_back(stateId);
            LOG_DEBUG("Recording leaf descendant state: {}", stateId);
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