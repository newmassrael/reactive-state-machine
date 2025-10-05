#pragma once

#include "actions/IActionNode.h"
#include "model/IDataModelItem.h"
#include "model/IGuardNode.h"
#include "model/IStateNode.h"
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Object model representation of SCXML document
 */

namespace RSM {

class SCXMLModel {
public:
    SCXMLModel();
    ~SCXMLModel();

    /**
     * @brief Set root state node
     * @param rootState Root state node
     */
    void setRootState(std::shared_ptr<IStateNode> rootState);

    /**
     * @brief Get root state node
     * @return Root state node
     */
    std::shared_ptr<IStateNode> getRootState() const;

    /**
     * @brief Set SCXML document name
     * @param name Document name
     */
    void setName(const std::string &name);

    /**
     * @brief Get SCXML document name
     * @return Document name
     */
    const std::string &getName() const;

    /**
     * @brief Set initial state ID(s) - supports space-separated multiple states for parallel regions
     *
     * W3C SCXML 3.3: The 'initial' attribute can contain space-separated state IDs
     * for parallel regions. This method parses the input string and stores all IDs.
     *
     * @param initialState Single state ID or space-separated list (e.g., "s1 s2 s3")
     *
     * @example
     *   setInitialState("s1");         // Single initial state
     *   setInitialState("s1 s2 s3");   // Multiple initial states for parallel
     *   setInitialState("");           // Clear initial states (empty vector)
     */
    void setInitialState(const std::string &initialState);

    /**
     * @brief Get initial state IDs (W3C SCXML 3.3 compliant)
     *
     * Returns all initial state IDs parsed from 'initial' attribute.
     * For parallel regions, this vector contains multiple state IDs.
     *
     * @return Vector of initial state IDs (may be empty)
     */
    const std::vector<std::string> &getInitialStates() const;

    /**
     * @brief Get first initial state ID (backward compatibility only)
     *
     * @deprecated Use getInitialStates() for W3C SCXML 3.3 compliance
     * @return First initial state ID, or empty string if no initial states
     * @warning Only returns the first element. For parallel regions with multiple
     *          initial states, use getInitialStates() to avoid data loss.
     */
    std::string getInitialState() const;

    /**
     * @brief Set data model type
     * @param datamodel Data model type
     */
    void setDatamodel(const std::string &datamodel);

    /**
     * @brief Get data model type
     * @return Data model type
     */
    const std::string &getDatamodel() const;

    /**
     * @brief Add context property
     * @param name Property name
     * @param type Property type
     */
    void addContextProperty(const std::string &name, const std::string &type);

    /**
     * @brief Get context properties
     * @return Context properties map
     */
    const std::unordered_map<std::string, std::string> &getContextProperties() const;

    /**
     * @brief Add dependency injection point
     * @param name Injection point name
     * @param type Injection point type
     */
    void addInjectPoint(const std::string &name, const std::string &type);

    /**
     * @brief Get dependency injection points
     * @return Dependency injection points map
     */
    const std::unordered_map<std::string, std::string> &getInjectPoints() const;

    /**
     * @brief Add guard condition
     * @param guard Guard condition node
     */
    void addGuard(std::shared_ptr<IGuardNode> guard);

    /**
     * @brief Get guard conditions
     * @return Guard conditions vector
     */
    const std::vector<std::shared_ptr<IGuardNode>> &getGuards() const;

    /**
     * @brief Add state node
     * @param state State node
     */
    void addState(std::shared_ptr<IStateNode> state);

    /**
     * @brief Get all state nodes
     * @return State nodes vector
     */
    const std::vector<std::shared_ptr<IStateNode>> &getAllStates() const;

    /**
     * @brief Find state node by ID
     * @param id State ID
     * @return State node pointer, nullptr if not found
     */
    IStateNode *findStateById(const std::string &id) const;

    /**
     * @brief Add data model item
     * @param dataItem Data model item
     */
    void addDataModelItem(std::shared_ptr<IDataModelItem> dataItem);

    /**
     * @brief Get data model items
     * @return Data model items vector
     */
    const std::vector<std::shared_ptr<IDataModelItem>> &getDataModelItems() const;

    /**
     * @brief Validate state relationships
     * @return Whether all relationships are valid
     */
    bool validateStateRelationships() const;

    /**
     * @brief Find missing state IDs
     * @return List of missing state IDs
     */
    std::vector<std::string> findMissingStateIds() const;

    /**
     * @brief Print model structure (for debugging)
     */
    void printModelStructure() const;

    /**
     * @brief Get data model variable names
     * @return Set of data model variable names
     */
    std::set<std::string> getDataModelVariableNames() const;

    /**
     * @brief Set binding mode
     * @param binding Binding mode ("early" or "late")
     */
    void setBinding(const std::string &binding);

    /**
     * @brief Get binding mode
     * @return Binding mode
     */
    const std::string &getBinding() const;

    /**
     * @brief Add system variable
     * @param systemVar System variable data model item
     */
    void addSystemVariable(std::shared_ptr<IDataModelItem> systemVar);

    /**
     * @brief Get system variables
     * @return System variables vector
     */
    const std::vector<std::shared_ptr<IDataModelItem>> &getSystemVariables() const;

    /**
     * @brief Add top-level script (W3C SCXML 5.8)
     * @param script Script action node
     */
    void addTopLevelScript(std::shared_ptr<IActionNode> script);

    /**
     * @brief Get top-level scripts (W3C SCXML 5.8)
     * @return Top-level scripts vector
     */
    const std::vector<std::shared_ptr<IActionNode>> &getTopLevelScripts() const;

private:
    /**
     * @brief Find state node recursively by state ID
     * @param state State node to start search from
     * @param id State ID to find
     * @return State node pointer, nullptr if not found
     */
    IStateNode *findStateByIdRecursive(IStateNode *state, const std::string &id,
                                       std::set<std::string> &visitedStates) const;

    /**
     * @brief Print state hierarchy (for debugging)
     * @param state State node
     * @param depth Depth
     */
    void printStateHierarchy(IStateNode *state, int depth) const;

    /**
     * @brief Collect all states recursively from state hierarchy
     * @param state Starting state node
     * @param allStates Vector to store all states
     */
    void collectAllStatesRecursively(IStateNode *state, std::vector<std::shared_ptr<IStateNode>> &allStates) const;

    /**
     * @brief Rebuild all states list (including all nested states from root)
     */
    void rebuildAllStatesList();

    // Member variables
    std::shared_ptr<IStateNode> rootState_;
    std::string name_;
    std::vector<std::string> initialStates_;  // W3C SCXML 3.3: supports multiple initial states
    std::string datamodel_;
    std::unordered_map<std::string, std::string> contextProperties_;
    std::unordered_map<std::string, std::string> injectPoints_;
    std::vector<std::shared_ptr<IGuardNode>> guards_;
    std::vector<std::shared_ptr<IStateNode>> allStates_;
    std::unordered_map<std::string, IStateNode *> stateIdMap_;
    std::vector<std::shared_ptr<IDataModelItem>> dataModelItems_;
    std::string binding_;
    std::vector<std::shared_ptr<IDataModelItem>> systemVariables_;
    std::vector<std::shared_ptr<IActionNode>> topLevelScripts_;  // W3C SCXML 5.8
};

}  // namespace RSM