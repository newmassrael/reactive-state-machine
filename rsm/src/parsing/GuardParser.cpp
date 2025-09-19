#include "parsing/GuardParser.h"
#include "GuardUtils.h"
#include "ParsingCommon.h"
#include "common/Logger.h"
#include <algorithm>

RSM::GuardParser::GuardParser(std::shared_ptr<RSM::INodeFactory> nodeFactory)
    : nodeFactory_(nodeFactory) {
  Logger::debug("GuardParser::Constructor - Creating guard parser");
}

RSM::GuardParser::~GuardParser() {
  Logger::debug("GuardParser::Destructor - Destroying guard parser");
}

std::shared_ptr<RSM::IGuardNode>
RSM::GuardParser::parseGuardNode(const xmlpp::Element *guardNode) {
  if (!guardNode) {
    Logger::warn("GuardParser::parseGuardNode() - Null guard node");
    return nullptr;
  }

  auto idAttr = guardNode->get_attribute("id");
  auto targetAttr = guardNode->get_attribute("target");
  auto conditionAttr = guardNode->get_attribute("condition");

  // id와 target/condition이 없는 경우 다른 속성 이름을 시도
  if (!idAttr) {
    idAttr = guardNode->get_attribute("name");
  }

  if (!targetAttr && !conditionAttr) {
    targetAttr = guardNode->get_attribute("to");
  }

  if (!idAttr || (!targetAttr && !conditionAttr)) {
    Logger::warn("GuardParser::parseGuardNode() - Guard node missing required "
                 "attributes");
    Logger::debug("GuardParser::parseGuardNode() - Node name: " +
                  guardNode->get_name());
    // 디버깅을 위해 가용한 모든 속성 출력
    auto attrs = guardNode->get_attributes();
    for (auto *attr : attrs) {
      auto *xmlAttr = dynamic_cast<const xmlpp::Attribute *>(attr);
      if (xmlAttr) {
        Logger::debug("GuardParser::parseGuardNode() - Attribute: " +
                      xmlAttr->get_name() + " = " + xmlAttr->get_value());
      }
    }
    return nullptr;
  }

  std::string id = idAttr->get_value();

  // 기본 가드 노드 생성 - 빈 문자열로 초기화
  auto guard = nodeFactory_->createGuardNode(id, "");

  // target 속성 처리
  if (targetAttr) {
    std::string target = targetAttr->get_value();
    Logger::debug("GuardParser::parseGuardNode() - Guard: " + id +
                  " with target attribute: " + target);

    if (GuardUtils::isConditionExpression(target)) {
      // target이 조건식인 경우
      guard->setCondition(target);
      Logger::debug(
          "GuardParser::parseGuardNode() - Set condition from target: " +
          target);
    } else {
      // target이 상태 ID인 경우
      guard->setTargetState(target);
      Logger::debug("GuardParser::parseGuardNode() - Set target state: " +
                    target);
    }
  }

  // condition 속성 처리
  if (conditionAttr) {
    std::string condition = conditionAttr->get_value();
    guard->setCondition(condition);
    Logger::debug(
        "GuardParser::parseGuardNode() - Set condition from attribute: " +
        condition);
  }

  // <code:condition> 또는 <condition> 요소 처리
  auto conditionNode = guardNode->get_first_child("code:condition");
  if (!conditionNode) {
    conditionNode = guardNode->get_first_child("condition");
  }

  if (conditionNode) {
    // <code:condition> 또는 <condition> 요소 처리
    auto conditionElement =
        RSM::ParsingCommon::findFirstChildElement(guardNode, "condition");
    if (conditionElement) {
      Logger::debug("GuardParser::parseGuardNode() - Found condition element");

      // CDATA 섹션과 일반 텍스트 모두 처리
      std::string conditionText =
          RSM::ParsingCommon::extractTextContent(conditionElement, true);
      Logger::debug("GuardParser::parseGuardNode() - Raw condition content: '" +
                    conditionText + "'");

      if (!conditionText.empty()) {
        guard->setCondition(conditionText);
        Logger::debug(
            "GuardParser::parseGuardNode() - Set condition from element: " +
            conditionText);
      }
    }
  }

  // 의존성 파싱
  parseDependencies(guardNode, guard);

  // 외부 구현 파싱
  parseExternalImplementation(guardNode, guard);

  Logger::debug("GuardParser::parseGuardNode() - Guard parsed successfully");
  return guard;
}

std::shared_ptr<RSM::IGuardNode>
RSM::GuardParser::parseGuardFromTransition(const xmlpp::Element *transitionNode,
                                           const std::string &targetState) {
  if (!transitionNode) {
    Logger::warn(
        "GuardParser::parseGuardFromTransition() - Null transition node");
    return nullptr;
  }

  // guard 속성을 네임스페이스 접두사를 고려하여 찾기
  auto guardAttr = transitionNode->get_attribute("guard", "code");
  if (!guardAttr) {
    // 네임스페이스 없이 시도
    guardAttr = transitionNode->get_attribute("guard");
  }

  if (!guardAttr) {
    // 가드 속성이 없는 경우
    return nullptr;
  }

  std::string guardId = guardAttr->get_value();

  Logger::debug("GuardParser::parseGuardFromTransition() - Parsing guard from "
                "transition: " +
                guardId + " for state: " + targetState);

  // 기본 가드 노드 생성 - 빈 문자열로 초기화
  auto guard = nodeFactory_->createGuardNode(guardId, "");

  // 명확하게 타겟 상태 설정
  guard->setTargetState(targetState);

  // cond 속성이 있는지 확인
  auto condAttr = transitionNode->get_attribute("cond");
  if (condAttr) {
    std::string condition = condAttr->get_value();
    guard->setCondition(condition);
    Logger::debug("GuardParser::parseGuardFromTransition() - Set condition "
                  "from cond attribute: " +
                  condition);
  }

  Logger::debug("GuardParser::parseGuardFromTransition() - Guard from "
                "transition parsed successfully");
  return guard;
}

std::shared_ptr<RSM::IGuardNode>
RSM::GuardParser::parseReactiveGuard(const xmlpp::Element *reactiveGuardNode) {
  if (!reactiveGuardNode) {
    Logger::warn(
        "GuardParser::parseReactiveGuard() - Null reactive guard node");
    return nullptr;
  }

  auto idAttr = reactiveGuardNode->get_attribute("id");
  auto targetAttr = reactiveGuardNode->get_attribute("target");
  auto conditionAttr = reactiveGuardNode->get_attribute("condition");

  if (!idAttr || (!targetAttr && !conditionAttr)) {
    Logger::warn("GuardParser::parseReactiveGuard() - Reactive guard node "
                 "missing required attributes");
    return nullptr;
  }

  std::string id = idAttr->get_value();

  // 기본 가드 노드 생성 - 빈 문자열로 초기화
  auto guard = nodeFactory_->createGuardNode(id, "");

  // 반응형 속성 설정
  guard->setReactive(true);
  guard->setAttribute("reactive", "true");

  // target 속성 처리
  if (targetAttr) {
    std::string target = targetAttr->get_value();
    Logger::debug("GuardParser::parseReactiveGuard() - Reactive guard: " + id +
                  " with target: " + target);

    if (GuardUtils::isConditionExpression(target)) {
      // target이 조건식인 경우
      guard->setCondition(target);
      Logger::debug(
          "GuardParser::parseReactiveGuard() - Set condition from target: " +
          target);
    } else {
      // target이 상태 ID인 경우
      guard->setTargetState(target);
      Logger::debug("GuardParser::parseReactiveGuard() - Set target state: " +
                    target);
    }
  }

  // condition 속성 처리
  if (conditionAttr) {
    std::string condition = conditionAttr->get_value();
    guard->setCondition(condition);
    Logger::debug(
        "GuardParser::parseReactiveGuard() - Set condition from attribute: " +
        condition);
  }

  // 의존성 파싱
  parseDependencies(reactiveGuardNode, guard);

  // 외부 구현 파싱
  parseExternalImplementation(reactiveGuardNode, guard);

  Logger::debug(
      "GuardParser::parseReactiveGuard() - Reactive guard parsed successfully");
  return guard;
}

std::vector<std::shared_ptr<RSM::IGuardNode>>
RSM::GuardParser::parseGuardsElement(const xmlpp::Element *guardsNode) {
  std::vector<std::shared_ptr<RSM::IGuardNode>> guards;

  if (!guardsNode) {
    Logger::warn("GuardParser::parseGuardsElement() - Null guards node");
    return guards;
  }

  Logger::debug("GuardParser::parseGuardsElement() - Parsing guards element");

  // guard 노드들 파싱
  auto guardNodes = guardsNode->get_children("code:guard");
  if (guardNodes.empty()) {
    // 네임스페이스 없이 시도
    guardNodes = guardsNode->get_children("guard");
  }

  for (auto *node : guardNodes) {
    auto *guardElement = dynamic_cast<const xmlpp::Element *>(node);
    if (guardElement) {
      auto guard = parseGuardNode(guardElement);
      if (guard) {
        guards.push_back(guard);
        Logger::debug("GuardParser::parseGuardsElement() - Added guard: " +
                      guard->getId());
      }
    }
  }

  Logger::debug("GuardParser::parseGuardsElement() - Parsed " +
                std::to_string(guards.size()) + " guards");
  return guards;
}

std::vector<std::shared_ptr<RSM::IGuardNode>>
RSM::GuardParser::parseAllGuards(const xmlpp::Element *scxmlNode) {
  std::vector<std::shared_ptr<RSM::IGuardNode>> allGuards;

  if (!scxmlNode) {
    Logger::warn("GuardParser::parseAllGuards() - Null SCXML node");
    return allGuards;
  }

  Logger::debug(
      "GuardParser::parseAllGuards() - Parsing all guards in SCXML document");

  // 1. code:guards 요소 내의 가드 파싱
  auto guardsNode = scxmlNode->get_first_child("code:guards");
  if (!guardsNode) {
    // 네임스페이스 없이 시도
    guardsNode = scxmlNode->get_first_child("guards");
  }

  if (guardsNode) {
    auto *element = dynamic_cast<const xmlpp::Element *>(guardsNode);
    if (element) {
      auto guards = parseGuardsElement(element);
      allGuards.insert(allGuards.end(), guards.begin(), guards.end());
    }
  }

  // 2. 모든 상태의 전환에서 가드 속성 찾기
  std::vector<const xmlpp::Node *> stateNodes;

  // 모든 상태 노드 수집 (상태, 병렬, 최종 포함)
  auto states = scxmlNode->get_children("state");
  stateNodes.insert(stateNodes.end(), states.begin(), states.end());

  auto parallels = scxmlNode->get_children("parallel");
  stateNodes.insert(stateNodes.end(), parallels.begin(), parallels.end());

  auto finals = scxmlNode->get_children("final");
  stateNodes.insert(stateNodes.end(), finals.begin(), finals.end());

  // 각 상태의 전환 요소에서 가드 속성 확인
  for (auto *stateNode : stateNodes) {
    auto *stateElement = dynamic_cast<const xmlpp::Element *>(stateNode);
    if (stateElement) {
      // 상태 ID 가져오기
      auto idAttr = stateElement->get_attribute("id");
      if (!idAttr)
        continue;

      std::string stateId = idAttr->get_value();

      // 전환 요소 처리
      auto transNodes = stateElement->get_children("transition");
      for (auto *transNode : transNodes) {
        auto *transElement = dynamic_cast<const xmlpp::Element *>(transNode);
        if (transElement) {
          auto targetAttr = transElement->get_attribute("target");
          if (targetAttr) {
            std::string target = targetAttr->get_value();
            auto guard = parseGuardFromTransition(transElement, target);
            if (guard) {
              allGuards.push_back(guard);
              Logger::debug("GuardParser::parseAllGuards() - Added guard from "
                            "transition in state " +
                            stateId);
            }
          }
        }
      }

      // 반응형 가드 처리
      auto reactiveGuardNodes =
          stateElement->get_children("code:reactive-guard");
      if (reactiveGuardNodes.empty()) {
        // 네임스페이스 없이 시도
        reactiveGuardNodes = stateElement->get_children("reactive-guard");
      }

      for (auto *node : reactiveGuardNodes) {
        auto *guardElement = dynamic_cast<const xmlpp::Element *>(node);
        if (guardElement) {
          auto guard = parseReactiveGuard(guardElement);
          if (guard) {
            allGuards.push_back(guard);
            Logger::debug("GuardParser::parseAllGuards() - Added reactive "
                          "guard from state " +
                          stateId);
          }
        }
      }
    }
  }

  // 3. 중복 제거 (ID 기준)
  std::sort(allGuards.begin(), allGuards.end(),
            [](const std::shared_ptr<RSM::IGuardNode> &a,
               const std::shared_ptr<RSM::IGuardNode> &b) {
              return a->getId() < b->getId();
            });

  allGuards.erase(std::unique(allGuards.begin(), allGuards.end(),
                              [](const std::shared_ptr<RSM::IGuardNode> &a,
                                 const std::shared_ptr<RSM::IGuardNode> &b) {
                                return a->getId() == b->getId();
                              }),
                  allGuards.end());

  Logger::debug("GuardParser::parseAllGuards() - Found " +
                std::to_string(allGuards.size()) + " unique guards");
  return allGuards;
}

bool RSM::GuardParser::isGuardNode(const xmlpp::Element *element) const {
  if (!element) {
    return false;
  }

  std::string nodeName = element->get_name();
  return RSM::ParsingCommon::matchNodeName(nodeName, "guard");
}

bool RSM::GuardParser::isReactiveGuardNode(
    const xmlpp::Element *element) const {
  if (!element) {
    return false;
  }

  std::string nodeName = element->get_name();
  return RSM::ParsingCommon::matchNodeName(nodeName, "reactive-guard");
}

void RSM::GuardParser::parseDependencies(
    const xmlpp::Element *guardNode,
    std::shared_ptr<RSM::IGuardNode> guardObject) {
  if (!guardNode || !guardObject) {
    return;
  }

  // 의존성 파싱
  auto depNodes = guardNode->get_children("code:dependency");
  if (depNodes.empty()) {
    // 네임스페이스 없이 시도
    depNodes = guardNode->get_children("dependency");
  }

  for (auto *node : depNodes) {
    auto *element = dynamic_cast<const xmlpp::Element *>(node);
    if (element) {
      auto propAttr = element->get_attribute("property");
      if (!propAttr) {
        propAttr = element->get_attribute("prop"); // 대체 속성 이름 시도
      }

      if (propAttr) {
        std::string property = propAttr->get_value();
        guardObject->addDependency(property);
        Logger::debug("GuardParser::parseDependencies() - Added dependency: " +
                      property);
      }
    }
  }
}

void RSM::GuardParser::parseExternalImplementation(
    const xmlpp::Element *guardNode,
    std::shared_ptr<RSM::IGuardNode> guardObject) {
  if (!guardNode || !guardObject) {
    return;
  }

  auto implNode = guardNode->get_first_child("code:external-implementation");
  if (!implNode) {
    // 네임스페이스 없이 시도
    implNode = guardNode->get_first_child("external-implementation");
  }

  if (implNode) {
    auto *element = dynamic_cast<const xmlpp::Element *>(implNode);
    if (element) {
      auto classAttr = element->get_attribute("class");
      auto factoryAttr = element->get_attribute("factory");

      if (classAttr) {
        std::string className = classAttr->get_value();
        guardObject->setExternalClass(className);
        Logger::debug(
            "GuardParser::parseExternalImplementation() - External class: " +
            className);
      }

      if (factoryAttr) {
        std::string factory = factoryAttr->get_value();
        guardObject->setExternalFactory(factory);
        Logger::debug(
            "GuardParser::parseExternalImplementation() - External factory: " +
            factory);
      }
    }
  }
}

bool RSM::GuardParser::matchNodeName(const std::string &nodeName,
                                     const std::string &searchName) const {
  // 정확히 일치하는 경우
  if (nodeName == searchName) {
    return true;
  }

  // 네임스페이스가 있는 경우 (예: "code:guard")
  size_t colonPos = nodeName.find(':');
  if (colonPos != std::string::npos && colonPos + 1 < nodeName.length()) {
    std::string localName = nodeName.substr(colonPos + 1);
    return localName == searchName;
  }

  return false;
}
