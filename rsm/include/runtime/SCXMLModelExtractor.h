#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace RSM {

class SCXMLModel;
class IStateNode;
class ITransitionNode;

/**
 * @brief Extracts state machine information from SCXML model
 *
 * This class bridges the gap between the parsed SCXML model
 * and the runtime state machine, extracting the essential
 * information needed for state machine execution.
 */
class SCXMLModelExtractor {
public:
    /**
     * @brief State information extracted from SCXML
     */
    struct StateInfo {
        std::string id;
        std::string fullPath;
        std::vector<std::string> onEntry;  // Entry action IDs
        std::vector<std::string> onExit;   // Exit action IDs
        bool isFinal = false;
        bool isInitial = false;
        std::string parent;  // For hierarchical states
    };

    /**
     * @brief Transition information extracted from SCXML
     */
    struct TransitionInfo {
        std::string fromState;
        std::string toState;
        std::string event;
        std::string condition;          // Guard condition (JavaScript)
        std::string action;             // Transition action (JavaScript)
        std::string type = "external";  // Transition type
        int priority = 0;               // For conflict resolution
    };

    /**
     * @brief Data model item from SCXML
     */
    struct DataItem {
        std::string id;
        std::string type;
        std::string initialValue;
        std::string expression;
    };

    /**
     * @brief Complete extracted state machine information
     */
    struct ExtractedModel {
        std::string initialState;
        std::vector<StateInfo> states;
        std::vector<TransitionInfo> transitions;
        std::vector<DataItem> dataItems;
        std::string name;
        std::string version;
    };

    /**
     * @brief Extract state machine model from SCXML
     * @param model Parsed SCXML model
     * @return Extracted model information
     */
    static ExtractedModel extractModel(std::shared_ptr<SCXMLModel> model);

private:
    /**
     * @brief Extract state information from state node
     * @param stateNode SCXML state node
     * @param parentId Parent state ID (for hierarchy)
     * @return State information
     */
    static StateInfo extractState(std::shared_ptr<IStateNode> stateNode, const std::string &parentId = "");

    /**
     * @brief Extract transition information from transition node
     * @param transitionNode SCXML transition node
     * @param fromStateId Source state ID
     * @return Transition information
     */
    static TransitionInfo extractTransition(std::shared_ptr<ITransitionNode> transitionNode,
                                            const std::string &fromStateId);

    /**
     * @brief Extract script content from action nodes
     * @param actionIds Vector of action IDs
     * @return Combined JavaScript code
     */
    static std::string extractScriptFromActions(const std::vector<std::string> &actionIds);

    /**
     * @brief Recursively extract states and transitions
     * @param stateNode Current state node
     * @param parentId Parent state ID
     * @param extracted Output structure to fill
     */
    static void extractRecursively(std::shared_ptr<IStateNode> stateNode, const std::string &parentId,
                                   ExtractedModel &extracted);

    /**
     * @brief Normalize and validate extracted model
     * @param extracted Model to validate
     * @return true if valid
     */
    static bool validateExtractedModel(ExtractedModel &extracted);
};

}  // namespace RSM