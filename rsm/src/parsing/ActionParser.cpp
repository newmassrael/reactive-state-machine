#include "parsing/ActionParser.h"
#include "common/Logger.h"
#include <algorithm>

RSM::ActionParser::ActionParser(std::shared_ptr<RSM::INodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    Logger::debug("RSM::ActionParser::Constructor - Creating action parser");
}

RSM::ActionParser::~ActionParser() {
    Logger::debug("RSM::ActionParser::Destructor - Destroying action parser");
}

std::shared_ptr<RSM::IActionNode> RSM::ActionParser::parseActionNode(const xmlpp::Element *actionNode) {
    if (!actionNode) {
        Logger::warn("RSM::ActionParser::parseActionNode() - Null action node");
        return nullptr;
    }

    // 액션 ID(이름) 가져오기
    std::string id;
    auto nameAttr = actionNode->get_attribute("name");

    // name 속성이 없는 경우 id 속성 시도
    if (!nameAttr) {
        nameAttr = actionNode->get_attribute("id");
    }

    // SCXML 표준 태그의 경우 노드 이름을 ID로 사용
    if (!nameAttr) {
        id = actionNode->get_name();

        // 네임스페이스가 있는 경우 제거
        size_t colonPos = id.find(':');
        if (colonPos != std::string::npos && colonPos + 1 < id.length()) {
            id = id.substr(colonPos + 1);
        }
    } else {
        id = nameAttr->get_value();
    }

    Logger::debug("RSM::ActionParser::parseActionNode() - Parsing action: " + id);

    // 액션 노드 생성
    auto action = nodeFactory_->createActionNode(id);

    // 타입 속성 처리
    auto typeAttr = actionNode->get_attribute("type");
    if (typeAttr) {
        action->setType(typeAttr->get_value());
        Logger::debug("RSM::ActionParser::parseActionNode() - Type: " + typeAttr->get_value());
    } else {
        // 표준 SCXML 태그의 경우 타입을 태그 이름으로 설정
        action->setType(id);
    }

    // 외부 구현 요소 파싱
    auto implNode = actionNode->get_first_child("code:external-implementation");
    if (!implNode) {
        // 네임스페이스 없이 시도
        implNode = actionNode->get_first_child("external-implementation");
    }

    if (implNode) {
        auto element = dynamic_cast<const xmlpp::Element *>(implNode);
        if (element) {
            parseExternalImplementation(element, action);
        }
    }

    // 추가 속성 처리
    auto attributes = actionNode->get_attributes();
    for (auto attr : attributes) {
        auto xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
        if (xmlAttr) {
            std::string name = xmlAttr->get_name();
            std::string value = xmlAttr->get_value();

            // 이미 처리한 속성은 건너뜀
            if (name != "name" && name != "id" && name != "type") {
                action->setAttribute(name, value);
                Logger::debug("RSM::ActionParser::parseActionNode() - Added attribute: " + name + " = " + value);
            }
        }
    }

    // 자식 요소를 단순 텍스트로 취급하는 대신 계층적으로 처리
    std::vector<std::shared_ptr<RSM::IActionNode>> childActions;
    auto children = actionNode->get_children();
    for (auto child : children) {
        auto textNode = dynamic_cast<const xmlpp::TextNode *>(child);
        if (textNode && !textNode->is_white_space()) {
            // 텍스트 내용이 있는 경우 처리
            action->setAttribute("textContent", textNode->get_content());
            Logger::debug("RSM::ActionParser::parseActionNode() - Added text content");
        } else {
            auto elementNode = dynamic_cast<const xmlpp::Element *>(child);
            if (elementNode && isActionNode(elementNode)) {
                // 자식 요소가 액션 노드인 경우 재귀적으로 파싱
                auto childAction = parseActionNode(elementNode);
                if (childAction) {
                    childActions.push_back(childAction);
                    Logger::debug("RSM::ActionParser::parseActionNode() - Added child action: " + childAction->getId());
                }
            }
        }
    }

    // 자식 액션 노드 추가
    if (!childActions.empty()) {
        action->setChildActions(childActions);
        Logger::debug("RSM::ActionParser::parseActionNode() - Added " + std::to_string(childActions.size()) +
                      " child actions");
    }

    Logger::debug("RSM::ActionParser::parseActionNode() - Action parsed successfully");
    return action;
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

    // 액션 노드 생성
    auto action = nodeFactory_->createActionNode(id);

    // 외부 액션 타입 설정
    action->setType("external");

    // 지연 시간 속성 처리
    auto delayAttr = externalActionNode->get_attribute("delay");
    if (delayAttr) {
        action->setAttribute("delay", delayAttr->get_value());
        Logger::debug("RSM::ActionParser::parseExternalActionNode() - Delay: " + delayAttr->get_value());
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
                action->setAttribute(name, value);
                Logger::debug("RSM::ActionParser::parseExternalActionNode() - Added attribute: " + name + " = " +
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
    std::vector<std::shared_ptr<IActionNode>> actions;

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
                                                      std::vector<std::shared_ptr<IActionNode>> &actions) {
    if (!element) {
        return;
    }

    std::string nodeName = element->get_name();
    Logger::debug("RSM::ActionParser::parseSpecialExecutableContent() - Parsing "
                  "special content: " +
                  nodeName);

    // 특수 요소에 대한 액션 노드 생성
    auto specialAction = nodeFactory_->createActionNode(getLocalName(nodeName));
    specialAction->setType(getLocalName(nodeName));

    // 속성 처리
    auto attributes = element->get_attributes();
    for (auto attr : attributes) {
        auto xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
        if (xmlAttr) {
            std::string name = xmlAttr->get_name();
            std::string value = xmlAttr->get_value();
            specialAction->setAttribute(name, value);
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

    // 자식 액션 노드 추가
    if (!childActions.empty()) {
        specialAction->setChildActions(childActions);
        Logger::debug("RSM::ActionParser::parseSpecialExecutableContent() - Added " +
                      std::to_string(childActions.size()) + " child actions to " + nodeName);
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
                                                    std::shared_ptr<IActionNode> actionNode) {
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
        actionNode->setExternalClass(className);
        Logger::debug("RSM::ActionParser::parseExternalImplementation() - External class: " + className);
    }

    if (factoryAttr) {
        std::string factory = factoryAttr->get_value();
        actionNode->setExternalFactory(factory);
        Logger::debug("RSM::ActionParser::parseExternalImplementation() - External "
                      "factory: " +
                      factory);
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
