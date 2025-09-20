#include "parsing/StateNodeParser.h"
#include "actions/AssignAction.h"
#include "actions/ScriptAction.h"
#include "common/Logger.h"
#include "parsing/ActionParser.h"
#include "parsing/DataModelParser.h"
#include "parsing/DoneDataParser.h"
#include "parsing/InvokeParser.h"
#include "parsing/ParsingCommon.h"
#include "parsing/TransitionParser.h"

RSM::StateNodeParser::StateNodeParser(std::shared_ptr<RSM::INodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    Logger::debug("RSM::StateNodeParser::Constructor - Creating state node parser");
}

RSM::StateNodeParser::~StateNodeParser() {
    Logger::debug("RSM::StateNodeParser::Destructor - Destroying state node parser");
}

void RSM::StateNodeParser::setRelatedParsers(std::shared_ptr<TransitionParser> transitionParser,
                                             std::shared_ptr<ActionParser> actionParser,
                                             std::shared_ptr<DataModelParser> dataModelParser,
                                             std::shared_ptr<InvokeParser> invokeParser,
                                             std::shared_ptr<DoneDataParser> doneDataParser) {
    transitionParser_ = transitionParser;
    actionParser_ = actionParser;
    dataModelParser_ = dataModelParser;
    invokeParser_ = invokeParser;
    doneDataParser_ = doneDataParser;

    Logger::debug("RSM::StateNodeParser::setRelatedParsers() - Related parsers set");
}

std::shared_ptr<RSM::IStateNode> RSM::StateNodeParser::parseStateNode(const xmlpp::Element *stateElement,
                                                                      std::shared_ptr<RSM::IStateNode> parentState,
                                                                      const RSM::SCXMLContext &context) {
    if (!stateElement) {
        Logger::warn("RSM::StateNodeParser::parseStateNode() - Null state element");
        return nullptr;
    }

    // 상태 ID 가져오기
    std::string stateId;
    auto idAttr = stateElement->get_attribute("id");
    if (idAttr) {
        stateId = idAttr->get_value();
    } else {
        // ID가 없는 경우 자동 생성
        stateId = "state_" + std::to_string(reinterpret_cast<uintptr_t>(stateElement));
        Logger::warn("RSM::StateNodeParser::parseStateNode() - State has no ID, "
                     "generated: " +
                     stateId);
    }

    // 상태 유형 결정
    Type stateType = determineStateType(stateElement);
    Logger::debug("RSM::StateNodeParser::parseStateNode() - Parsing state: " + stateId + " (" +
                  (stateType == Type::PARALLEL  ? "parallel"
                   : stateType == Type::FINAL   ? "final"
                   : stateType == Type::HISTORY ? "history"
                                                : "state") +
                  ")");

    // 상태 노드 생성
    auto stateNode = nodeFactory_->createStateNode(stateId, stateType);
    if (!stateNode) {
        Logger::error("RSM::StateNodeParser::parseStateNode() - Failed to create state node");
        return nullptr;
    }

    // 부모 상태 설정
    stateNode->setParent(parentState.get());
    if (!parentState) {
        Logger::debug("RSM::StateNodeParser::parseStateNode() - No parent state (root)");
    }

    // 히스토리 상태인 경우 추가 처리
    if (stateType == Type::HISTORY) {
        parseHistoryType(stateElement, stateNode);
    } else {
        // onentry/onexit 요소 파싱 (히스토리 상태가 아닌 경우에만) - DISABLED for Phase 2
        // parseEntryExitElements(stateElement, stateNode);

        // 새로운 IActionNode 기반 액션 파싱
        parseEntryExitActionNodes(stateElement, stateNode);

        // 전환 파싱 (히스토리 상태가 아닌 경우에만)
        if (transitionParser_) {
            parseTransitions(stateElement, stateNode);
        } else {
            Logger::warn("RSM::StateNodeParser::parseStateNode() - TransitionParser "
                         "not set, skipping transitions");
        }

        parseReactiveGuards(stateElement, stateNode);
    }

    // 데이터 모델 파싱 - context 전달
    if (dataModelParser_) {
        auto dataItems = dataModelParser_->parseDataModelInState(stateElement, context);
        for (const auto &item : dataItems) {
            stateNode->addDataItem(item);
            Logger::debug("RSM::StateNodeParser::parseStateNode() - Added data item: " + item->getId());
        }
    } else {
        Logger::warn("RSM::StateNodeParser::parseStateNode() - DataModelParser not "
                     "set, skipping data model");
    }

    // 자식 상태 파싱 (compound 및 parallel 상태의 경우) - context 전달
    if (stateType != Type::FINAL && stateType != Type::HISTORY) {
        parseChildStates(stateElement, stateNode, context);
    }

    // invoke 요소 파싱
    if (invokeParser_) {
        parseInvokeElements(stateElement, stateNode);
    } else {
        Logger::warn("RSM::StateNodeParser::parseStateNode() - InvokeParser not "
                     "set, skipping invoke elements");
    }

    // <final> 상태에서 <donedata> 요소 파싱
    if (stateType == Type::FINAL && doneDataParser_) {
        const xmlpp::Element *doneDataElement = ParsingCommon::findFirstChildElement(stateElement, "donedata");
        if (doneDataElement) {
            bool success = doneDataParser_->parseDoneData(doneDataElement, stateNode.get());
            if (!success) {
                Logger::warn("RSM::StateNodeParser::parseStateNode() - Failed to parse "
                             "<donedata> in final state: " +
                             stateId);
                // 오류가 있어도 계속 진행 (치명적이지 않음)
            } else {
                Logger::debug("RSM::StateNodeParser::parseStateNode() - Successfully "
                              "parsed <donedata> in final state: " +
                              stateId);
            }
        }
    }

    // 초기 상태 설정 (compound 상태인 경우)
    if (stateType == Type::COMPOUND && !stateNode->getChildren().empty()) {
        if (stateType == Type::COMPOUND && !stateNode->getChildren().empty()) {
            // <initial> 요소 확인
            auto initialElement = ParsingCommon::findFirstChildElement(stateElement, "initial");
            if (initialElement) {
                // <initial> 요소 파싱
                parseInitialElement(initialElement, stateNode);
                Logger::debug("RSM::StateNodeParser::parseStateNode() - Parsed "
                              "<initial> element for state: " +
                              stateId);
            } else {
                // initial 속성에서 초기 상태 설정
                auto initialAttr = stateElement->get_attribute("initial");
                if (initialAttr) {
                    stateNode->setInitialState(initialAttr->get_value());
                    Logger::debug("RSM::StateNodeParser::parseStateNode() - Set initial "
                                  "state from attribute: " +
                                  initialAttr->get_value());
                } else if (!stateNode->getChildren().empty()) {
                    // 초기 상태가 지정되지 않은 경우 첫 번째 자식을 사용
                    stateNode->setInitialState(stateNode->getChildren().front()->getId());
                    Logger::debug("RSM::StateNodeParser::parseStateNode() - Set default "
                                  "initial state: " +
                                  stateNode->getChildren().front()->getId());
                }
            }
        }
    }

    Logger::debug("RSM::StateNodeParser::parseStateNode() - State " + stateId + " parsed successfully with " +
                  std::to_string(stateNode->getChildren().size()) + " child states");
    return stateNode;
}

RSM::Type RSM::StateNodeParser::determineStateType(const xmlpp::Element *stateElement) {
    if (!stateElement) {
        return Type::ATOMIC;
    }

    // 노드 이름 가져오기
    std::string nodeName = stateElement->get_name();

    // history 요소 확인
    if (ParsingCommon::matchNodeName(nodeName, "history")) {
        return Type::HISTORY;
    }

    // final 요소 확인
    if (ParsingCommon::matchNodeName(nodeName, "final")) {
        return Type::FINAL;
    }

    // parallel 요소 확인
    if (ParsingCommon::matchNodeName(nodeName, "parallel")) {
        return Type::PARALLEL;
    }

    // compound vs. atomic 상태 구분
    // 자식 상태가 있으면 compound, 없으면 atomic
    bool hasChildStates = false;
    auto children = stateElement->get_children();
    for (auto child : children) {
        auto element = dynamic_cast<const xmlpp::Element *>(child);
        if (element) {
            std::string childName = element->get_name();
            if (ParsingCommon::matchNodeName(childName, "state") ||
                ParsingCommon::matchNodeName(childName, "parallel") ||
                ParsingCommon::matchNodeName(childName, "final") ||
                ParsingCommon::matchNodeName(childName, "history")) {
                hasChildStates = true;
                break;
            }
        }
    }

    Logger::debug("RSM::StateNodeParser::determineStateType() - State type: " +
                  std::string(hasChildStates ? "Compound" : "Standard"));
    return hasChildStates ? Type::COMPOUND : Type::ATOMIC;
}

void RSM::StateNodeParser::parseTransitions(const xmlpp::Element *parentElement,
                                            std::shared_ptr<RSM::IStateNode> state) {
    if (!parentElement || !state || !transitionParser_) {
        return;
    }

    auto transitionElements = ParsingCommon::findChildElements(parentElement, "transition");
    for (auto *transitionElement : transitionElements) {
        auto transition = transitionParser_->parseTransitionNode(transitionElement, state.get());
        if (transition) {
            state->addTransition(transition);
        }
    }

    Logger::debug("RSM::StateNodeParser::parseStateNode() - Parsed " + std::to_string(state->getTransitions().size()) +
                  " transitions");
}

void RSM::StateNodeParser::parseEntryExitElements(const xmlpp::Element *parentElement,
                                                  std::shared_ptr<RSM::IStateNode> state) {
    if (!parentElement || !state || !actionParser_) {
        return;
    }

    // onentry 요소 처리
    auto onentryElements = ParsingCommon::findChildElements(parentElement, "onentry");
    for (auto *onentryElement : onentryElements) {
        auto actions = actionParser_->parseActionsInElement(onentryElement);
        for (const auto &action : actions) {
            // script 액션의 경우 textContent를 사용, 그렇지 않으면 ID 사용
            std::string actionContent;
            if (action->getType() == "script" && !action->getAttribute("textContent").empty()) {
                actionContent = action->getAttribute("textContent");
            } else {
                actionContent = action->getId();
            }
            state->addEntryAction(actionContent);
            Logger::debug("RSM::StateNodeParser::parseEntryExitElements() - Added "
                          "entry action: " +
                          actionContent);
        }
    }

    // onexit 요소 처리
    auto onexitElements = ParsingCommon::findChildElements(parentElement, "onexit");
    for (auto *onexitElement : onexitElements) {
        auto actions = actionParser_->parseActionsInElement(onexitElement);
        for (const auto &action : actions) {
            // script 액션의 경우 textContent를 사용, 그렇지 않으면 ID 사용
            std::string actionContent;
            if (action->getType() == "script" && !action->getAttribute("textContent").empty()) {
                actionContent = action->getAttribute("textContent");
            } else {
                actionContent = action->getId();
            }
            state->addExitAction(actionContent);
            Logger::debug("RSM::StateNodeParser::parseEntryExitElements() - Added "
                          "exit action: " +
                          actionContent);
        }
    }
}

void RSM::StateNodeParser::parseChildStates(const xmlpp::Element *stateElement,
                                            std::shared_ptr<RSM::IStateNode> parentState,
                                            const RSM::SCXMLContext &context) {
    Logger::debug("RSM::StateNodeParser::parseChildStates() - Parsing child states");

    // state, parallel, final, history 등의 자식 요소 검색
    std::vector<const xmlpp::Element *> childStateElements;
    auto stateElements = ParsingCommon::findChildElements(stateElement, "state");
    childStateElements.insert(childStateElements.end(), stateElements.begin(), stateElements.end());

    auto parallelElements = ParsingCommon::findChildElements(stateElement, "parallel");
    childStateElements.insert(childStateElements.end(), parallelElements.begin(), parallelElements.end());

    auto finalElements = ParsingCommon::findChildElements(stateElement, "final");
    childStateElements.insert(childStateElements.end(), finalElements.begin(), finalElements.end());

    auto historyElements = ParsingCommon::findChildElements(stateElement, "history");
    childStateElements.insert(childStateElements.end(), historyElements.begin(), historyElements.end());

    // 발견된 각 자식 상태를 재귀적으로 파싱
    for (auto *childElement : childStateElements) {
        auto childState = parseStateNode(childElement, parentState, context);
        if (childState) {
            parentState->addChild(childState);
        }
    }

    Logger::debug("RSM::StateNodeParser::parseChildStates() - Found " + std::to_string(childStateElements.size()) +
                  " child states");
}

void RSM::StateNodeParser::parseInvokeElements(const xmlpp::Element *parentElement,
                                               std::shared_ptr<RSM::IStateNode> state) {
    if (!parentElement || !state || !invokeParser_) {
        return;
    }

    auto invokeElements = ParsingCommon::findChildElements(parentElement, "invoke");
    for (auto *invokeElement : invokeElements) {
        auto invokeNode = invokeParser_->parseInvokeNode(invokeElement);
        if (invokeNode) {
            // Invoke 노드를 상태에 추가
            state->addInvoke(invokeNode);
            Logger::debug("RSM::StateNodeParser::parseInvokeElements() - Added invoke: " + invokeNode->getId());

            // Param 요소들로부터 데이터 모델 아이템 생성 및 추가
            auto dataItems = invokeParser_->parseParamElementsAndCreateDataItems(invokeElement, invokeNode);
            for (const auto &dataItem : dataItems) {
                state->addDataItem(dataItem);
                Logger::debug("RSM::StateNodeParser::parseInvokeElements() - Added "
                              "data item from param: " +
                              dataItem->getId());
            }
        }
    }

    Logger::debug("RSM::StateNodeParser::parseInvokeElements() - Parsed " + std::to_string(state->getInvoke().size()) +
                  " invoke elements");
}

void RSM::StateNodeParser::parseHistoryType(const xmlpp::Element *historyElement,
                                            std::shared_ptr<RSM::IStateNode> state) {
    if (!historyElement || !state) {
        return;
    }

    // 기본값은 shallow
    bool isDeep = false;

    // type 속성 확인
    auto typeAttr = historyElement->get_attribute("type");
    if (typeAttr && typeAttr->get_value() == "deep") {
        isDeep = true;
    }

    // 히스토리 타입 설정
    state->setHistoryType(isDeep);

    Logger::debug("RSM::StateNodeParser::parseHistoryType() - History state " + state->getId() +
                  " type: " + (isDeep ? "deep" : "shallow"));

    // 히스토리 상태의 기본 전환도 파싱
    if (transitionParser_) {
        parseTransitions(historyElement, state);
    }
}

void RSM::StateNodeParser::parseReactiveGuards(const xmlpp::Element *parentElement,
                                               std::shared_ptr<RSM::IStateNode> state) {
    if (!parentElement || !state) {
        return;
    }

    // code:reactive-guard 요소 찾기
    auto reactiveGuardElements =
        ParsingCommon::findChildElementsWithNamespace(parentElement, "reactive-guard", "http://example.org/code");

    for (auto *reactiveGuardElement : reactiveGuardElements) {
        // id 속성 가져오기
        auto idAttr = reactiveGuardElement->get_attribute("id");
        if (idAttr) {
            std::string guardId = idAttr->get_value();
            state->addReactiveGuard(guardId);
            Logger::debug("RSM::StateNodeParser::parseReactiveGuards() - Added "
                          "reactive guard: " +
                          guardId);
        } else {
            Logger::warn("RSM::StateNodeParser::parseReactiveGuards() - Reactive "
                         "guard without ID");
        }
    }

    Logger::debug("RSM::StateNodeParser::parseReactiveGuards() - Parsed " +
                  std::to_string(reactiveGuardElements.size()) + " reactive guards");
}

void RSM::StateNodeParser::parseInitialElement(const xmlpp::Element *initialElement,
                                               std::shared_ptr<RSM::IStateNode> state) {
    if (!initialElement || !state || !transitionParser_) {
        return;
    }

    Logger::debug("RSM::StateNodeParser::parseInitialElement() - Parsing initial "
                  "element for state: " +
                  state->getId());

    // <transition> 요소 찾기
    const xmlpp::Element *transitionElement = ParsingCommon::findFirstChildElement(initialElement, "transition");
    if (transitionElement) {
        // 전환 파싱 - 부모 상태도 함께 전달
        auto transition = transitionParser_->parseTransitionNode(transitionElement, state.get());
        if (transition) {
            // IStateNode 인터페이스를 통해 직접 호출
            state->setInitialTransition(transition);

            // initialState_ 설정 (첫 번째 target)
            if (!transition->getTargets().empty()) {
                state->setInitialState(transition->getTargets()[0]);
                Logger::debug("RSM::StateNodeParser::parseInitialElement() - Initial "
                              "state set to: " +
                              transition->getTargets()[0]);
            }

            Logger::debug("RSM::StateNodeParser::parseInitialElement() - Initial "
                          "transition set for state: " +
                          state->getId());
        }
    }
}

void RSM::StateNodeParser::parseEntryExitActionNodes(const xmlpp::Element *parentElement,
                                                     std::shared_ptr<RSM::IStateNode> state) {
    if (!parentElement || !state) {
        return;
    }

    // onentry 요소 처리
    auto onentryElements = ParsingCommon::findChildElements(parentElement, "onentry");
    for (auto *onentryElement : onentryElements) {
        parseExecutableContent(onentryElement, state, true);  // true = entry actions
    }

    // onexit 요소 처리
    auto onexitElements = ParsingCommon::findChildElements(parentElement, "onexit");
    for (auto *onexitElement : onexitElements) {
        parseExecutableContent(onexitElement, state, false);  // false = exit actions
    }
}

void RSM::StateNodeParser::parseExecutableContent(const xmlpp::Element *parentElement,
                                                  std::shared_ptr<RSM::IStateNode> state, bool isEntryAction) {
    if (!parentElement || !state) {
        return;
    }

    auto children = parentElement->get_children();
    for (auto child : children) {
        auto element = dynamic_cast<const xmlpp::Element *>(child);
        if (!element) {
            continue;
        }

        std::string nodeName = element->get_name();

        // <script> 요소 파싱
        if (ParsingCommon::matchNodeName(nodeName, "script")) {
            auto scriptAction = parseScriptAction(element);
            if (scriptAction) {
                if (isEntryAction) {
                    state->addEntryActionNode(scriptAction);
                } else {
                    state->addExitActionNode(scriptAction);
                }
                Logger::debug("RSM::StateNodeParser::parseExecutableContent() - Added " +
                              std::string(isEntryAction ? "entry" : "exit") + " script action");
            }
        }
        // <assign> 요소 파싱
        else if (ParsingCommon::matchNodeName(nodeName, "assign")) {
            auto assignAction = parseAssignAction(element);
            if (assignAction) {
                if (isEntryAction) {
                    state->addEntryActionNode(assignAction);
                } else {
                    state->addExitActionNode(assignAction);
                }
                Logger::debug("RSM::StateNodeParser::parseExecutableContent() - Added " +
                              std::string(isEntryAction ? "entry" : "exit") + " assign action");
            }
        }
    }
}

std::shared_ptr<RSM::Actions::IActionNode>
RSM::StateNodeParser::parseScriptAction(const xmlpp::Element *scriptElement) {
    if (!scriptElement) {
        return nullptr;
    }

    // 스크립트 내용 추출
    std::string content;
    auto children = scriptElement->get_children();
    for (auto child : children) {
        auto textNode = dynamic_cast<const xmlpp::TextNode *>(child);
        if (textNode) {
            content += textNode->get_content();
        }
    }

    // src 속성 확인 (현재는 inline content만 지원)
    auto srcAttr = scriptElement->get_attribute("src");
    if (srcAttr && !srcAttr->get_value().empty()) {
        Logger::warn("RSM::StateNodeParser::parseScriptAction() - External script sources not yet supported: " +
                     srcAttr->get_value());
        // TODO: 외부 스크립트 파일 로딩 구현
    }

    // 내용이 비어있으면 빈 스크립트로 생성
    if (content.empty()) {
        content = "";  // 빈 스크립트
    }

    // ScriptAction 생성 (생성자에 content 전달)
    auto scriptAction = std::make_shared<RSM::Actions::ScriptAction>(content);

    return scriptAction;
}

std::shared_ptr<RSM::Actions::IActionNode>
RSM::StateNodeParser::parseAssignAction(const xmlpp::Element *assignElement) {
    if (!assignElement) {
        return nullptr;
    }

    // location 속성 (필수)
    auto locationAttr = assignElement->get_attribute("location");
    if (!locationAttr) {
        Logger::warn("RSM::StateNodeParser::parseAssignAction() - Missing required 'location' attribute");
        return nullptr;
    }
    std::string location = locationAttr->get_value();

    // expr 속성
    std::string expr;
    auto exprAttr = assignElement->get_attribute("expr");
    if (exprAttr) {
        expr = exprAttr->get_value();
    } else {
        // expr 속성이 없으면 텍스트 내용 사용
        auto children = assignElement->get_children();
        for (auto child : children) {
            auto textNode = dynamic_cast<const xmlpp::TextNode *>(child);
            if (textNode) {
                expr += textNode->get_content();
            }
        }
    }

    // expr이 비어있으면 경고
    if (expr.empty()) {
        Logger::warn("RSM::StateNodeParser::parseAssignAction() - Empty expression for location: " + location);
        expr = "undefined";  // 기본값
    }

    // AssignAction 생성 (생성자에 location과 expr 전달)
    auto assignAction = std::make_shared<RSM::Actions::AssignAction>(location, expr);

    return assignAction;
}
