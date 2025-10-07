#include "parsing/ActionParser.h"
#include "actions/AssignAction.h"
#include "actions/CancelAction.h"
#include "actions/ForeachAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "actions/SendAction.h"
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

    // 요소 이름에서 액션 타입 결정
    std::string elementName = actionElement->get_name();
    size_t colonPos = elementName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < elementName.length()) {
        elementName = elementName.substr(colonPos + 1);
    }

    // ID 추출
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

    // 액션 타입별로 구체적인 액션 객체 생성
    if (elementName == "script") {
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
                        xmlContent += (const char *)buf->content;
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

        // Process children sequentially to maintain branch context
        for (auto child : children) {
            auto element = dynamic_cast<const xmlpp::Element *>(child);
            if (!element) {
                continue;
            }

            std::string childName = element->get_name();
            // Remove namespace prefix if present
            size_t colonPos = childName.find(':');
            if (colonPos != std::string::npos && colonPos + 1 < childName.length()) {
                childName = childName.substr(colonPos + 1);
            }

            if (childName == "elseif") {
                auto elseifCondAttr = element->get_attribute("cond");
                std::string elseifCondition = elseifCondAttr ? elseifCondAttr->get_value() : "";
                currentBranch = &ifAction->addElseIfBranch(elseifCondition);
            } else if (childName == "else") {
                currentBranch = &ifAction->addElseBranch();
            } else if (isActionNode(element)) {
                auto childAction = parseActionNode(element);
                if (childAction) {
                    if (currentBranch) {
                        // Add to current elseif/else branch
                        currentBranch->actions.push_back(childAction);
                    } else {
                        // Add to main if branch (before any elseif/else)
                        ifAction->addIfAction(childAction);
                    }
                }
            }
        }

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

        // W3C SCXML C.2 & B.2: Parse content child element for HTTP body and XML DOM
        auto contentElements = ParsingCommon::findChildElements(actionElement, "content");
        if (!contentElements.empty()) {
            auto contentElement = contentElements[0];
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
                LOG_DEBUG("ActionParser: Parsed send content: '{}'", contentText);
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
        // SCXML W3C 표준: foreach 요소 파싱
        auto arrayAttr = actionElement->get_attribute("array");
        auto itemAttr = actionElement->get_attribute("item");
        auto indexAttr = actionElement->get_attribute("index");

        std::string array = arrayAttr ? arrayAttr->get_value() : "";
        std::string item = itemAttr ? itemAttr->get_value() : "";
        std::string index = indexAttr ? indexAttr->get_value() : "";

        LOG_DEBUG("Parsing foreach: array='{}', item='{}', index='{}'", array, item, index);

        auto foreachAction = std::make_shared<RSM::ForeachAction>(array, item, index, id);

        // 중첩 액션들 파싱 (foreach 내부의 실행 가능 콘텐츠)
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

    // 액션 ID(이름) 가져오기
    auto nameAttr = externalActionNode->get_attribute("name");

    // name 속성이 없는 경우 id 속성 시도
    if (!nameAttr) {
        nameAttr = externalActionNode->get_attribute("id");
    }

    if (!nameAttr) {
        LOG_WARN("External action node missing required name attribute");
        return nullptr;
    }

    std::string id = nameAttr->get_value();
    LOG_DEBUG("Parsing external action: {}", id);

    // 외부 액션은 ScriptAction으로 처리 (향후 외부 액션 지원 시 확장)
    auto action = std::make_shared<RSM::ScriptAction>("", id);

    // 지연 시간은 무시 (현재 구현에서는 지원하지 않음)
    auto delayAttr = externalActionNode->get_attribute("delay");
    if (delayAttr) {
        LOG_DEBUG("ActionParser: Delay attribute value: {}", delayAttr->get_value());
    }

    // 외부 구현 요소 파싱
    auto implNode = externalActionNode->get_first_child("code:external-implementation");
    if (!implNode) {
        // 네임스페이스 없이 시도
        implNode = externalActionNode->get_first_child("external-implementation");
    }

    if (implNode) {
        auto element = dynamic_cast<const xmlpp::Element *>(implNode);
        if (element) {
            parseExternalImplementation(element, action);
        }
    }

    // 추가 속성 처리
    auto attributes = externalActionNode->get_attributes();
    for (auto attr : attributes) {
        auto xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
        if (xmlAttr) {
            std::string name = xmlAttr->get_name();
            std::string value = xmlAttr->get_value();

            // 이미 처리한 속성은 건너뜀
            if (name != "name" && name != "id" && name != "delay") {
                // 추가 속성 무시 (현재 구현에서는 지원하지 않음)
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

    // 모든 자식 요소 검사
    auto children = parentElement->get_children();
    LOG_DEBUG("ActionParser: Found {} child elements in {}", children.size(), parentElement->get_name());

    for (auto child : children) {
        auto element = dynamic_cast<const xmlpp::Element *>(child);
        if (!element) {
            LOG_DEBUG("ActionParser: Skipping non-element child node");
            continue;
        }

        LOG_DEBUG("ActionParser: Processing child element: '{}'", element->get_name());

        // 액션 노드 확인
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
        // 외부 실행 액션 노드 확인
        if (isExternalActionNode(element)) {
            auto action = parseExternalActionNode(element);
            if (action) {
                actions.push_back(action);
            }
        }
        // 특수 처리가 필요한 SCXML 요소 (if/elseif/else, foreach 등)
        else if (isSpecialExecutableContent(element)) {
            // 특수 요소 처리 - 자식 요소를 재귀적으로 파싱
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

    // 특수 요소에 대한 액션 노드 생성 (if 액션으로 처리)
    std::string localName = getLocalName(nodeName);
    std::shared_ptr<RSM::IActionNode> specialAction;

    if (localName == "if") {
        auto condAttr = element->get_attribute("cond");
        std::string condition = condAttr ? condAttr->get_value() : "";
        specialAction = std::make_shared<RSM::IfAction>(condition);
    } else {
        // 기타 특수 요소는 ScriptAction으로 처리
        specialAction = std::make_shared<RSM::ScriptAction>("", localName);
    }

    // 추가 속성 처리 (무시)
    auto attributes = element->get_attributes();
    for (auto attr : attributes) {
        auto xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
        if (xmlAttr) {
            std::string name = xmlAttr->get_name();
            std::string value = xmlAttr->get_value();
            LOG_DEBUG("ActionParser: Special content attribute {} = {}", name, value);
        }
    }

    // if/elseif/else 또는 foreach 내의 자식 요소 처리
    std::vector<std::shared_ptr<RSM::IActionNode>> childActions;
    auto children = element->get_children();

    for (auto child : children) {
        auto elementNode = dynamic_cast<const xmlpp::Element *>(child);
        if (elementNode) {
            // 자식 요소가 액션 노드인 경우 파싱
            if (isActionNode(elementNode) || isSpecialExecutableContent(elementNode)) {
                auto childAction = parseActionNode(elementNode);
                if (childAction) {
                    childActions.push_back(childAction);
                }
            }
        }
    }

    // 자식 액션 노드 처리 (현재 구현에서는 무시)
    if (!childActions.empty()) {
        LOG_DEBUG("Ignored {} child actions for {}", childActions.size(), nodeName);
    }

    // 생성된 특수 액션 추가
    actions.push_back(specialAction);
}

bool RSM::ActionParser::isActionNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    LOG_DEBUG("ActionParser: isActionNode checking element: '{}'", nodeName);

    // 커스텀 액션 태그
    if (matchNodeName(nodeName, "action") || matchNodeName(nodeName, "code:action")) {
        return true;
    }

    // 표준 SCXML 실행 가능 콘텐츠 태그
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

    // 특수 처리가 필요한 SCXML 실행 가능 콘텐츠
    // Note: else/elseif는 if 블록 내에서만 처리되므로 여기서는 제외
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
    // 정확히 일치하는 경우
    if (nodeName == searchName) {
        return true;
    }

    // 네임스페이스가 있는 경우 (예: "code:action")
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length()) {
        std::string localName = nodeName.substr(colonPos + 1);
        return localName == searchName;
    }

    return false;
}

std::string RSM::ActionParser::getLocalName(const std::string &nodeName) const {
    // 네임스페이스가 있는 경우 제거
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length()) {
        return nodeName.substr(colonPos + 1);
    }
    return nodeName;
}
