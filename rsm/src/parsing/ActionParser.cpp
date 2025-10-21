#include "parsing/ActionParser.h"
#include "actions/AssignAction.h"
#include "actions/CancelAction.h"
#include "actions/ForeachAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "actions/SendAction.h"
#include "common/LogUtils.h"
#include "common/Logger.h"
#include "parsing/ParsingCommon.h"  // ✅ Fix: Missing ParsingCommon header
#include <algorithm>
#include <libxml++/nodes/textnode.h>
#include <libxml/tree.h>

RSM::ActionParser::ActionParser(std::shared_ptr<RSM::NodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    LOG_DEBUG("Creating action parser");
}

RSM::ActionParser::~ActionParser() {
    LOG_DEBUG("Destroying action parser");
}

std::shared_ptr<RSM::IActionNode> RSM::ActionParser::parseActionNode(const xmlpp::Element *actionElement) {
    if (!actionElement) {
        LOG_WARN("Null action element");
        return nullptr;
    }

    // Determine action type from element name
    std::string elementName = actionElement->get_name();
    size_t colonPos = elementName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < elementName.length()) {
        elementName = elementName.substr(colonPos + 1);
    }

    // Extract ID
    std::string id;
    auto nameAttr = actionElement->get_attribute("name");
    if (!nameAttr) {
        nameAttr = actionElement->get_attribute("id");
    }
    if (nameAttr) {
        id = nameAttr->get_value();
    } else {
        id = elementName;
    }

    LOG_DEBUG("ActionParser: Processing action with id: {}", id);

    // Create specific action objects by action type
    if (elementName == "script") {
        // W3C SCXML 5.8: Check for external script source
        auto srcAttr = actionElement->get_attribute("src");

        if (srcAttr) {
            // External script specified via 'src' attribute
            std::string srcPath = srcAttr->get_value();

            // ⚠️ LIMITATION: External script loading NOT implemented (W3C SCXML 5.8)
            //
            // W3C SCXML 5.8 requires: "If the script specified by the 'src' attribute cannot be
            // downloaded within a platform-specific timeout interval, the document is considered
            // non-conformant, and the platform MUST reject it."
            //
            // Current implementation: ALL external scripts are rejected for security reasons.
            // W3C Test 301 (external script validation) is marked as "manual: True" in metadata
            // and is automatically skipped by TestResultValidator::shouldSkipTest().
            //
            // Workaround: Use inline scripts instead: <script>your code here</script>
            // Future work: Implement secure external script loading with timeout and sandboxing
            LOG_ERROR("ActionParser: W3C SCXML 5.8 - Document rejected, cannot load external script: {}",
                      Log::sanitize(srcPath));

            // Reject document by returning nullptr (caller will handle error)
            return nullptr;
        }

        // Inline script: read text content
        std::string content;
        auto textNode = actionElement->get_first_child_text();
        if (textNode) {
            content = textNode->get_content();
        }
        return std::make_shared<RSM::ScriptAction>(content, id);

    } else if (elementName == "assign") {
        auto locationAttr = actionElement->get_attribute("location");
        auto exprAttr = actionElement->get_attribute("expr");
        std::string location = locationAttr ? locationAttr->get_value() : "";
        std::string expr;

        if (exprAttr) {
            expr = exprAttr->get_value();
        } else {
            // W3C SCXML test 530: Serialize XML content as string literal
            // When no expr attribute, serialize XML children and wrap in quotes
            std::string xmlContent;
            auto children = actionElement->get_children();
            for (auto child : children) {
                // XML element - serialize using libxml2
                if (auto childElement = dynamic_cast<const xmlpp::Element *>(child)) {
                    _xmlNode *node = const_cast<_xmlNode *>(childElement->cobj());
                    auto buf = xmlBufferCreate();
                    if (buf) {
                        xmlNodeDump(buf, node->doc, node, 0, 0);
                        xmlContent += (const char *)xmlBufferContent(buf);
                        xmlBufferFree(buf);
                    }
                } else if (auto textNode = dynamic_cast<const xmlpp::TextNode *>(child)) {
                    // Text node - add content directly
                    std::string text = textNode->get_content();
                    // Only add non-whitespace text
                    if (!std::all_of(text.begin(), text.end(), ::isspace)) {
                        xmlContent += text;
                    }
                }
            }

            // Wrap XML content in JavaScript string literal (escape quotes)
            if (!xmlContent.empty()) {
                // Escape quotes and backslashes for JavaScript string literal
                std::string escaped;
                for (char c : xmlContent) {
                    if (c == '"') {
                        escaped += "\\\"";
                    } else if (c == '\\') {
                        escaped += "\\\\";
                    } else if (c == '\n') {
                        escaped += "\\n";
                    } else if (c == '\r') {
                        escaped += "\\r";
                    } else {
                        escaped += c;
                    }
                }
                expr = "\"" + escaped + "\"";
            }
        }

        return std::make_shared<RSM::AssignAction>(location, expr, id);

    } else if (elementName == "log") {
        auto exprAttr = actionElement->get_attribute("expr");
        auto labelAttr = actionElement->get_attribute("label");
        std::string message;
        if (exprAttr) {
            message = exprAttr->get_value();
        } else if (labelAttr) {
            message = labelAttr->get_value();
        }
        return std::make_shared<RSM::LogAction>(message, id);

    } else if (elementName == "raise") {
        auto eventAttr = actionElement->get_attribute("event");
        std::string event = eventAttr ? eventAttr->get_value() : "";
        return std::make_shared<RSM::RaiseAction>(event, id);

    } else if (elementName == "if") {
        auto condAttr = actionElement->get_attribute("cond");
        std::string condition = condAttr ? condAttr->get_value() : "";
        auto ifAction = std::make_shared<RSM::IfAction>(condition, id);

        // Parse child elements for elseif and else branches
        auto children = actionElement->get_children();
        RSM::IfAction::ConditionalBranch *currentBranch = nullptr;

        LOG_DEBUG("IF action: found {} children, condition='{}'", children.size(), condition);

        // Process children sequentially to maintain branch context
        int childIndex = 0;
        for (auto child : children) {
            auto element = dynamic_cast<const xmlpp::Element *>(child);
            if (!element) {
                LOG_DEBUG("  Child {}: not an element (text node)", childIndex++);
                continue;
            }

            std::string childName = element->get_name();
            // Remove namespace prefix if present
            size_t colonPos = childName.find(':');
            if (colonPos != std::string::npos && colonPos + 1 < childName.length()) {
                childName = childName.substr(colonPos + 1);
            }

            LOG_DEBUG("  Child {}: name='{}', currentBranch={}", childIndex, childName,
                      currentBranch ? "else/elseif" : "if");

            if (childName == "elseif") {
                auto elseifCondAttr = element->get_attribute("cond");
                std::string elseifCondition = elseifCondAttr ? elseifCondAttr->get_value() : "";
                currentBranch = &ifAction->addElseIfBranch(elseifCondition);
                LOG_DEBUG("    Added elseif branch with condition='{}'", elseifCondition);
            } else if (childName == "else") {
                currentBranch = &ifAction->addElseBranch();
                LOG_DEBUG("    Added else branch");
            } else if (isActionNode(element)) {
                auto childAction = parseActionNode(element);
                if (childAction) {
                    if (currentBranch) {
                        // Add to current elseif/else branch
                        currentBranch->actions.push_back(childAction);
                        LOG_DEBUG("    Added {} action to current branch (size now: {})", childName,
                                  currentBranch->actions.size());
                    } else {
                        // Add to main if branch (before any elseif/else)
                        ifAction->addIfAction(childAction);
                        LOG_DEBUG("    Added {} action to main if branch", childName);
                    }
                } else {
                    LOG_WARN("    parseActionNode returned nullptr for '{}'", childName);
                }
            } else {
                LOG_DEBUG("    Skipping non-action element '{}'", childName);
            }
            childIndex++;
        }

        LOG_DEBUG("IF action complete: {} branches", ifAction->getBranchCount());
        return ifAction;

    } else if (elementName == "send") {
        auto eventAttr = actionElement->get_attribute("event");
        std::string event = eventAttr ? eventAttr->get_value() : "";

        auto sendAction = std::make_shared<RSM::SendAction>(event, id);

        // Parse idlocation attribute for W3C compliance
        auto idlocationAttr = actionElement->get_attribute("idlocation");
        if (idlocationAttr) {
            sendAction->setIdLocation(idlocationAttr->get_value());
        }

        // Parse other send attributes
        auto targetAttr = actionElement->get_attribute("target");
        if (targetAttr) {
            sendAction->setTarget(targetAttr->get_value());
        }

        auto targetExprAttr = actionElement->get_attribute("targetexpr");
        if (targetExprAttr) {
            sendAction->setTargetExpr(targetExprAttr->get_value());
        }

        auto eventExprAttr = actionElement->get_attribute("eventexpr");
        if (eventExprAttr) {
            sendAction->setEventExpr(eventExprAttr->get_value());
        }

        auto delayAttr = actionElement->get_attribute("delay");
        if (delayAttr) {
            sendAction->setDelay(delayAttr->get_value());
        }

        auto delayExprAttr = actionElement->get_attribute("delayexpr");
        if (delayExprAttr) {
            sendAction->setDelayExpr(delayExprAttr->get_value());
        }

        auto typeAttr = actionElement->get_attribute("type");
        if (typeAttr) {
            sendAction->setType(typeAttr->get_value());
        }

        // W3C SCXML 6.2: Parse typeexpr attribute for dynamic type evaluation (Test 174)
        auto typeExprAttr = actionElement->get_attribute("typeexpr");
        if (typeExprAttr) {
            sendAction->setTypeExpr(typeExprAttr->get_value());
        }

        // W3C SCXML C.1: Parse namelist attribute for event data
        auto namelistAttr = actionElement->get_attribute("namelist");
        if (namelistAttr) {
            sendAction->setNamelist(namelistAttr->get_value());
        }

        // W3C SCXML: send element uses 'id' attribute for sendid (for cancellation reference)
        auto sendIdAttr = actionElement->get_attribute("id");
        if (sendIdAttr) {
            sendAction->setSendId(sendIdAttr->get_value());
        }

        // W3C SCXML 5.10 & C.2: Parse content child element for event data
        auto contentElements = ParsingCommon::findChildElements(actionElement, "content");
        if (!contentElements.empty()) {
            auto contentElement = contentElements[0];

            // W3C SCXML 5.10: Check for expr attribute (dynamic content evaluation)
            auto contentExprAttr = contentElement->get_attribute("expr");
            if (contentExprAttr) {
                // Use expr attribute for dynamic content
                std::string contentExpr = contentExprAttr->get_value();
                sendAction->setContentExpr(contentExpr);
                LOG_DEBUG("ActionParser: Parsed send content expr: '{}'", contentExpr);
            } else {
                // W3C SCXML 5.10: Use child content as literal (test179)
                std::string contentText;

                // W3C SCXML B.2: Content element can contain XML elements requiring full serialization
                auto children = contentElement->get_children();
                for (auto child : children) {
                    // Check if child is an element node (XML content)
                    if (auto elementNode = dynamic_cast<const xmlpp::Element *>(child)) {
                        // Serialize XML element using libxml2
                        xmlNodePtr xmlNode = const_cast<xmlNodePtr>(elementNode->cobj());
                        xmlBufferPtr buffer = xmlBufferCreate();
                        xmlNodeDump(buffer, xmlNode->doc, xmlNode, 0, 0);
                        std::string xmlContent = reinterpret_cast<const char *>(xmlBufferContent(buffer));
                        xmlBufferFree(buffer);
                        contentText += xmlContent;
                    } else if (auto textNode = dynamic_cast<const xmlpp::TextNode *>(child)) {
                        // Include text nodes as-is
                        contentText += textNode->get_content();
                    }
                }

                if (!contentText.empty()) {
                    sendAction->setContent(contentText);
                    LOG_DEBUG("ActionParser: Parsed send content literal: '{}'", contentText);
                }
            }
        }

        // 🚨 CRITICAL: Parse param child elements for W3C SCXML compliance
        auto paramElements = ParsingCommon::findChildElements(actionElement, "param");
        for (auto paramElement : paramElements) {
            auto nameAttr = paramElement->get_attribute("name");
            auto exprAttr = paramElement->get_attribute("expr");

            if (nameAttr && exprAttr) {
                std::string paramName = nameAttr->get_value();
                std::string paramExpr = exprAttr->get_value();
                sendAction->addParamWithExpr(paramName, paramExpr);
                LOG_DEBUG("ActionParser: Added send param '{}' with expr '{}'", paramName, paramExpr);
            } else {
                LOG_WARN("ActionParser: send param element missing name or expr attribute");
            }
        }

        return sendAction;

    } else if (elementName == "cancel") {
        auto sendidAttr = actionElement->get_attribute("sendid");
        auto sendidexprAttr = actionElement->get_attribute("sendidexpr");

        std::string sendid = sendidAttr ? sendidAttr->get_value() : "";
        auto cancelAction = std::make_shared<RSM::CancelAction>(sendid, id);

        // W3C SCXML 1.0: Handle sendidexpr attribute for dynamic send ID evaluation
        if (sendidexprAttr) {
            cancelAction->setSendIdExpr(sendidexprAttr->get_value());
        }

        return cancelAction;

    } else if (elementName == "foreach") {
        // SCXML W3C standard: Parse foreach element
        auto arrayAttr = actionElement->get_attribute("array");
        auto itemAttr = actionElement->get_attribute("item");
        auto indexAttr = actionElement->get_attribute("index");

        std::string array = arrayAttr ? arrayAttr->get_value() : "";
        std::string item = itemAttr ? itemAttr->get_value() : "";
        std::string index = indexAttr ? indexAttr->get_value() : "";

        LOG_DEBUG("Parsing foreach: array='{}', item='{}', index='{}'", array, item, index);

        auto foreachAction = std::make_shared<RSM::ForeachAction>(array, item, index, id);

        // Parse nested actions (executable content inside foreach)
        auto childActions = parseActionsInElement(actionElement);
        for (const auto &childAction : childActions) {
            if (childAction) {
                foreachAction->addIterationAction(childAction);
            }
        }

        LOG_DEBUG("Foreach action created with {} child actions", childActions.size());

        return foreachAction;

    } else {
        LOG_WARN("Unknown action type: {}, creating ScriptAction", elementName);
        return std::make_shared<RSM::ScriptAction>("", id);
    }
}

std::shared_ptr<RSM::IActionNode> RSM::ActionParser::parseExternalActionNode(const xmlpp::Element *externalActionNode) {
    if (!externalActionNode) {
        LOG_WARN("Null external action node");
        return nullptr;
    }

    // Get action ID (name)
    auto nameAttr = externalActionNode->get_attribute("name");

    // Try id attribute if name attribute is missing
    if (!nameAttr) {
        nameAttr = externalActionNode->get_attribute("id");
    }

    if (!nameAttr) {
        LOG_WARN("External action node missing required name attribute");
        return nullptr;
    }

    std::string id = nameAttr->get_value();
    LOG_DEBUG("Parsing external action: {}", id);

    // External actions are handled as ScriptAction (extend when external action support is added)
    auto action = std::make_shared<RSM::ScriptAction>("", id);

    // Ignore delay time (not supported in current implementation)
    auto delayAttr = externalActionNode->get_attribute("delay");
    if (delayAttr) {
        LOG_DEBUG("ActionParser: Delay attribute value: {}", delayAttr->get_value());
    }

    // Parse external implementation element
    auto implNode = externalActionNode->get_first_child("code:external-implementation");
    if (!implNode) {
        // Try without namespace
        implNode = externalActionNode->get_first_child("external-implementation");
    }

    if (implNode) {
        auto element = dynamic_cast<const xmlpp::Element *>(implNode);
        if (element) {
            parseExternalImplementation(element, action);
        }
    }

    // Process additional attributes
    auto attributes = externalActionNode->get_attributes();
    for (auto attr : attributes) {
        auto xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
        if (xmlAttr) {
            std::string name = xmlAttr->get_name();
            std::string value = xmlAttr->get_value();

            // Skip already processed attributes
            if (name != "name" && name != "id" && name != "delay") {
                // Ignore additional attributes (not supported in current implementation)
                LOG_DEBUG("ActionParser: Additional attribute {} = {}", name, value);
            }
        }
    }

    LOG_DEBUG("External action parsed successfully");
    return action;
}

std::vector<std::shared_ptr<RSM::IActionNode>>
RSM::ActionParser::parseActionsInElement(const xmlpp::Element *parentElement) {
    std::vector<std::shared_ptr<RSM::IActionNode>> actions;

    if (!parentElement) {
        LOG_WARN("Null parent element");
        return actions;
    }

    LOG_DEBUG("ActionParser: Parsing actions in element: {}", parentElement->get_name());

    // Examine all child elements
    auto children = parentElement->get_children();
    LOG_DEBUG("ActionParser: Found {} child elements in {}", children.size(), parentElement->get_name());

    for (auto child : children) {
        auto element = dynamic_cast<const xmlpp::Element *>(child);
        if (!element) {
            LOG_DEBUG("ActionParser: Skipping non-element child node");
            continue;
        }

        LOG_DEBUG("ActionParser: Processing child element: '{}'", element->get_name());

        // Check action node
        if (isActionNode(element)) {
            LOG_DEBUG("ActionParser: '{}' is recognized as action node", element->get_name());
            auto action = parseActionNode(element);
            if (action) {
                LOG_DEBUG("ActionParser: Successfully parsed action node: '{}'", element->get_name());
                actions.push_back(action);
            } else {
                LOG_ERROR("ActionParser: Failed to parse action node: '{}'", element->get_name());
            }
        } else {
            LOG_DEBUG("ActionParser: '{}' is NOT recognized as action node", element->get_name());
        }
        // Check external executable action node
        if (isExternalActionNode(element)) {
            auto action = parseExternalActionNode(element);
            if (action) {
                actions.push_back(action);
            }
        }
        // SCXML elements requiring special processing (if/elseif/else, foreach, etc.)
        else if (isSpecialExecutableContent(element)) {
            // Process special elements - recursively parse child elements
            parseSpecialExecutableContent(element, actions);
        }
    }

    LOG_DEBUG("ActionParser: Parsed {} actions", actions.size());
    return actions;
}

void RSM::ActionParser::parseSpecialExecutableContent(const xmlpp::Element *element,
                                                      std::vector<std::shared_ptr<RSM::IActionNode>> &actions) {
    if (!element) {
        return;
    }

    std::string nodeName = element->get_name();
    LOG_DEBUG("Parsing special content: {}", nodeName);

    // Special elements should use parseActionNode for proper parsing
    std::string localName = getLocalName(nodeName);

    if (localName == "if" || localName == "foreach") {
        // Use parseActionNode for proper IF/foreach parsing with all child actions
        auto specialAction = parseActionNode(element);
        if (specialAction) {
            actions.push_back(specialAction);
        }
    } else {
        // Other special elements treated as script actions
        auto specialAction = std::make_shared<RSM::ScriptAction>("", localName);
        actions.push_back(specialAction);
    }
}

bool RSM::ActionParser::isActionNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    LOG_DEBUG("ActionParser: isActionNode checking element: '{}'", nodeName);

    // Custom action tags
    if (matchNodeName(nodeName, "action") || matchNodeName(nodeName, "code:action")) {
        return true;
    }

    // Standard SCXML executable content tags
    bool isStandardAction = matchNodeName(nodeName, "raise") || matchNodeName(nodeName, "assign") ||
                            matchNodeName(nodeName, "script") || matchNodeName(nodeName, "log") ||
                            matchNodeName(nodeName, "send") || matchNodeName(nodeName, "cancel");

    LOG_DEBUG("ActionParser: isActionNode result for '{}': {}", nodeName, isStandardAction);
    return isStandardAction;
}

bool RSM::ActionParser::isSpecialExecutableContent(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();

    // SCXML executable content requiring special processing
    // Note: else/elseif are only processed within if blocks, excluded here
    return matchNodeName(nodeName, "if") || matchNodeName(nodeName, "foreach") || matchNodeName(nodeName, "invoke") ||
           matchNodeName(nodeName, "finalize");
}

bool RSM::ActionParser::isExternalActionNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    return matchNodeName(nodeName, "external-action") || matchNodeName(nodeName, "code:external-action");
}

void RSM::ActionParser::parseExternalImplementation(const xmlpp::Element *element,
                                                    std::shared_ptr<RSM::IActionNode> actionNode) {
    if (!element || !actionNode) {
        return;
    }

    LOG_DEBUG("Parsing external implementation for action: {}", actionNode->getId());

    auto classAttr = element->get_attribute("class");
    auto factoryAttr = element->get_attribute("factory");

    if (classAttr) {
        std::string className = classAttr->get_value();
        LOG_DEBUG("Class name: {}", className);
    }

    if (factoryAttr) {
        std::string factory = factoryAttr->get_value();
        LOG_DEBUG("Factory: {}", factory);
    }
}

bool RSM::ActionParser::matchNodeName(const std::string &nodeName, const std::string &searchName) const {
    // Exact match
    if (nodeName == searchName) {
        return true;
    }

    // With namespace (e.g., "code:action")
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length()) {
        std::string localName = nodeName.substr(colonPos + 1);
        return localName == searchName;
    }

    return false;
}

std::string RSM::ActionParser::getLocalName(const std::string &nodeName) const {
    // Remove namespace if present
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length()) {
        return nodeName.substr(colonPos + 1);
    }
    return nodeName;
}
