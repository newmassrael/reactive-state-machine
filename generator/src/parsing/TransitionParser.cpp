
#include "parsing/TransitionParser.h"
#include "common/Logger.h"
#include "parsing/ParsingCommon.h"
#include <algorithm>
#include <sstream>

using namespace SCXML::Parsing;
using namespace std;

TransitionParser::TransitionParser(std::shared_ptr<SCXML::Model::INodeFactory> nodeFactory)
    : nodeFactory_(nodeFactory) {
    SCXML::Common::Logger::debug("TransitionParser::Constructor - Creating transition parser");
}

TransitionParser::~TransitionParser() {
    SCXML::Common::Logger::debug("TransitionParser::Destructor - Destroying transition parser");
}

void TransitionParser::setActionParser(std::shared_ptr<ActionParser> actionParser) {
    actionParser_ = actionParser;
    SCXML::Common::Logger::debug("TransitionParser::setActionParser() - Action parser set");
}

std::shared_ptr<SCXML::Model::ITransitionNode>
TransitionParser::parseTransitionNode(const xmlpp::Element *transElement, SCXML::Model::IStateNode * /*stateNode*/) {
    if (!transElement) {
        SCXML::Common::Logger::warning("TransitionParser::parseTransitionNode() - Null transition element");
        return nullptr;
    }

    // Note: stateNode can be nullptr for document-level transitions

    auto eventAttr = transElement->get_attribute("event");
    auto targetAttr = transElement->get_attribute("target");

    std::string event = eventAttr ? eventAttr->get_value() : "";
    std::string target = targetAttr ? targetAttr->get_value() : "";

    SCXML::Common::Logger::debug(
        "TransitionParser::parseTransitionNode() - Parsing transition: " + (event.empty() ? "<no event>" : event) +
        " -> " + (target.empty() ? "<internal>" : target));

    // target이 비어있는 경우 내부 전환으로 처리
    bool isInternal = target.empty();

    // 전환 노드 생성
    std::shared_ptr<SCXML::Model::ITransitionNode> transition;

    if (isInternal) {
        SCXML::Common::Logger::debug(
            "TransitionParser::parseTransitionNode() - Internal transition detected (no target)");

        // 빈 타겟으로 전환 생성
        transition = nodeFactory_->createTransitionNode(event, "");

        // 명시적으로 타겟 목록 비우기
        transition->clearTargets();

        SCXML::Common::Logger::debug(
            "TransitionParser::parseTransitionNode() - After clearTargets() - targets count: " +
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
                SCXML::Common::Logger::debug("TransitionParser::parseTransitionNode() - Added target: " + targetId);
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
        SCXML::Common::Logger::debug("TransitionParser::parseTransitionNode() - Type: " + type);

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
        SCXML::Common::Logger::debug("TransitionParser::parseTransitionNode() - Condition: " + cond);
    }

    // 가드 속성 처리
    std::string guard = SCXML::Parsing::ParsingCommon::getAttributeValue(transElement, {"guard"});
    if (!guard.empty()) {
        transition->setGuard(guard);
        SCXML::Common::Logger::debug("TransitionParser::parseTransitionNode() - Guard: " + guard);
    }

    // 이벤트 목록 파싱
    if (!event.empty()) {
        auto events = parseEventList(event);
        for (const auto &eventName : events) {
            transition->addEvent(eventName);
            SCXML::Common::Logger::debug("TransitionParser::parseTransitionNode() - Added event: " + eventName);
        }
    }

    // 액션 파싱
    parseActions(transElement, transition);

    SCXML::Common::Logger::debug("TransitionParser::parseTransitionNode() - Transition parsed successfully with " +
                                 std::to_string(transition->getActions().size()) + " actions");
    return transition;
}

std::shared_ptr<SCXML::Model::ITransitionNode>
TransitionParser::parseInitialTransition(const xmlpp::Element *initialElement) {
    if (!initialElement) {
        SCXML::Common::Logger::warning("TransitionParser::parseInitialTransition() - Null initial element");
        return nullptr;
    }

    SCXML::Common::Logger::debug("TransitionParser::parseInitialTransition() - Parsing initial transition");

    // initial 요소 내의 transition 요소 찾기
    auto transElement = SCXML::Parsing::ParsingCommon::findFirstChildElement(initialElement, "transition");
    if (!transElement) {
        SCXML::Common::Logger::warning(
            "TransitionParser::parseInitialTransition() - No transition element found in initial");
        return nullptr;
    }

    auto targetAttr = transElement->get_attribute("target");
    if (!targetAttr) {
        SCXML::Common::Logger::warning(
            "TransitionParser::parseInitialTransition() - Initial transition missing target attribute");
        return nullptr;
    }

    std::string target = targetAttr->get_value();
    SCXML::Common::Logger::debug("TransitionParser::parseInitialTransition() - Initial transition target: " + target);

    // 초기 전환 생성 - 이벤트 없음
    auto transition = nodeFactory_->createTransitionNode("", target);

    // 특수 속성 설정
    transition->setAttribute("initial", "true");

    // 액션 파싱
    parseActions(transElement, transition);

    SCXML::Common::Logger::debug("TransitionParser::parseInitialTransition() - Initial transition parsed successfully");
    return transition;
}

std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>>
TransitionParser::parseTransitionsInState(const xmlpp::Element *stateElement, SCXML::Model::IStateNode *stateNode) {
    std::vector<std::shared_ptr<SCXML::Model::ITransitionNode>> transitions;

    if (!stateElement || !stateNode) {
        SCXML::Common::Logger::warning("TransitionParser::parseTransitionsInState() - Null state element or node");
        return transitions;
    }

    SCXML::Common::Logger::debug("TransitionParser::parseTransitionsInState() - Parsing transitions in state: " +
                                 stateNode->getId());

    // 모든 transition 요소 찾기
    auto transElements = SCXML::Parsing::ParsingCommon::findChildElements(stateElement, "transition");
    for (auto *transElement : transElements) {
        auto transition = parseTransitionNode(transElement, stateNode);
        if (transition) {
            transitions.push_back(transition);
        }
    }

    SCXML::Common::Logger::debug("TransitionParser::parseTransitionsInState() - Found " +
                                 std::to_string(transitions.size()) + " transitions");
    return transitions;
}

bool TransitionParser::isTransitionNode(const xmlpp::Element *element) const {
    if (!element) {
        return false;
    }

    std::string nodeName = element->get_name();
    return matchNodeName(nodeName, "transition");
}

void TransitionParser::parseActions(const xmlpp::Element *transElement,
                                    std::shared_ptr<SCXML::Model::ITransitionNode> transition) {
    SCXML::Common::Logger::debug("TransitionParser::parseActions() - Starting action parsing");
    if (!transElement || !transition) {
        return;
    }

    // 디버그: ActionParser 상태 확인
    SCXML::Common::Logger::debug(std::string("TransitionParser::parseActions() - ActionParser state: ") +
                                 (actionParser_ ? "SET" : "NULL"));

    // ActionParser를 사용하여 액션 파싱
    if (actionParser_) {
        SCXML::Common::Logger::debug("TransitionParser::parseActions() - Using ActionParser to parse actions");
        auto actions = actionParser_->parseActionsInElement(transElement);
        for (const auto &action : actions) {
            // Store both ActionNode object and ID for backward compatibility
            transition->addActionNode(action);
            SCXML::Common::Logger::debug("TransitionParser::parseActions() - Added parsed action node: " +
                                         action->getId());
        }
        SCXML::Common::Logger::debug("TransitionParser::parseActions() - ActionParser found " +
                                     std::to_string(actions.size()) + " actions");
    } else {
        SCXML::Common::Logger::error(
            "TransitionParser::parseActions() - ActionParser not set! This should not happen.");
        // ActionParser가 설정되지 않은 경우는 시스템 오류
        return;
    }
}

std::vector<std::string> TransitionParser::parseEventList(const std::string &eventStr) const {
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

bool TransitionParser::matchNodeName(const std::string &nodeName, const std::string &searchName) const {
    return SCXML::Parsing::ParsingCommon::matchNodeName(nodeName, searchName);
}

std::vector<std::string> TransitionParser::parseTargetList(const std::string &targetStr) const {
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
