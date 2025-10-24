#pragma once

#include "actions/IActionNode.h"
#include "factory/NodeFactory.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Class responsible for parsing action elements
 *
 * This class provides functionality to parse action-related elements
 * in SCXML documents. It handles both regular actions (<code:action>)
 * and external execution actions (<code:external-action>).
 */

namespace RSM {

class ActionParser {
public:
    /**
     * @brief Constructor
     * @param nodeFactory Factory instance for node creation
     */
    explicit ActionParser(std::shared_ptr<NodeFactory> nodeFactory);

    /**
     * @brief Destructor
     */
    ~ActionParser();

    /**
     * @brief Parse action node
     * @param actionNode XML action node
     * @return Created action node
     */
    std::shared_ptr<RSM::IActionNode> parseActionNode(const xmlpp::Element *actionNode);

    /**
     * @brief Parse external execution action node
     * @param externalActionNode XML external execution action node
     * @return Created action node
     */
    std::shared_ptr<RSM::IActionNode> parseExternalActionNode(const xmlpp::Element *externalActionNode);

    /**
     * @brief Parse actions within onentry/onexit elements
     * @param parentElement Parent element (onentry or onexit)
     * @return List of parsed actions
     */
    std::vector<std::shared_ptr<RSM::IActionNode>> parseActionsInElement(const xmlpp::Element *parentElement);

    /**
     * @brief Check if element is an action node
     * @param element XML element
     * @return Whether it is an action node
     */
    bool isActionNode(const xmlpp::Element *element) const;

    /**
     * @brief Check if element is an external execution action node
     * @param element XML element
     * @return Whether it is an external execution action node
     */
    bool isExternalActionNode(const xmlpp::Element *element) const;

    /**
     * @brief Set SCXML file base path for external script loading
     * @param basePath Base directory path of the SCXML file
     *
     * W3C SCXML 5.8: External scripts (src attribute) are resolved
     * relative to the SCXML file location
     */
    void setScxmlBasePath(const std::string &basePath);

    /**
     * @brief Check if element is executable content requiring special processing
     * @param element XML element
     * @return Whether it is special executable content
     */
    bool isSpecialExecutableContent(const xmlpp::Element *element) const;

private:
    /**
     * @brief Parse external implementation element
     * @param element XML element
     * @param actionNode Action node
     */
    void parseExternalImplementation(const xmlpp::Element *element, std::shared_ptr<RSM::IActionNode> actionNode);

    /**
     * @brief Parse special executable content
     * @param element XML element
     * @param actions List of parsed actions (modified)
     */
    void parseSpecialExecutableContent(const xmlpp::Element *element,
                                       std::vector<std::shared_ptr<RSM::IActionNode>> &actions);

    /**
     * @brief Handle namespace matching
     * @param nodeName Node name
     * @param searchName Name to search for
     * @return Whether node name matches search name (considering namespace)
     */
    bool matchNodeName(const std::string &nodeName, const std::string &searchName) const;

    /**
     * @brief Extract local name from node name (remove namespace)
     * @param nodeName Node name
     * @return Local node name
     */
    std::string getLocalName(const std::string &nodeName) const;

    std::shared_ptr<NodeFactory> nodeFactory_;

    /**
     * @brief Base directory path of the SCXML file (for resolving external script src attributes)
     * W3C SCXML 5.8: External scripts are resolved relative to SCXML file location
     */
    std::string scxmlBasePath_;
};

}  // namespace RSM