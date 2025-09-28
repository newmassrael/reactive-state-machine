#include "runtime/ShallowHistoryFilter.h"
#include "common/Logger.h"
#include "model/IStateNode.h"
#include <algorithm>

namespace RSM {

ShallowHistoryFilter::ShallowHistoryFilter(
    std::function<std::shared_ptr<IStateNode>(const std::string &)> stateProvider)
    : stateProvider_(std::move(stateProvider)) {
    LOG_DEBUG("ShallowHistoryFilter: Initialized shallow history filter");
}

std::vector<std::string> ShallowHistoryFilter::filterStates(const std::vector<std::string> &activeStateIds,
                                                            const std::string &parentStateId) const {
    LOG_DEBUG("Filtering states for shallow history - parent: {}, active states: {}", parentStateId,
              activeStateIds.size());

    if (parentStateId.empty()) {
        LOG_WARN("ShallowHistoryFilter: Empty parent state ID provided");
        return {};
    }

    std::vector<std::string> filteredStates;

    // For shallow history, we only record immediate child states of the parent
    for (const auto &stateId : activeStateIds) {
        if (isImmediateChild(stateId, parentStateId)) {
            filteredStates.push_back(stateId);
            LOG_DEBUG("Recording immediate child state: {}", stateId);
        }
    }

    LOG_INFO("Filtered {} states to {} immediate children", activeStateIds.size(), filteredStates.size());

    return filteredStates;
}

bool ShallowHistoryFilter::isImmediateChild(const std::string &stateId, const std::string &parentStateId) const {
    if (!stateProvider_) {
        LOG_ERROR("ShallowHistoryFilter: No state provider available");
        return false;
    }

    auto state = stateProvider_(stateId);
    if (!state) {
        LOG_WARN("State not found: {}", stateId);
        return false;
    }

    auto parent = state->getParent();
    if (!parent) {
        LOG_DEBUG("State {} has no parent", stateId);
        return false;
    }

    bool isImmediate = (parent->getId() == parentStateId);
    LOG_DEBUG("State {} is {}immediate child of {}", stateId, (isImmediate ? "" : "not "), parentStateId);

    return isImmediate;
}

}  // namespace RSM