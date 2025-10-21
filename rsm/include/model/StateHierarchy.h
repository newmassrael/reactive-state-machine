#pragma once

#include "model/IStateNode.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief State hierarchy management class
 *
 * This class manages hierarchical relationships between state nodes
 * and provides hierarchy navigation and manipulation functionality.
 */

namespace RSM {

class StateHierarchy {
public:
    /**
     * @brief Constructor
     */
    StateHierarchy();

    /**
     * @brief Destructor
     */
    ~StateHierarchy();

    /**
     * @brief Set root state
     * @param rootState Root state node
     */
    void setRootState(std::shared_ptr<IStateNode> rootState);

    /**
     * @brief Return root state
     * @return Root state node
     */
    IStateNode *getRootState() const;

    /**
     * @brief Add state
     * @param state State node
     * @param parentId Parent state ID (if empty, added as root child)
     * @return Success status
     */
    bool addState(std::shared_ptr<IStateNode> state, const std::string &parentId = "");

    /**
     * @brief Find state by ID
     * @param id State ID
     * @return State node pointer, nullptr if not found
     */
    IStateNode *findStateById(const std::string &id) const;

    /**
     * @brief Check relationship between two states
     * @param ancestorId Ancestor state ID
     * @param descendantId Descendant state ID
     * @return Whether descendantId is a descendant of ancestorId
     */
    bool isDescendantOf(const std::string &ancestorId, const std::string &descendantId) const;

    /**
     * @brief Return list of all states
     * @return List of all state nodes
     */
    const std::vector<std::shared_ptr<IStateNode>> &getAllStates() const;

    /**
     * @brief Validate state relationships
     * @return Whether all relationships are valid
     */
    bool validateRelationships() const;

    /**
     * @brief Find missing state IDs
     * @return List of missing state IDs
     */
    std::vector<std::string> findMissingStateIds() const;

    /**
     * @brief Print state hierarchy (for debugging)
     */
    void printHierarchy() const;

private:
    /**
     * @brief Print state hierarchy (internal use)
     * @param state State node
     * @param depth Depth
     */
    void printStateHierarchy(IStateNode *state, int depth) const;

    /**
     * @brief Check relationship between two states (internal use)
     * @param ancestor Ancestor state
     * @param descendant Descendant state
     * @return Whether descendant is a descendant of ancestor
     */
    bool isDescendantOf(IStateNode *ancestor, IStateNode *descendant) const;

    // Member variables
    std::shared_ptr<IStateNode> rootState_;
    std::vector<std::shared_ptr<IStateNode>> allStates_;
    std::unordered_map<std::string, IStateNode *> stateIdMap_;
};

}  // namespace RSM