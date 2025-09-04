#include "model/StateHierarchyManager.h"
#include "common/PerformanceUtils.h"
#include "model/DocumentModel.h"
#include "model/IStateNode.h"
#include <algorithm>
#include <queue>

namespace SCXML {

StateHierarchyManager::StateHierarchyManager(std::shared_ptr<Model::DocumentModel> model)
    : model_(model), rootStateId_() {}

bool StateHierarchyManager::buildHierarchy() {
    if (!model_) {
        return false;
    }

    // Clear existing cache
    clearCache();

    try {
        // Get root state
        Model::IStateNode *rootState = model_->getRootState();
        if (!rootState) {
            return false;
        }

        rootStateId_ = rootState->getId();
        stateSet_.insert(rootStateId_);

        // Build relationships recursively
        buildRelationships(std::shared_ptr<Model::IStateNode>(rootState, [](Model::IStateNode *) {}), "");

        // Build depth cache
        buildDepthCache();

        // Validate hierarchy
        return validateHierarchy();

    } catch (const std::exception &e) {
        clearCache();
        return false;
    }
}

std::string StateHierarchyManager::getParent(const std::string &stateId) const {
    auto it = parentMap_.find(stateId);
    return (it != parentMap_.end()) ? it->second : "";
}

std::vector<std::string> StateHierarchyManager::getChildren(const std::string &stateId) const {
    auto it = childrenMap_.find(stateId);
    return (it != childrenMap_.end()) ? it->second : std::vector<std::string>();
}

std::vector<std::string> StateHierarchyManager::getAncestors(const std::string &stateId) const {
    auto it = ancestorsCache_.find(stateId);
    if (it != ancestorsCache_.end()) {
        return it->second;
    }

    // Compute and cache
    auto ancestors = computeAncestors(stateId);
    ancestorsCache_[stateId] = ancestors;
    return ancestors;
}

std::vector<std::string> StateHierarchyManager::getProperAncestors(const std::string &stateId) const {
    return getAncestors(stateId);  // getAncestors already excludes the state itself
}

std::vector<std::string> StateHierarchyManager::getDescendants(const std::string &stateId) const {
    auto it = descendantsCache_.find(stateId);
    if (it != descendantsCache_.end()) {
        return it->second;
    }

    // Compute and cache
    auto descendants = computeDescendants(stateId);
    descendantsCache_[stateId] = descendants;
    return descendants;
}

std::vector<std::string> StateHierarchyManager::getProperDescendants(const std::string &stateId) const {
    return getDescendants(stateId);  // getDescendants already excludes the state itself
}

std::string StateHierarchyManager::getLeastCommonAncestor(const std::vector<std::string> &stateIds) const {
    if (stateIds.empty()) {
        return "";
    }

    if (stateIds.size() == 1) {
        return stateIds[0];
    }

    // Get ancestors for first state
    auto commonAncestors = getAncestors(stateIds[0]);
    commonAncestors.push_back(stateIds[0]);  // Include the state itself

    // Intersect with ancestors of other states
    for (size_t i = 1; i < stateIds.size(); ++i) {
        auto ancestors = getAncestors(stateIds[i]);
        ancestors.push_back(stateIds[i]);  // Include the state itself

        // Keep only common ancestors
        std::vector<std::string> intersection;
        std::set_intersection(commonAncestors.begin(), commonAncestors.end(), ancestors.begin(), ancestors.end(),
                              std::back_inserter(intersection));

        commonAncestors = intersection;

        if (commonAncestors.empty()) {
            return "";
        }
    }

    // Return the deepest common ancestor (last in the list)
    return commonAncestors.empty() ? "" : commonAncestors.back();
}

bool StateHierarchyManager::isAncestor(const std::string &ancestorId, const std::string &descendantId) const {
    if (ancestorId == descendantId) {
        return false;  // A state is not its own ancestor
    }

    auto ancestors = getAncestors(descendantId);
    return std::find(ancestors.begin(), ancestors.end(), ancestorId) != ancestors.end();
}

bool StateHierarchyManager::isDescendant(const std::string &descendantId, const std::string &ancestorId) const {
    return isAncestor(ancestorId, descendantId);
}

int StateHierarchyManager::getDepth(const std::string &stateId) const {
    auto it = depthCache_.find(stateId);
    return (it != depthCache_.end()) ? it->second : -1;
}

std::vector<std::string> StateHierarchyManager::getPathFromRoot(const std::string &stateId) const {
    std::vector<std::string> path;

    if (!hasState(stateId)) {
        return path;
    }

    // Build path from state to root, then reverse
    std::string currentState = stateId;
    while (!currentState.empty()) {
        path.push_back(currentState);
        currentState = getParent(currentState);
    }

    // Reverse to get root-to-state path
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<std::string> StateHierarchyManager::getPathBetween(const std::string &fromStateId,
                                                               const std::string &toStateId) const {
    if (!hasState(fromStateId) || !hasState(toStateId)) {
        return {};
    }

    if (fromStateId == toStateId) {
        return {fromStateId};
    }

    // Find LCA
    std::string lca = getLeastCommonAncestor({fromStateId, toStateId});
    if (lca.empty()) {
        return {};
    }

    // Path from source to LCA (excluding LCA)
    std::vector<std::string> pathToLCA;
    std::string current = fromStateId;
    while (current != lca && !current.empty()) {
        pathToLCA.push_back(current);
        current = getParent(current);
    }

    // Path from LCA to target (excluding LCA, including target)
    std::vector<std::string> pathFromLCA;
    current = toStateId;
    while (current != lca && !current.empty()) {
        pathFromLCA.push_back(current);
        current = getParent(current);
    }

    // Reverse path from LCA
    std::reverse(pathFromLCA.begin(), pathFromLCA.end());

    // Combine paths
    std::vector<std::string> fullPath = pathToLCA;
    fullPath.push_back(lca);  // Add LCA
    fullPath.insert(fullPath.end(), pathFromLCA.begin(), pathFromLCA.end());

    return fullPath;
}

std::vector<std::string> StateHierarchyManager::getSiblings(const std::string &stateId) const {
    std::string parent = getParent(stateId);
    if (parent.empty()) {
        return {};  // Root state has no siblings
    }

    auto siblings = getChildren(parent);
    siblings.erase(std::remove(siblings.begin(), siblings.end(), stateId), siblings.end());
    return siblings;
}

bool StateHierarchyManager::hasState(const std::string &stateId) const {
    return stateSet_.contains(stateId);
}

std::string StateHierarchyManager::getRootStateId() const {
    return rootStateId_;
}

std::vector<std::string> StateHierarchyManager::getAllStateIds() const {
    return stateSet_.getSorted();
}

void StateHierarchyManager::clearCache() {
    parentMap_.clear();
    childrenMap_.clear();
    ancestorsCache_.clear();
    descendantsCache_.clear();
    depthCache_.clear();
    stateSet_.clear();
    rootStateId_.clear();
}

bool StateHierarchyManager::rebuildHierarchy() {
    return buildHierarchy();
}

// ========== Protected Methods ==========

void StateHierarchyManager::buildRelationships(std::shared_ptr<Model::IStateNode> state, const std::string &parentId) {
    if (!state) {
        return;
    }

    std::string stateId = state->getId();
    stateSet_.insert(stateId);

    // Set parent relationship
    if (!parentId.empty()) {
        parentMap_[stateId] = parentId;
        childrenMap_[parentId].push_back(stateId);
    }

    // Process children
    const auto &children = state->getChildren();
    for (const auto &child : children) {
        if (child) {
            buildRelationships(child, stateId);
        }
    }
}

void StateHierarchyManager::buildDepthCache() {
    // BFS to compute depths
    std::queue<std::pair<std::string, int>> queue;
    queue.push({rootStateId_, 0});

    while (!queue.empty()) {
        auto [stateId, depth] = queue.front();
        queue.pop();

        depthCache_[stateId] = depth;

        // Add children to queue
        auto children = getChildren(stateId);
        for (const auto &child : children) {
            queue.push({child, depth + 1});
        }
    }
}

bool StateHierarchyManager::validateHierarchy() const {
    // Check that every state (except root) has a parent
    for (const auto &stateId : stateSet_) {
        if (stateId != rootStateId_) {
            if (getParent(stateId).empty()) {
                return false;  // Non-root state without parent
            }
        }
    }

    // Check that parent-child relationships are consistent
    for (const auto &[parent, children] : childrenMap_) {
        for (const auto &child : children) {
            if (getParent(child) != parent) {
                return false;  // Inconsistent relationship
            }
        }
    }

    return true;
}

// ========== Private Methods ==========

std::vector<std::string> StateHierarchyManager::computeAncestors(const std::string &stateId) const {
    std::vector<std::string> ancestors;

    std::string current = getParent(stateId);
    while (!current.empty()) {
        ancestors.push_back(current);
        current = getParent(current);
    }

    return ancestors;
}

std::vector<std::string> StateHierarchyManager::computeDescendants(const std::string &stateId) const {
    std::vector<std::string> descendants;

    // BFS traversal
    std::queue<std::string> queue;
    auto children = getChildren(stateId);
    for (const auto &child : children) {
        queue.push(child);
        descendants.push_back(child);
    }

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();

        auto currentChildren = getChildren(current);
        for (const auto &child : currentChildren) {
            queue.push(child);
            descendants.push_back(child);
        }
    }

    return descendants;
}

// ========== Additional Methods for Test Compatibility ==========

std::string StateHierarchyManager::getCommonAncestor(const std::string &state1, const std::string &state2) const {
    return getLeastCommonAncestor({state1, state2});
}

bool StateHierarchyManager::isValidConfiguration(const std::vector<std::string> &configuration) const {
    if (configuration.empty()) {
        return false;
    }

    // Check that all states exist
    for (const auto &stateId : configuration) {
        if (!hasState(stateId)) {
            return false;
        }
    }

    // Check that configuration is properly nested
    // For each state in configuration, all its ancestors should also be in configuration
    for (const auto &stateId : configuration) {
        auto ancestors = getAncestors(stateId);
        for (const auto &ancestor : ancestors) {
            if (std::find(configuration.begin(), configuration.end(), ancestor) == configuration.end()) {
                return false;  // Ancestor not in configuration
            }
        }
    }

    return true;
}

std::vector<std::string> StateHierarchyManager::getDefaultConfiguration() const {
    std::vector<std::string> config;

    if (rootStateId_.empty()) {
        return config;
    }

    // Start with root
    config.push_back(rootStateId_);

    // For compound states, add their initial states recursively
    std::string current = rootStateId_;
    int maxDepth = 20;  // Prevent infinite loops in malformed state hierarchies
    int depth = 0;

    while (depth < maxDepth) {
        auto children = getChildren(current);
        if (children.empty()) {
            break;  // No children, atomic state
        }

        // For now, just pick the first child as initial state
        // In a real implementation, this should check the initial attribute
        std::string initialChild = children[0];

        // Check for circular references
        auto it = std::find(config.begin(), config.end(), initialChild);
        if (it != config.end()) {
            Logger::warn("StateHierarchyManager: Circular reference detected at state: " + initialChild);
            break;
        }

        config.push_back(initialChild);
        current = initialChild;
        depth++;
    }

    if (depth >= maxDepth) {
        Logger::warn("StateHierarchyManager: Maximum hierarchy depth reached, possible infinite loop");
    }

    return config;
}

std::vector<std::string>
StateHierarchyManager::getAtomicStatesInConfiguration(const std::vector<std::string> &config) const {
    std::vector<std::string> atomicStates;

    for (const auto &stateId : config) {
        auto children = getChildren(stateId);
        if (children.empty()) {
            // State has no children, it's atomic
            atomicStates.push_back(stateId);
        }
    }

    return atomicStates;
}

std::vector<std::string> StateHierarchyManager::getCompletionStates(const std::string &stateId) const {
    std::vector<std::string> completionStates;

    auto children = getChildren(stateId);
    if (children.empty()) {
        // Atomic state - completion is itself
        completionStates.push_back(stateId);
    } else {
        // Compound state - completion is its initial state(s)
        // For now, just return first child
        // In real implementation, should check initial attribute
        completionStates.push_back(children[0]);
    }

    return completionStates;
}

}  // namespace SCXML