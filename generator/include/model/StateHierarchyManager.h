#pragma once

#include "common/PerformanceUtils.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
namespace SCXML {
// Forward declarations
namespace Model {
class DocumentModel;
}

namespace Model {
class IStateNode;
}

/**
 * @brief State Hierarchy Manager for SCXML
 *
 * This class provides utilities for navigating and querying the state hierarchy
 * in SCXML documents. It maintains cached relationships for efficient traversal.
 */
class StateHierarchyManager {
public:
    /**
     * @brief Construct a new State Hierarchy Manager
     * @param model SCXML model to manage hierarchy for
     */
    explicit StateHierarchyManager(std::shared_ptr<::DocumentModel> model);

    /**
     * @brief Destructor
     */
    ~StateHierarchyManager() = default;

    /**
     * @brief Build hierarchy cache from model
     * @return true if hierarchy was built successfully
     */
    bool buildHierarchy();

    /**
     * @brief Get parent state of a given state
     * @param stateId State ID to find parent for
     * @return Parent state ID, or empty if root state
     */
    std::string getParent(const std::string &stateId) const;

    /**
     * @brief Get all child states of a given state
     * @param stateId State ID to find children for
     * @return Vector of child state IDs
     */
    std::vector<std::string> getChildren(const std::string &stateId) const;

    /**
     * @brief Get all ancestors of a state (from parent to root)
     * @param stateId State ID to find ancestors for
     * @return Vector of ancestor state IDs (parent to root order)
     */
    std::vector<std::string> getAncestors(const std::string &stateId) const;

    /**
     * @brief Get proper ancestors of a state (excluding the state itself)
     * @param stateId State ID to find ancestors for
     * @return Vector of proper ancestor state IDs
     */
    std::vector<std::string> getProperAncestors(const std::string &stateId) const;

    /**
     * @brief Get all descendants of a state (recursive)
     * @param stateId State ID to find descendants for
     * @return Vector of descendant state IDs
     */
    std::vector<std::string> getDescendants(const std::string &stateId) const;

    /**
     * @brief Get proper descendants of a state (excluding the state itself)
     * @param stateId State ID to find descendants for
     * @return Vector of proper descendant state IDs
     */
    std::vector<std::string> getProperDescendants(const std::string &stateId) const;

    /**
     * @brief Find least common ancestor of two or more states
     * @param stateIds Vector of state IDs
     * @return Least common ancestor state ID, or empty if no common ancestor
     */
    std::string getLeastCommonAncestor(const std::vector<std::string> &stateIds) const;

    /**
     * @brief Check if one state is ancestor of another
     * @param ancestorId Potential ancestor state ID
     * @param descendantId Potential descendant state ID
     * @return true if ancestorId is ancestor of descendantId
     */
    bool isAncestor(const std::string &ancestorId, const std::string &descendantId) const;

    /**
     * @brief Check if one state is descendant of another
     * @param descendantId Potential descendant state ID
     * @param ancestorId Potential ancestor state ID
     * @return true if descendantId is descendant of ancestorId
     */
    bool isDescendant(const std::string &descendantId, const std::string &ancestorId) const;

    /**
     * @brief Get depth of state in hierarchy (root = 0)
     * @param stateId State ID to find depth for
     * @return Depth level, -1 if state not found
     */
    int getDepth(const std::string &stateId) const;

    /**
     * @brief Get path from root to state
     * @param stateId Target state ID
     * @return Vector of state IDs from root to target (inclusive)
     */
    std::vector<std::string> getPathFromRoot(const std::string &stateId) const;

    /**
     * @brief Get path between two states
     * @param fromStateId Source state ID
     * @param toStateId Target state ID
     * @return Vector of state IDs forming path from source to target
     */
    std::vector<std::string> getPathBetween(const std::string &fromStateId, const std::string &toStateId) const;

    /**
     * @brief Get all sibling states of a given state
     * @param stateId State ID to find siblings for
     * @return Vector of sibling state IDs (excluding the state itself)
     */
    std::vector<std::string> getSiblings(const std::string &stateId) const;

    /**
     * @brief Check if state exists in hierarchy
     * @param stateId State ID to check
     * @return true if state exists
     */
    bool hasState(const std::string &stateId) const;

    /**
     * @brief Get root state ID
     * @return Root state ID
     */
    std::string getRootStateId() const;

    /**
     * @brief Get all state IDs in hierarchy
     * @return Vector of all state IDs
     */
    std::vector<std::string> getAllStateIds() const;

    /**
     * @brief Clear hierarchy cache
     */
    void clearCache();

    /**
     * @brief Rebuild hierarchy cache
     * @return true if rebuild was successful
     */
    bool rebuildHierarchy();

    // ========== Additional Methods for Test Compatibility ==========

    /**
     * @brief Get common ancestor of two states (convenience method)
     * @param state1 First state ID
     * @param state2 Second state ID
     * @return Common ancestor state ID, or empty if no common ancestor
     */
    std::string getCommonAncestor(const std::string &state1, const std::string &state2) const;

    /**
     * @brief Check if configuration is valid (proper nesting)
     * @param configuration Vector of state IDs representing configuration
     * @return true if configuration is valid
     */
    bool isValidConfiguration(const std::vector<std::string> &configuration) const;

    /**
     * @brief Get default initial configuration starting from root
     * @return Vector of state IDs representing default configuration
     */
    std::vector<std::string> getDefaultConfiguration() const;

    /**
     * @brief Get atomic states from a configuration
     * @param config Configuration vector
     * @return Vector of atomic state IDs in the configuration
     */
    std::vector<std::string> getAtomicStatesInConfiguration(const std::vector<std::string> &config) const;

    /**
     * @brief Get completion states for a given state
     * @param stateId State ID to get completion for
     * @return Vector of states that complete the given state
     */
    std::vector<std::string> getCompletionStates(const std::string &stateId) const;

protected:
    /**
     * @brief Build parent-child relationships recursively
     * @param state State node to process
     * @param parentId Parent state ID (empty for root)
     */
    void buildRelationships(std::shared_ptr<IStateNode> state, const std::string &parentId);

    /**
     * @brief Build depth cache for all states
     */
    void buildDepthCache();

    /**
     * @brief Validate hierarchy consistency
     * @return true if hierarchy is consistent
     */
    bool validateHierarchy() const;

private:
    // SCXML model
    std::shared_ptr<::DocumentModel> model_;

    // Hierarchy relationships
    std::unordered_map<std::string, std::string> parentMap_;                 // child -> parent
    std::unordered_map<std::string, std::vector<std::string>> childrenMap_;  // parent -> children

    // Cached data for performance
    mutable std::unordered_map<std::string, std::vector<std::string>> ancestorsCache_;
    mutable std::unordered_map<std::string, std::vector<std::string>> descendantsCache_;
    std::unordered_map<std::string, int> depthCache_;

    // Root state
    std::string rootStateId_;

    // State existence set for fast lookup
    SCXML::Common::FastStateSet stateSet_;

    /**
     * @brief Compute ancestors for a state (internal)
     * @param stateId State ID
     * @return Vector of ancestor state IDs
     */
    std::vector<std::string> computeAncestors(const std::string &stateId) const;

    /**
     * @brief Compute descendants for a state (internal)
     * @param stateId State ID
     * @return Vector of descendant state IDs
     */
    std::vector<std::string> computeDescendants(const std::string &stateId) const;
};

}  // namespace SCXML