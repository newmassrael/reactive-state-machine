#include "parsing/ActionParser.h"
#include "actions/AssignAction.h"
#include "actions/CancelAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "actions/SendAction.h"
#include "common/Logger.h"
#include <algorithm>

RSM::ActionParser::ActionParser(std::shared_ptr<RSM::INodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    Logger::debug("RSM::ActionParser::Constructor - Creating action parser");
}

RSM::ActionParser::~ActionParser() {
    Logger::debug("RSM::ActionParser::Destructor - Destroying action parser");
}

std::shared_ptr<RSM::IActionNode> RSM::ActionParser::parseActionNode(const xmlpp::Element *actionElement) {
    if (!actionElement) {
        Logger::warn("RSM::ActionParser::parseActionNode() - Null action element");
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

    Logger::debug("RSM::ActionParser::parseActionNode() - Parsing action type: " + elementName + ", id: " + id);

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
        std::string expr = exprAttr ? exprAttr->get_value() : "";
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
        return std::make_shared<RSM::IfAction>(condition, id);

    } else if (elementName == "send") {
        auto eventAttr = actionElement->get_attribute("event");
        std::string event = eventAttr ? eventAttr->get_value() : "";
        return std::make_shared<RSM::SendAction>(event, id);

    } else if (elementName == "cancel") {
        auto sendidAttr = actionElement->get_attribute("sendid");
        std::string sendid = sendidAttr ? sendidAttr->get_value() : "";
        return std::make_shared<RSM::CancelAction>(sendid, id);

    } else {
        Logger::warn("RSM::ActionParser::parseActionNode() - Unknown action type: " + elementName +
                     ", creating ScriptAction");
        return std::make_shared<RSM::ScriptAction>("", id);
    }
}

std::shared_ptr<RSM::IActionNode> RSM::ActionParser::parseExternalActionNode(const xmlpp::Element *externalActionNode) {
    if (!externalActionNode) {
        Logger::warn("RSM::ActionParser::parseExternalActionNode() - Null external "
                     "action node");
        return nullptr;
    }

    // 액션 ID(이름) 가져오기
    auto nameAttr = externalActionNode->get_attribute("name");

    // name 속성이 없는 경우 id 속성 시도
    if (!nameAttr) {
        nameAttr = externalActionNode->get_attribute("id");
    }

    if (!nameAttr) {
        Logger::warn("RSM::ActionParser::parseExternalActionNode() - External "
                     "action node missing required name attribute");
        return nullptr;
    }

    std::string id = nameAttr->get_value();
    Logger::debug("RSM::ActionParser::parseExternalActionNode() - Parsing "
                  "external action: " +
                  id);

    // 외부 액션은 ScriptAction으로 처리 (향후 외부 액션 지원 시 확장)
    auto action = std::make_shared<RSM::ScriptAction>("", id);

    // 지연 시간은 무시 (현재 구현에서는 지원하지 않음)
    auto delayAttr = externalActionNode->get_attribute("delay");
    if (delayAttr) {
        Logger::debug("RSM::ActionParser::parseExternalActionNode() - Delay ignored: " + delayAttr->get_value());
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
                Logger::debug("RSM::ActionParser::parseExternalActionNode() - Ignored attribute: " + name + " = " +
                              value);
            }
        }
    }

    Logger::debug("RSM::ActionParser::parseExternalActionNode() - External "
                  "action parsed successfully");
    return action;
}

std::vector<std::shared_ptr<RSM::IActionNode>>
RSM::ActionParser::parseActionsInElement(const xmlpp::Element *parentElement) {
    std::vector<std::shared_ptr<RSM::IActionNode>> actions;

    if (!parentElement) {
        Logger::warn("RSM::ActionParser::parseActionsInElement() - Null parent element");
        return actions;
    }

    Logger::debug("RSM::ActionParser::parseActionsInElement() - Parsing actions "
                  "in element: " +
                  parentElement->get_name());

    // 모든 자식 요소 검사
    auto children = parentElement->get_children();
    for (auto child : children) {
        auto element = dynamic_cast<const xmlpp::Element *>(child);
        if (!element) {
            continue;
        }

        // 액션 노드 확인
        if (isActionNode(element)) {
            auto action = parseActionNode(element);
            if (action) {
                actions.push_back(action);
            }
        }
        // 외부 실행 액션 노드 확인
        else if (isExternalActionNode(element)) {
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

    Logger::debug("RSM::ActionParser::parseActionsInElement() - Found " + std::to_string(actions.size()) + " actions");
    return actions;
}

void RSM::ActionParser::parseSpecialExecutableContent(const xmlpp::Element *element,
                                                      std::vector<std::shared_ptr<RSM::IActionNode>> &actions) {
    if (!element) {
        return;
    }

    std::string nodeName = element->get_name();
    Logger::debug("RSM::ActionParser::parseSpecialExecutableContent() - Parsing "
                  "special content: " +
                  nodeName);

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
            Logger::debug("RSM::ActionParser::parseSpecialExecutableContent() - Ignored attribute: " + name + " = " +
                          value);
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
        Logger::debug("RSM::ActionParser::parseSpecialExecutableContent() - Ignored " +
                      std::to_string(childActions.size()) + " child actions for " + nodeName);
    }

    // 생성된 특수 액션 추가
    actions.push_back(specialAction);
}

bool RSM::ActionParser::isActionNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();

    // 커스텀 액션 태그
    if (matchNodeName(nodeName, "action") || matchNodeName(nodeName, "code:action")) {
        return true;
    }

    // 표준 SCXML 실행 가능 콘텐츠 태그
    return matchNodeName(nodeName, "raise") || matchNodeName(nodeName, "assign") || matchNodeName(nodeName, "script") ||
           matchNodeName(nodeName, "log") || matchNodeName(nodeName, "send") || matchNodeName(nodeName, "cancel");
}

bool RSM::ActionParser::isSpecialExecutableContent(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();

    // 특수 처리가 필요한 SCXML 실행 가능 콘텐츠
    return matchNodeName(nodeName, "if") || matchNodeName(nodeName, "elseif") || matchNodeName(nodeName, "else") ||
           matchNodeName(nodeName, "foreach") || matchNodeName(nodeName, "invoke") ||
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

    Logger::debug("RSM::ActionParser::parseExternalImplementation() - Parsing "
                  "external implementation for action: " +
                  actionNode->getId());

    auto classAttr = element->get_attribute("class");
    auto factoryAttr = element->get_attribute("factory");

    if (classAttr) {
        std::string className = classAttr->get_value();
        Logger::debug("RSM::ActionParser::parseExternalImplementation() - External class ignored: " + className);
    }

    if (factoryAttr) {
        std::string factory = factoryAttr->get_value();
        Logger::debug("RSM::ActionParser::parseExternalImplementation() - External factory ignored: " + factory);
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
