#pragma once

#include "model/IActionNode.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>
#include <vector>

using SCXML::Model::IActionNode;


// Forward declarations
namespace SCXML {

namespace Model {
class IActionNode;
}

// Forward declarations
namespace Runtime {
class RuntimeContext;
}

namespace Core {
class IfActionNode;
}

namespace Parsing {

/**
 * @brief SCXML standard action parser for executable content
 *
 * This parser handles W3C SCXML standard executable content elements:
 * - <if>/<elseif>/<else> - Conditional execution
 * - <send> - Event sending
 * - <raise> - Internal event raising
 * - <assign> - Variable assignment
 * - <log> - Debug logging
 * - <cancel> - Cancel delayed events
 * - <script> - Script execution
 * - <foreach> - Iteration
 */
class ExecutableContentParser {
public:
    /**
     * @brief Constructor
     */
    ExecutableContentParser();

    /**
     * @brief Destructor
     */
    ~ExecutableContentParser() = default;

    /**
     * @brief Parse executable content from XML element
     * @param element XML element containing executable content
     * @return Vector of parsed action nodes
     */
    ::std::vector<::std::shared_ptr<::SCXML::Model::IActionNode>> parseExecutableContent(const xmlpp::Element *element);

    /**
     * @brief Parse a single SCXML action element
     * @param element XML element representing an SCXML action
     * @return Parsed action node, or nullptr if unsupported
     */
    ::std::shared_ptr<::SCXML::Model::IActionNode> parseAction(const xmlpp::Element *element);

    /**
     * @brief Check if element represents a supported SCXML action
     * @param element XML element to check
     * @return true if element is a supported SCXML action
     */
    bool isSupportedAction(const xmlpp::Element *element) const;

protected:
    /**
     * @brief Parse <if> conditional action
     * @param ifElement <if> XML element
     * @return IfActionNode with parsed conditions and actions
     */
    ::std::shared_ptr<::SCXML::Model::IActionNode> parseIfAction(const xmlpp::Element *ifElement);

    /**
     * @brief Parse <send> action
     * @param sendElement <send> XML element
     * @return SendActionNode with parsed parameters
     */
    ::std::shared_ptr<::SCXML::Model::IActionNode> parseSendAction(const xmlpp::Element *sendElement);

    /**
     * @brief Parse <raise> action
     * @param raiseElement <raise> XML element
     * @return RaiseActionNode with parsed event
     */
    ::std::shared_ptr<::SCXML::Model::IActionNode> parseRaiseAction(const xmlpp::Element *raiseElement);

    /**
     * @brief Parse <assign> action
     * @param assignElement <assign> XML element
     * @return AssignActionNode with parsed location and expression
     */
    ::std::shared_ptr<::SCXML::Model::IActionNode> parseAssignAction(const xmlpp::Element *assignElement);

    /**
     * @brief Parse <log> action
     * @param logElement <log> XML element
     * @return LogActionNode with parsed message and label
     */
    ::std::shared_ptr<::SCXML::Model::IActionNode> parseLogAction(const xmlpp::Element *logElement);

    /**
     * @brief Parse <cancel> action
     * @param cancelElement <cancel> XML element
     * @return CancelActionNode with parsed send ID
     */
    ::std::shared_ptr<::SCXML::Model::IActionNode> parseCancelAction(const xmlpp::Element *cancelElement);

    /**
     * @brief Parse <script> action
     * @param scriptElement <script> XML element
     * @return ScriptActionNode with parsed script content
     */
    ::std::shared_ptr<::SCXML::Model::IActionNode> parseScriptAction(const xmlpp::Element *scriptElement);

    /**
     * @brief Parse <foreach> action
     * @param foreachElement <foreach> XML element
     * @return ForeachActionNode with parsed iteration parameters
     */
    ::std::shared_ptr<::SCXML::Model::IActionNode> parseForeachAction(const xmlpp::Element *foreachElement);

private:
    /**
     * @brief Get attribute value with default
     * @param element XML element
     * @param attributeName Attribute name
     * @param defaultValue Default value if attribute not found
     * @return Attribute value or default
     */
    ::std::string getAttributeValue(const xmlpp::Element *element, const ::std::string &attributeName,
                                    const ::std::string &defaultValue = "") const;

    /**
     * @brief Get element text content
     * @param element XML element
     * @return Combined text content of element
     */
    ::std::string getTextContent(const xmlpp::Element *element) const;

    /**
     * @brief Get child elements with specific name
     * @param parent Parent element
     * @param childName Name of child elements to find
     * @return Vector of child elements with matching name
     */
    ::std::vector<const xmlpp::Element *> getChildElements(const xmlpp::Element *parent,
                                                           const ::std::string &childName) const;

    /**
     * @brief Parse conditional structure for if/elseif/else
     * @param ifAction IfActionNode to populate
     * @param ifElement <if> XML element
     */
    void parseConditionalStructure(::std::shared_ptr<::SCXML::Core::IfActionNode> ifAction,
                                   const xmlpp::Element *ifElement);

    /**
     * @brief Parse executable content within an element
     * @param parent Parent element containing executable content
     * @return Vector of parsed action nodes
     */
    ::std::vector<::std::shared_ptr<::SCXML::Model::IActionNode>> parseChildActions(const xmlpp::Element *parent);

    /**
     * @brief Generate unique action ID
     * @param actionType Type of action (e.g., "if", "send")
     * @return Unique action identifier
     */
    ::std::string generateActionId(const ::std::string &actionType);

    // Action ID counter for unique ID generation
    static size_t actionIdCounter_;
};

}  // namespace Parsing
}  // namespace SCXML