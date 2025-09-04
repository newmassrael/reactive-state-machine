#include "core/ParallelStateNode.h"
#include "common/Logger.h"
#include <algorithm>
#include <stack>

namespace SCXML {
namespace Core {

ParallelStateNode::ParallelStateNode(const std::string &id) : StateNode(id, Type::PARALLEL) {
    SCXML::Common::Logger::debug("ParallelStateNode::Constructor - Creating parallel state: " + id);
}

void ParallelStateNode::addChild(std::shared_ptr<SCXML::Model::IStateNode> child) {
    StateNode::addChild(child);

    if (child) {
        initializeRegionTracking(child);
        SCXML::Common::Logger::debug("ParallelStateNode::addChild - Added parallel region: " + child->getId() + " to " + getId());
    }
}

void ParallelStateNode::addParallelRegion(std::shared_ptr<SCXML::Model::IStateNode> region,
                                          const std::string &initialState) {
    if (!region) {
        SCXML::Common::Logger::warning("ParallelStateNode::addParallelRegion - Null region provided");
        return;
    }

    addChild(region);

    // Set custom initial state if provided
    if (!initialState.empty()) {
        std::lock_guard<std::mutex> lock(regionStateMutex_);
        regionInitialStates_[region->getId()] = initialState;
        SCXML::Common::Logger::debug("ParallelStateNode::addParallelRegion - Set initial state for region " + region->getId() + ": " +
                      initialState);
    }
}

std::vector<std::shared_ptr<SCXML::Model::IStateNode>> ParallelStateNode::getParallelRegions() const {
    return getChildren();  // Parallel regions are the same as children
}

bool ParallelStateNode::areAllRegionsComplete() const {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    for (const auto &pair : regionCompletionStatus_) {
        if (!pair.second) {
            return false;  // Found an incomplete region
        }
    }

    return !regionCompletionStatus_.empty();  // True only if we have regions and all are complete
}

std::unordered_map<std::string, bool> ParallelStateNode::getRegionCompletionStatus() const {
    std::lock_guard<std::mutex> lock(regionStateMutex_);
    return regionCompletionStatus_;
}

bool ParallelStateNode::isRegionComplete(const std::string &regionId) const {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    auto it = regionCompletionStatus_.find(regionId);
    return (it != regionCompletionStatus_.end()) ? it->second : false;
}

void ParallelStateNode::setRegionActiveStates(const std::string &regionId,
                                              const std::unordered_set<std::string> &activeStates) {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    regionActiveStates_[regionId] = activeStates;

    // Update completion status based on whether region has active states
    regionCompletionStatus_[regionId] =
        activeStates.empty() || std::any_of(activeStates.begin(), activeStates.end(),
                                            [this](const std::string &stateId) { return this->isStateFinal(stateId); });

    SCXML::Common::Logger::debug("ParallelStateNode::setRegionActiveStates - Region " + regionId + " has " +
                  std::to_string(activeStates.size()) + " active states, " +
                  "complete: " + (regionCompletionStatus_[regionId] ? "true" : "false"));
}

std::unordered_set<std::string> ParallelStateNode::getRegionActiveStates(const std::string &regionId) const {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    auto it = regionActiveStates_.find(regionId);
    return (it != regionActiveStates_.end()) ? it->second : std::unordered_set<std::string>();
}

std::unordered_set<std::string> ParallelStateNode::getAllActiveStates() const {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    std::unordered_set<std::string> allActiveStates;

    for (const auto &pair : regionActiveStates_) {
        const auto &regionStates = pair.second;
        allActiveStates.insert(regionStates.begin(), regionStates.end());
    }

    return allActiveStates;
}

void ParallelStateNode::markRegionComplete(const std::string &regionId) {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    regionCompletionStatus_[regionId] = true;
    SCXML::Common::Logger::debug("ParallelStateNode::markRegionComplete - Marked region complete: " + regionId);
}

void ParallelStateNode::markRegionActive(const std::string &regionId) {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    regionCompletionStatus_[regionId] = false;
    SCXML::Common::Logger::debug("ParallelStateNode::markRegionActive - Marked region active: " + regionId);
}

void ParallelStateNode::resetRegionStates() {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    for (auto &pair : regionCompletionStatus_) {
        pair.second = false;  // Mark all regions as active (not complete)
    }

    regionActiveStates_.clear();

    SCXML::Common::Logger::debug("ParallelStateNode::resetRegionStates - Reset all region states for " + getId());
}

std::vector<std::string> ParallelStateNode::validateParallelStructure() const {
    std::vector<std::string> errors;

    const auto &regions = getParallelRegions();

    // Check minimum regions requirement
    if (regions.empty()) {
        errors.push_back("Parallel state '" + getId() + "' has no child regions");
        return errors;
    }

    if (regions.size() < 2) {
        errors.push_back("Parallel state '" + getId() + "' should have at least 2 regions, found " +
                         std::to_string(regions.size()));
    }

    // Validate each region
    for (const auto &region : regions) {
        if (!region) {
            errors.push_back("Parallel state '" + getId() + "' contains null region");
            continue;
        }

        // Check region type - should not be parallel itself (nested parallel not recommended)
        if (region->getType() == Type::PARALLEL) {
            errors.push_back("Parallel state '" + getId() + "' contains nested parallel region '" + region->getId() +
                             "' - this may cause complexity issues");
        }

        // Check region has proper structure
        if (region->getType() == Type::COMPOUND && region->getChildren().empty()) {
            errors.push_back("Compound region '" + region->getId() + "' in parallel state '" + getId() +
                             "' has no child states");
        }
    }

    return errors;
}

bool ParallelStateNode::canProcessEvents() const {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    // Can process events if at least one region is active (not complete)
    for (const auto &pair : regionCompletionStatus_) {
        if (!pair.second) {
            return true;
        }
    }

    return false;  // All regions complete, cannot process events
}

std::unordered_map<std::string, std::string> ParallelStateNode::getRegionInitialStates() const {
    std::lock_guard<std::mutex> lock(regionStateMutex_);

    std::unordered_map<std::string, std::string> result;

    for (const auto &region : getParallelRegions()) {
        if (region) {
            std::string regionId = region->getId();

            // Use explicit initial state if set, otherwise find automatically
            auto it = regionInitialStates_.find(regionId);
            if (it != regionInitialStates_.end()) {
                result[regionId] = it->second;
            } else {
                result[regionId] = findRegionInitialState(region);
            }
        }
    }

    return result;
}

void ParallelStateNode::initializeRegionTracking(std::shared_ptr<SCXML::Model::IStateNode> child) {
    if (!child) {
        return;
    }

    std::lock_guard<std::mutex> lock(regionStateMutex_);

    std::string regionId = child->getId();

    // Initialize empty active states
    regionActiveStates_[regionId] = std::unordered_set<std::string>();

    // Initialize as active (not complete)
    regionCompletionStatus_[regionId] = false;

    // Find and store initial state
    regionInitialStates_[regionId] = findRegionInitialState(child);

    SCXML::Common::Logger::debug("ParallelStateNode::initializeRegionTracking - Initialized tracking for region: " + regionId);
}

std::string ParallelStateNode::findRegionInitialState(std::shared_ptr<SCXML::Model::IStateNode> region) const {
    if (!region) {
        return "";
    }

    // Check if region has explicit initial state
    std::string initialState = region->getInitialState();
    if (!initialState.empty()) {
        return initialState;
    }

    // For compound states, find first child as initial
    if (region->getType() == Type::COMPOUND) {
        const auto &children = region->getChildren();
        if (!children.empty() && children[0]) {
            return children[0]->getId();
        }
    }

    // For atomic states, the state itself is the initial state
    if (region->getType() == Type::ATOMIC) {
        return region->getId();
    }

    SCXML::Common::Logger::warning("ParallelStateNode::findRegionInitialState - Could not determine initial state for region: " +
                    region->getId());
    return "";
}

bool ParallelStateNode::isRegionInFinalState(std::shared_ptr<SCXML::Model::IStateNode> region) const {
    if (!region) {
        return false;
    }

    // Check if region itself is final
    if (region->getType() == Type::FINAL) {
        return true;
    }

    // For compound regions, check if current active state is final
    std::string regionId = region->getId();
    auto activeStates = getRegionActiveStates(regionId);

    for (const std::string &stateId : activeStates) {
        if (isStateFinal(stateId)) {
            return true;
        }
    }

    return false;
}

bool ParallelStateNode::isStateFinal(const std::string &stateId) const {
    // Helper method to check if a state ID represents a final state
    // This would typically lookup the state in the model and check its type
    // For now, we use a naming convention
    return stateId.find("final") != std::string::npos || stateId.find("Final") != std::string::npos ||
           stateId.find("FINAL") != std::string::npos;
}

}  // namespace Core
}  // namespace SCXML
