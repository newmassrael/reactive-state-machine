#include "parsing/TransitionParser.h"
#include "common/Logger.h"
#include "parsing/ParsingCommon.h"
#include <algorithm>
#include <sstream>

RSM::TransitionParser::TransitionParser(std::shared_ptr<RSM::INodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    Logger::debug("RSM::TransitionParser::Constructor - Creating transition parser");
}

RSM::TransitionParser::~TransitionParser() {
    Logger::debug("RSM::TransitionParser::Destructor - Destroying transition parser");
}

void RSM::TransitionParser::setActionParser(std::shared_ptr<RSM::ActionParser> actionParser) {
    actionParser_ = actionParser;
    Logger::debug("RSM::TransitionParser::setActionParser() - Action parser set");
}

std::shared_ptr<RSM::ITransitionNode> RSM::TransitionParser::parseTransitionNode(const xmlpp::Element *transElement,
                                                                                 RSM::IStateNode *stateNode) {
    if (!transElement || !stateNode) {
        Logger::warn("RSM::TransitionParser::parseTransitionNode() - Null "
                     "transition element or state node");
        return nullptr;
    }

    auto eventAttr = transElement->get_attribute("event");
    auto targetAttr = transElement->get_attribute("target");

    std::string event = eventAttr ? eventAttr->get_value() : "";
    std::string target = targetAttr ? targetAttr->get_value() : "";

    Logger::debug("RSM::TransitionParser::parseTransitionNode() - Parsing transition: " +
                  (event.empty() ? "<no event>" : event) + " -> " + (target.empty() ? "<internal>" : target));

    // target이 비어있는 경우 내부 전환으로 처리
    bool isInternal = target.empty();

    // 전환 노드 생성
    std::shared_ptr<RSM::ITransitionNode> transition;

    if (isInternal) {
        Logger::debug("RSM::TransitionParser::parseTransitionNode() - Internal "
                      "transition detected (no target)");

        // 빈 타겟으로 전환 생성
        transition = nodeFactory_->createTransitionNode(event, "");

        // 명시적으로 타겟 목록 비우기
        transition->clearTargets();

        Logger::debug("RSM::TransitionParser::parseTransitionNode() - After "
                      "clearTargets() - targets count: " +
                      std::to_string(transition->getTargets().size()));
    } else {
        // 초기화 시 빈 문자열로 생성
        transition = nodeFactory_->createTransitionNode(event, "");

        // 기존 타겟 목록을 비우고 시작
        transition->clearTargets();

        // 공백으로 구분된 타겟 문자열 파싱
        std::stringstream ss(target);
        std::string targetId;

        // 개별 타겟 추가
        while (ss >> targetId) {
            if (!targetId.empty()) {
                transition->addTarget(targetId);
                Logger::debug("RSM::TransitionParser::parseTransitionNode() - Added target: " + targetId);
            }
        }
    }

    // 내부 전환 설정
    transition->setInternal(isInternal);

    // 타입 속성 처리
    auto typeAttr = transElement->get_attribute("type");
    if (typeAttr) {
        std::string type = typeAttr->get_value();
        transition->setAttribute("type", type);
        Logger::debug("RSM::TransitionParser::parseTransitionNode() - Type: " + type);

        // type이 "internal"인 경우 내부 전환으로 설정
        if (type == "internal") {
            transition->setInternal(true);
            isInternal = true;  // isInternal 변수 업데이트
        }
    }

    // 조건 속성 처리
    auto condAttr = transElement->get_attribute("cond");
    if (condAttr) {
        std::string cond = condAttr->get_value();
        transition->setAttribute("cond", cond);
        transition->setGuard(cond);
        Logger::debug("RSM::TransitionParser::parseTransitionNode() - Condition: " + cond);
    }

    // 가드 속성 처리
    std::string guard = ParsingCommon::getAttributeValue(transElement, {"guard"});
    if (!guard.empty()) {
        transition->setGuard(guard);
        Logger::debug("RSM::TransitionParser::parseTransitionNode() - Guard: " + guard);
    }

    // 이벤트 목록 파싱
    if (!event.empty()) {
        auto events = parseEventList(event);
        for (const auto &eventName : events) {
            transition->addEvent(eventName);
            Logger::debug("RSM::TransitionParser::parseTransitionNode() - Added event: " + eventName);
        }
    }

    // 액션 파싱
    parseActions(transElement, transition);

    Logger::debug("RSM::TransitionParser::parseTransitionNode() - Transition "
                  "parsed successfully with " +
                  std::to_string(transition->getActionNodes().size()) + " ActionNodes");
    return transition;
}

std::shared_ptr<RSM::ITransitionNode>
RSM::TransitionParser::parseInitialTransition(const xmlpp::Element *initialElement) {
    if (!initialElement) {
        Logger::warn("RSM::TransitionParser::parseInitialTransition() - Null "
                     "initial element");
        return nullptr;
    }

    Logger::debug("RSM::TransitionParser::parseInitialTransition() - Parsing "
                  "initial transition");

    // initial 요소 내의 transition 요소 찾기
    auto transElement = ParsingCommon::findFirstChildElement(initialElement, "transition");
    if (!transElement) {
        Logger::warn("RSM::TransitionParser::parseInitialTransition() - No "
                     "transition element found in initial");
        return nullptr;
    }

    auto targetAttr = transElement->get_attribute("target");
    if (!targetAttr) {
        Logger::warn("RSM::TransitionParser::parseInitialTransition() - Initial "
                     "transition missing target attribute");
        return nullptr;
    }

    std::string target = targetAttr->get_value();
    Logger::debug("RSM::TransitionParser::parseInitialTransition() - Initial "
                  "transition target: " +
                  target);

    // 초기 전환 생성 - 이벤트 없음
    auto transition = nodeFactory_->createTransitionNode("", target);

    // 특수 속성 설정
    transition->setAttribute("initial", "true");

    // 액션 파싱
    parseActions(transElement, transition);

    Logger::debug("RSM::TransitionParser::parseInitialTransition() - Initial "
                  "transition parsed successfully");
    return transition;
}

std::vector<std::shared_ptr<RSM::ITransitionNode>>
RSM::TransitionParser::parseTransitionsInState(const xmlpp::Element *stateElement, RSM::IStateNode *stateNode) {
    std::vector<std::shared_ptr<RSM::ITransitionNode>> transitions;

    if (!stateElement || !stateNode) {
        Logger::warn("RSM::TransitionParser::parseTransitionsInState() - Null "
                     "state element or node");
        return transitions;
    }

    Logger::debug("RSM::TransitionParser::parseTransitionsInState() - Parsing "
                  "transitions in state: " +
                  stateNode->getId());

    // 모든 transition 요소 찾기
    auto transElements = ParsingCommon::findChildElements(stateElement, "transition");
    for (auto *transElement : transElements) {
        auto transition = parseTransitionNode(transElement, stateNode);
        if (transition) {
            transitions.push_back(transition);
        }
    }

    Logger::debug("RSM::TransitionParser::parseTransitionsInState() - Found " + std::to_string(transitions.size()) +
                  " transitions");
    return transitions;
}

bool RSM::TransitionParser::isTransitionNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    return matchNodeName(nodeName, "transition");
}

void RSM::TransitionParser::parseActions(const xmlpp::Element *transElement,
                                         std::shared_ptr<RSM::ITransitionNode> transition) {
    if (!transElement || !transition) {
        return;
    }

    // ActionParser는 SCXML 파싱에 필수
    if (!actionParser_) {
        assert(false && "ActionParser is required for SCXML compliance");
        return;
    }

    {
        // SCXML 사양 준수: ActionNode 객체를 직접 저장
        auto actionNodes = actionParser_->parseActionsInElement(transElement);
        for (const auto &actionNode : actionNodes) {
            if (actionNode) {
                transition->addActionNode(actionNode);
                Logger::debug("RSM::TransitionParser::parseActions() - Added ActionNode: " +
                              actionNode->getActionType());
            }
        }
    }
}

std::vector<std::string> RSM::TransitionParser::parseEventList(const std::string &eventStr) const {
    std::vector<std::string> events;
    std::stringstream ss(eventStr);
    std::string event;

    // 공백으로 구분된 이벤트 목록 파싱
    while (std::getline(ss, event, ' ')) {
        // 빈 문자열 제거
        if (!event.empty()) {
            events.push_back(event);
        }
    }

    return events;
}

bool RSM::TransitionParser::matchNodeName(const std::string &nodeName, const std::string &searchName) const {
    return ParsingCommon::matchNodeName(nodeName, searchName);
}

std::vector<std::string> RSM::TransitionParser::parseTargetList(const std::string &targetStr) const {
    std::vector<std::string> targets;
    std::stringstream ss(targetStr);
    std::string target;

    // 공백으로 구분된 타겟 목록 파싱
    while (std::getline(ss, target, ' ')) {
        // 빈 문자열 제거
        if (!target.empty()) {
            targets.push_back(target);
        }
    }

    return targets;
}
