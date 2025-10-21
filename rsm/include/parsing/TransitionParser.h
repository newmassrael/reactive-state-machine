#pragma once

#include "factory/NodeFactory.h"
#include "model/IStateNode.h"
#include "model/ITransitionNode.h"
#include "parsing/ActionParser.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Class responsible for parsing transition elements
 *
 * This class provides functionality to parse transition-related elements
 * in SCXML documents. It builds transition relationships between states
 * and handles actions and guard conditions to be executed during transitions.
 */

namespace RSM {

class TransitionParser {
public:
    /**
     * @brief Constructor
     * @param nodeFactory Factory instance for node creation
     */
    explicit TransitionParser(std::shared_ptr<NodeFactory> nodeFactory);

    /**
     * @brief Destructor
     */
    ~TransitionParser();

    /**
     * @brief Set action parser
     * @param actionParser Parser for action parsing
     */
    void setActionParser(std::shared_ptr<ActionParser> actionParser);

    /**
     * @brief Parse transition node
     * @param transElement XML transition element
     * @param stateNode Owner state node
     * @return Created transition node
     */
    std::shared_ptr<ITransitionNode> parseTransitionNode(const xmlpp::Element *transElement, IStateNode *stateNode);

    /**
     * @brief Parse initial transition
     * @param initialElement XML initial element
     * @return Created transition node
     */
    std::shared_ptr<ITransitionNode> parseInitialTransition(const xmlpp::Element *initialElement);

    /**
     * @brief Parse all transitions within a state
     * @param stateElement State element
     * @param stateNode State node
     * @return List of parsed transition nodes
     */
    std::vector<std::shared_ptr<ITransitionNode>> parseTransitionsInState(const xmlpp::Element *stateElement,
                                                                          IStateNode *stateNode);

    /**
     * @brief Check if element is a transition node
     * @param element XML element
     * @return Whether it is a transition node
     */
    bool isTransitionNode(const xmlpp::Element *element) const;

private:
    /**
     * @brief Parse transition actions
     * @param transElement Transition element
     * @param transition Transition node
     */
    void parseActions(const xmlpp::Element *transElement, std::shared_ptr<ITransitionNode> transition);

    /**
     * @brief Parse event list
     * @param eventStr Event string (space-separated list)
     * @return Individual event list
     */
    std::vector<std::string> parseEventList(const std::string &eventStr) const;

    /**
     * @brief Handle namespace matching
     * @param nodeName Node name
     * @param searchName Name to search for
     * @return Whether node name matches search name
     */
    bool matchNodeName(const std::string &nodeName, const std::string &searchName) const;

    std::vector<std::string> parseTargetList(const std::string &targetStr) const;

    std::shared_ptr<NodeFactory> nodeFactory_;
    std::shared_ptr<ActionParser> actionParser_;
};

}  // namespace RSM