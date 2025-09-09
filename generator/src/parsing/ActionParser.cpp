#include "parsing/ActionParser.h"
#include "common/Logger.h"
#include "core/actions/AssignActionNode.h"
#include "core/actions/IfActionNode.h"
#include "core/actions/LogActionNode.h"
#include "core/actions/RaiseActionNode.h"
#include "core/actions/ScriptActionNode.h"
#include "core/actions/SendActionNode.h"
#include <algorithm>

using namespace SCXML::Parsing;
using namespace std;

ActionParser::ActionParser(std::shared_ptr<SCXML::Model::INodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    SCXML::Common::Logger::debug("ActionParser::Constructor - Creating action parser");
}

ActionParser::~ActionParser() {
    SCXML::Common::Logger::debug("ActionParser::Destructor - Destroying action parser");
}

std::shared_ptr<SCXML::Model::IActionNode> ActionParser::parseActionNode(const xmlpp::Element *actionNode) {
    if (!actionNode) {
        SCXML::Common::Logger::warning("ActionParser::parseActionNode() - Null action node");
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

    SCXML::Common::Logger::debug("ActionParser::parseActionNode() - Parsing action: " + id);

    // 액션 노드 생성
    auto action = nodeFactory_->createActionNode(id);

    // 타입 속성 처리
    auto typeAttr = actionNode->get_attribute("type");
    if (typeAttr) {
        action->setType(typeAttr->get_value());
        SCXML::Common::Logger::debug("ActionParser::parseActionNode() - Type: " + typeAttr->get_value());
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
                // Special handling for AssignActionNode attributes
                if (id == "assign") {
                    auto assignAction = std::dynamic_pointer_cast<SCXML::Core::AssignActionNode>(action);
                    if (assignAction) {
                        if (name == "location") {
                            assignAction->setLocation(value);

                        } else if (name == "expr") {
                            assignAction->setExpr(value);

                        } else if (name == "attr") {
                            assignAction->setAttr(value);

                        } else {
                            action->setAttribute(name, value);
                        }
                    } else {
                        action->setAttribute(name, value);
                    }
                }
                // Special handling for LogActionNode attributes
                else if (id == "log") {
                    auto logAction = std::dynamic_pointer_cast<SCXML::Core::LogActionNode>(action);
                    if (logAction) {
                        if (name == "expr") {
                            logAction->setExpr(value);

                        } else if (name == "label") {
                            logAction->setLabel(value);

                        } else if (name == "level") {
                            logAction->setLevel(value);

                        } else {
                            action->setAttribute(name, value);
                        }
                    } else {
                        action->setAttribute(name, value);
                    }
                }
                // Special handling for ScriptActionNode attributes
                else if (id == "script") {
                    auto scriptAction = std::dynamic_pointer_cast<SCXML::Core::ScriptActionNode>(action);
                    if (scriptAction) {
                        if (name == "src") {
                            scriptAction->setSrc(value);

                        } else if (name == "lang") {
                            scriptAction->setLang(value);

                        } else {
                            action->setAttribute(name, value);
                        }
                    } else {
                        action->setAttribute(name, value);
                    }
                }
                // Special handling for RaiseActionNode attributes
                else if (id == "raise") {
                    auto raiseAction = std::dynamic_pointer_cast<SCXML::Core::RaiseActionNode>(action);
                    if (raiseAction) {
                        if (name == "event") {
                            raiseAction->setEvent(value);

                        } else if (name == "data") {
                            raiseAction->setData(value);

                        } else {
                            action->setAttribute(name, value);
                        }
                    } else {
                        action->setAttribute(name, value);
                    }
                }
                // Special handling for SendActionNode attributes
                else if (id == "send") {
                    auto sendAction = std::dynamic_pointer_cast<SCXML::Core::SendActionNode>(action);
                    if (sendAction) {
                        if (name == "event") {
                            sendAction->setEvent(value);
                        } else if (name == "target") {
                            sendAction->setTarget(value);
                        } else if (name == "delay") {
                            sendAction->setDelay(value);
                        } else if (name == "data") {
                            sendAction->setData(value);
                        } else if (name == "sendid") {
                            sendAction->setSendId(value);
                        } else if (name == "type") {
                            sendAction->setType(value);
                        } else {
                            action->setAttribute(name, value);
                        }
                    } else {
                        action->setAttribute(name, value);
                    }
                } else {
                    action->setAttribute(name, value);
                }
                SCXML::Common::Logger::debug("ActionParser::parseActionNode() - Added attribute: " + name + " = " +
                                             value);
            }
        }
    }

    // 자식 요소를 단순 텍스트로 취급하는 대신 계층적으로 처리
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> childActions;
    auto children = actionNode->get_children();
    for (auto child : children) {
        auto textNode = dynamic_cast<const xmlpp::TextNode *>(child);
        if (textNode && !textNode->is_white_space()) {
            // 텍스트 내용이 있는 경우 처리
            action->setAttribute("textContent", textNode->get_content());

            // Special handling for ScriptActionNode content
            if (action->getType() == "script") {
                auto scriptAction = std::dynamic_pointer_cast<SCXML::Core::ScriptActionNode>(action);
                if (scriptAction) {
                    scriptAction->setContent(textNode->get_content());
                }
            }

            SCXML::Common::Logger::debug("ActionParser::parseActionNode() - Added text content");
        } else {
            auto elementNode = dynamic_cast<const xmlpp::Element *>(child);
            if (elementNode && isActionNode(elementNode)) {
                // 자식 요소가 액션 노드인 경우 재귀적으로 파싱
                auto childAction = parseActionNode(elementNode);
                if (childAction) {
                    childActions.push_back(childAction);
                    SCXML::Common::Logger::debug("ActionParser::parseActionNode() - Added child action: " +
                                                 childAction->getId());
                }
            }
        }
    }

    // 자식 액션 노드 추가
    if (!childActions.empty()) {
        action->setChildActions(childActions);
        SCXML::Common::Logger::debug("ActionParser::parseActionNode() - Added " + std::to_string(childActions.size()) +
                                     " child actions");
    }

    SCXML::Common::Logger::debug("ActionParser::parseActionNode() - Action parsed successfully");
    return action;
}

std::shared_ptr<SCXML::Model::IActionNode>
ActionParser::parseExternalActionNode(const xmlpp::Element *externalActionNode) {
    if (!externalActionNode) {
        SCXML::Common::Logger::warning("ActionParser::parseExternalActionNode() - Null external action node");
        return nullptr;
    }

    // 액션 ID(이름) 가져오기
    auto nameAttr = externalActionNode->get_attribute("name");

    // name 속성이 없는 경우 id 속성 시도
    if (!nameAttr) {
        nameAttr = externalActionNode->get_attribute("id");
    }

    if (!nameAttr) {
        SCXML::Common::Logger::warning(
            "ActionParser::parseExternalActionNode() - External action node missing required name attribute");
        return nullptr;
    }

    std::string id = nameAttr->get_value();
    SCXML::Common::Logger::debug("ActionParser::parseExternalActionNode() - Parsing external action: " + id);

    // 액션 노드 생성
    auto action = nodeFactory_->createActionNode(id);

    // 외부 액션 타입 설정
    action->setType("external");

    // 지연 시간 속성 처리
    auto delayAttr = externalActionNode->get_attribute("delay");
    if (delayAttr) {
        action->setAttribute("delay", delayAttr->get_value());
        SCXML::Common::Logger::debug("ActionParser::parseExternalActionNode() - Delay: " + delayAttr->get_value());
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
                SCXML::Common::Logger::debug("ActionParser::parseExternalActionNode() - Added attribute: " + name +
                                             " = " + value);
            }
        }
    }

    SCXML::Common::Logger::debug("ActionParser::parseExternalActionNode() - External action parsed successfully");
    return action;
}

std::vector<std::shared_ptr<SCXML::Model::IActionNode>>
ActionParser::parseActionsInElement(const xmlpp::Element *parentElement) {
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> actions;

    if (!parentElement) {
        SCXML::Common::Logger::warning("ActionParser::parseActionsInElement() - Null parent element");
        return actions;
    }

    SCXML::Common::Logger::debug("ActionParser::parseActionsInElement() - Parsing actions in element: " +
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
            SCXML::Common::Logger::debug("ActionParser::parseActionsInElement() - Found action node: " +
                                         element->get_name());
            auto action = parseActionNode(element);
            if (action) {
                SCXML::Common::Logger::debug("ActionParser::parseActionsInElement() - Successfully parsed action: " +
                                             action->getId());
                actions.push_back(action);
            } else {
                SCXML::Common::Logger::warning("ActionParser::parseActionsInElement() - Failed to parse action node: " +
                                               element->get_name());
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

    SCXML::Common::Logger::debug("ActionParser::parseActionsInElement() - Found " + std::to_string(actions.size()) +
                                 " actions");
    return actions;
}

void ActionParser::parseSpecialExecutableContent(const xmlpp::Element *element,
                                                 std::vector<std::shared_ptr<SCXML::Model::IActionNode>> &actions) {
    if (!element) {
        return;
    }

    std::string nodeName = element->get_name();
    SCXML::Common::Logger::debug("ActionParser::parseSpecialExecutableContent() - Parsing special content: " +
                                 nodeName);

    std::string actionType = getLocalName(nodeName);

    // W3C SCXML: <if> 요소는 특별한 파싱이 필요
    if (actionType == "if") {
        auto ifAction = parseIfElement(element);
        if (ifAction) {
            actions.push_back(ifAction);
        }
        return;
    }

    // 다른 특수 요소들에 대한 기본 처리
    auto specialAction = nodeFactory_->createActionNode(actionType);
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

    // 자식 액션 처리 (foreach 등)
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> childActions;
    auto children = element->get_children();

    for (auto child : children) {
        auto elementNode = dynamic_cast<const xmlpp::Element *>(child);
        if (elementNode) {
            if (isActionNode(elementNode) || isSpecialExecutableContent(elementNode)) {
                auto childAction = parseActionNode(elementNode);
                if (childAction) {
                    childActions.push_back(childAction);
                }
            }
        }
    }

    if (!childActions.empty()) {
        specialAction->setChildActions(childActions);
        SCXML::Common::Logger::debug("ActionParser::parseSpecialExecutableContent() - Added " +
                                     std::to_string(childActions.size()) + " child actions to " + nodeName);
    }

    actions.push_back(specialAction);
}

bool ActionParser::isActionNode(const xmlpp::Element *element) const {
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

bool ActionParser::isSpecialExecutableContent(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();

    // 특수 처리가 필요한 SCXML 실행 가능 콘텐츠
    return matchNodeName(nodeName, "if") || matchNodeName(nodeName, "elseif") || matchNodeName(nodeName, "else") ||
           matchNodeName(nodeName, "foreach") || matchNodeName(nodeName, "invoke") ||
           matchNodeName(nodeName, "finalize");
}

bool ActionParser::isExternalActionNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    return matchNodeName(nodeName, "external-action") || matchNodeName(nodeName, "code:external-action");
}

void ActionParser::parseExternalImplementation(const xmlpp::Element *element,
                                               std::shared_ptr<SCXML::Model::IActionNode> actionNode) {
    if (!element || !actionNode) {
        return;
    }

    SCXML::Common::Logger::debug(
        "ActionParser::parseExternalImplementation() - Parsing external implementation for action: " +
        actionNode->getId());

    auto classAttr = element->get_attribute("class");
    auto factoryAttr = element->get_attribute("factory");

    if (classAttr) {
        std::string className = classAttr->get_value();
        actionNode->setExternalClass(className);
        SCXML::Common::Logger::debug("ActionParser::parseExternalImplementation() - External class: " + className);
    }

    if (factoryAttr) {
        std::string factory = factoryAttr->get_value();
        actionNode->setExternalFactory(factory);
        SCXML::Common::Logger::debug("ActionParser::parseExternalImplementation() - External factory: " + factory);
    }
}

bool ActionParser::matchNodeName(const std::string &nodeName, const std::string &searchName) const {
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

std::string ActionParser::getLocalName(const std::string &nodeName) const {
    // 네임스페이스가 있는 경우 제거
    size_t colonPos = nodeName.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < nodeName.length()) {
        return nodeName.substr(colonPos + 1);
    }
    return nodeName;
}

std::shared_ptr<SCXML::Model::IActionNode> ActionParser::parseIfElement(const xmlpp::Element *element) {
    if (!element) {
        return nullptr;
    }

    SCXML::Common::Logger::debug("ActionParser::parseIfElement() - Parsing W3C SCXML <if> element");

    // W3C SCXML: IfActionNode 생성
    auto actionNode = nodeFactory_->createActionNode("if");
    auto ifAction = std::dynamic_pointer_cast<SCXML::Core::IfActionNode>(actionNode);

    if (!ifAction) {
        SCXML::Common::Logger::error("ActionParser::parseIfElement() - Failed to create IfActionNode");
        return nullptr;
    }

    ifAction->setType("if");

    // W3C SCXML: <if> 요소의 'cond' 속성 처리
    auto condAttr = element->get_attribute("cond");
    if (condAttr) {
        std::string condition = condAttr->get_value();
        ifAction->setIfCondition(condition);
        SCXML::Common::Logger::debug("ActionParser::parseIfElement() - If condition: " + condition);
    } else {
        SCXML::Common::Logger::warning("ActionParser::parseIfElement() - Missing 'cond' attribute in <if> element");
    }

    // W3C SCXML: <if> 요소의 자식 요소들을 브랜치로 파싱
    auto children = element->get_children();

    enum ParseState { IF_ACTIONS, ELSEIF_ACTIONS, ELSE_ACTIONS };

    ParseState currentState = IF_ACTIONS;
    size_t currentBranchIndex = 0;  // 현재 브랜치 인덱스 (0 = if, 1+ = elseif/else)

    for (auto child : children) {
        auto elementNode = dynamic_cast<const xmlpp::Element *>(child);
        if (!elementNode) {
            continue;
        }

        std::string childName = getLocalName(elementNode->get_name());

        if (childName == "elseif") {
            // W3C SCXML: <elseif> 브랜치 처리
            auto condAttr = elementNode->get_attribute("cond");
            if (condAttr) {
                std::string elseifCondition = condAttr->get_value();
                ifAction->addElseIfBranch(elseifCondition);
                currentBranchIndex = ifAction->getBranches().size() - 1;  // 방금 추가된 브랜치
                currentState = ELSEIF_ACTIONS;
                SCXML::Common::Logger::debug("ActionParser::parseIfElement() - ElseIf condition: " + elseifCondition);
            }
        } else if (childName == "else") {
            // W3C SCXML: <else> 브랜치 처리
            ifAction->addElseBranch();
            currentBranchIndex = ifAction->getBranches().size() - 1;  // 방금 추가된 브랜치
            currentState = ELSE_ACTIONS;
            SCXML::Common::Logger::debug("ActionParser::parseIfElement() - Processing else branch");
        } else {
            // W3C SCXML: 일반 액션 요소 처리
            if (isActionNode(elementNode) || isSpecialExecutableContent(elementNode)) {
                auto childAction = parseActionNode(elementNode);
                if (childAction) {
                    // 현재 상태에 따라 액션을 적절한 브랜치에 추가
                    switch (currentState) {
                    case IF_ACTIONS:
                        ifAction->addIfAction(childAction);
                        break;
                    case ELSEIF_ACTIONS:
                    case ELSE_ACTIONS:
                        ifAction->addActionToBranch(currentBranchIndex, childAction);
                        break;
                    }
                    SCXML::Common::Logger::debug(
                        "ActionParser::parseIfElement() - Added action: " + childAction->getActionType() + " to " +
                        (currentState == IF_ACTIONS       ? "if"
                         : currentState == ELSEIF_ACTIONS ? "elseif"
                                                          : "else") +
                        " branch");
                }
            }
        }
    }

    SCXML::Common::Logger::debug("ActionParser::parseIfElement() - Completed parsing <if> element with " +
                                 std::to_string(ifAction->getBranches().size()) + " total branches");

    return ifAction;
}
