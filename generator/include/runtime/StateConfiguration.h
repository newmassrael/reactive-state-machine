#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace SCXML {

namespace Model {
class DocumentModel;
class IStateNode;
}  // namespace Model

namespace Runtime {

/**
 * @brief State Configuration Management for SCXML Engine
 *
 * This class manages the current configuration of active states in an SCXML
 * state machine according to W3C SCXML 1.0 specification. It maintains the
 * set of states that are currently active and provides efficient operations
 * for state entry, exit, and queries.
 *
 * The configuration represents the set of states that are "active" at any
 * given time during the execution of the state machine.
 */
class StateConfiguration {
public:
    /**
     * @brief Constructor
     */
    StateConfiguration();

    /**
     * @brief Destructor
     */
    ~StateConfiguration() = default;

    // ====== Initialization ======

    /**
     * @brief Initialize with SCXML model
     * @param model Document model for state hierarchy
     * @return true if initialization succeeded
     */
    bool initialize(std::shared_ptr<Model::DocumentModel> model);

    /**
     * @brief Reset to empty configuration
     */
    void clear();

    // ====== State Management ======

    /**
     * @brief Add state to active configuration
     * @param stateId State identifier to add
     * @return true if state was added (false if already active)
     */
    bool addState(const std::string &stateId);

    /**
     * @brief Remove state from active configuration
     * @param stateId State identifier to remove
     * @return true if state was removed (false if not active)
     */
    bool removeState(const std::string &stateId);

    /**
     * @brief Add multiple states to configuration
     * @param stateIds Vector of state identifiers to add
     */
    void addStates(const std::vector<std::string> &stateIds);

    /**
     * @brief Remove multiple states from configuration
     * @param stateIds Vector of state identifiers to remove
     */
    void removeStates(const std::vector<std::string> &stateIds);

    // ====== State Queries ======

    /**
     * @brief Check if state is active
     * @param stateId State identifier to check
     * @return true if state is in active configuration
     */
    bool isActive(const std::string &stateId) const;

    /**
     * @brief Get all active states
     * @return Set of active state identifiers
     */
    const std::set<std::string> &getActiveStates() const;

    /**
     * @brief Get active states as vector (for ordered operations)
     * @return Vector of active state identifiers
     */
    std::vector<std::string> getActiveStatesVector() const;

    /**
     * @brief Check if configuration is empty
     * @return true if no states are active
     */
    bool isEmpty() const;

    /**
     * @brief Get number of active states
     * @return Count of active states
     */
    size_t size() const;

    // ====== Hierarchy Operations ======

    /**
     * @brief Get active atomic states (leaf states with no children)
     * @return Vector of atomic state identifiers
     */
    std::vector<std::string> getActiveAtomicStates() const;

    /**
     * @brief Get active compound states (states with children)
     * @return Vector of compound state identifiers
     */
    std::vector<std::string> getActiveCompoundStates() const;

    /**
     * @brief Check if all ancestors of a state are active
     * @param stateId State to check ancestors for
     * @return true if all proper ancestors are active
     */
    bool areAncestorsActive(const std::string &stateId) const;

    /**
     * @brief Get active states in document order
     * @return Vector of state IDs sorted by document order
     */
    std::vector<std::string> getStatesInDocumentOrder() const;

    /**
     * @brief Get active states in reverse document order
     * @return Vector of state IDs sorted in reverse document order
     */
    std::vector<std::string> getStatesInReverseDocumentOrder() const;

    // ====== Final State Checking ======

    /**
     * @brief Check if configuration includes any final states
     * @return true if any final states are active
     */
    bool hasActiveFinalStates() const;

    /**
     * @brief Check if state machine is in a final configuration
     * @return true if only final states are active and machine should stop
     */
    bool isInFinalConfiguration() const;

    /**
     * @brief Get all active final states
     * @return Vector of active final state identifiers
     */
    std::vector<std::string> getActiveFinalStates() const;

    // ====== Validation and Debugging ======

    /**
     * @brief Validate configuration consistency
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate() const;

    /**
     * @brief Get configuration as string for debugging
     * @return String representation of active states
     */
    std::string toString() const;

    /**
     * @brief Check configuration equality
     * @param other Configuration to compare with
     * @return true if configurations are identical
     */
    bool equals(const StateConfiguration &other) const;

private:
    // Core data
    std::set<std::string> activeStates_;
    std::shared_ptr<Model::DocumentModel> model_;

    // Helper methods
    bool isAtomicState(const std::string &stateId) const;
    bool isCompoundState(const std::string &stateId) const;
    bool isFinalState(const std::string &stateId) const;
    std::vector<std::string> getAncestors(const std::string &stateId) const;
    std::shared_ptr<Model::IStateNode> getStateNode(const std::string &stateId) const;
    int getDocumentOrder(const std::string &stateId) const;

    // Validation helpers
    bool isValidConfiguration() const;
    std::vector<std::string> findOrphanedStates() const;
    std::vector<std::string> findConflictingStates() const;
};

}  // namespace Runtime
}  // namespace SCXML