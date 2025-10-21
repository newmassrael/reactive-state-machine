#pragma once

#include "factory/NodeFactory.h"
#include "model/IGuardNode.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Class responsible for parsing guard conditions
 *
 * This class provides functionality to parse guard condition-related elements
 * in SCXML documents. It handles code:guard elements within code:guards elements
 * and guard attributes of transition elements.
 */

namespace RSM {

class GuardParser {
public:
    /**
     * @brief Constructor
     * @param nodeFactory Factory instance for node creation
     */
    explicit GuardParser(std::shared_ptr<NodeFactory> nodeFactory);

    /**
     * @brief Destructor
     */
    ~GuardParser();

    /**
     * @brief Parse guard node
     * @param guardNode XML guard node
     * @return Created guard node
     */
    std::shared_ptr<IGuardNode> parseGuardNode(const xmlpp::Element *guardNode);

    /**
     * @brief Parse guard attribute from transition
     * @param transitionNode XML transition node
     * @param targetState Transition target state
     * @return Created guard node, nullptr if no guard attribute
     */
    std::shared_ptr<IGuardNode> parseGuardFromTransition(const xmlpp::Element *transitionNode,
                                                         const std::string &targetState);

    /**
     * @brief Parse reactive guard
     * @param reactiveGuardNode XML reactive guard node
     * @return Created guard node
     */
    std::shared_ptr<IGuardNode> parseReactiveGuard(const xmlpp::Element *reactiveGuardNode);

    /**
     * @brief Parse all guards within guards element
     * @param guardsNode code:guards element
     * @return List of parsed guard nodes
     */
    std::vector<std::shared_ptr<IGuardNode>> parseGuardsElement(const xmlpp::Element *guardsNode);

    /**
     * @brief Parse all guards in SCXML document
     * @param scxmlNode SCXML root node
     * @return List of parsed guard nodes
     */
    std::vector<std::shared_ptr<IGuardNode>> parseAllGuards(const xmlpp::Element *scxmlNode);

    /**
     * @brief Check if element is a guard node
     * @param element XML element
     * @return Whether it is a guard node
     */
    bool isGuardNode(const xmlpp::Element *element) const;

    /**
     * @brief Check if element is a reactive guard node
     * @param element XML element
     * @return Whether it is a reactive guard node
     */
    bool isReactiveGuardNode(const xmlpp::Element *element) const;

private:
    /**
     * @brief Parse dependency list
     * @param guardNode Guard node
     * @param guardObject Guard object
     */
    void parseDependencies(const xmlpp::Element *guardNode, std::shared_ptr<IGuardNode> guardObject);

    /**
     * @brief Parse external implementation element
     * @param guardNode Guard node
     * @param guardObject Guard object
     */
    void parseExternalImplementation(const xmlpp::Element *guardNode, std::shared_ptr<IGuardNode> guardObject);

    /**
     * @brief Handle namespace matching
     * @param nodeName Node name
     * @param searchName Name to search for
     * @return Whether node name matches search name (considering namespace)
     */
    bool matchNodeName(const std::string &nodeName, const std::string &searchName) const;

    /**
     * @brief Separate condition expression and state
     * @param guardNode Guard node
     * @param guardObject Guard object
     * @param target XML target attribute value
     */
    void parseTargetAndCondition(const xmlpp::Element *guardNode, std::shared_ptr<IGuardNode> guardObject,
                                 const std::string &target);

    std::shared_ptr<NodeFactory> nodeFactory_;
};

}  // namespace RSM