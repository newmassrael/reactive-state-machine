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

RSM::StateNodeParser::StateNodeParser(std::shared_ptr<RSM::NodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    LOG_DEBUG("Creating state node parser");
}

RSM::StateNodeParser::~StateNodeParser() {
    LOG_DEBUG("Destroying state node parser");
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

    LOG_DEBUG("Related parsers set");
}

std::shared_ptr<RSM::IStateNode> RSM::StateNodeParser::parseStateNode(const xmlpp::Element *stateElement,
                                                                      std::shared_ptr<RSM::IStateNode> parentState,
                                                                      const RSM::SCXMLContext &context) {
    if (!stateElement) {
        LOG_WARN("Null state element");
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
        LOG_WARN("State has no ID, generated: {}", stateId);
    }

    // 상태 유형 결정
    Type stateType = determineStateType(stateElement);
    LOG_DEBUG("Parsing state: {} ({})", stateId,
              (stateType == Type::PARALLEL  ? "parallel"
               : stateType == Type::FINAL   ? "final"
               : stateType == Type::HISTORY ? "history"
                                            : "state"));

    // 상태 노드 생성
    auto stateNode = nodeFactory_->createStateNode(stateId, stateType);
    if (!stateNode) {
        LOG_ERROR("Failed to create state node");
        return nullptr;
    }

    // 부모 상태 설정
    stateNode->setParent(parentState.get());
    if (!parentState) {
        LOG_DEBUG("No parent state (root)");
    }

    // 히스토리 상태인 경우 추가 처리
    if (stateType == Type::HISTORY) {
        parseHistoryType(stateElement, stateNode);
    } else {
        // onentry/onexit 요소 파싱 (히스토리 상태가 아닌 경우에만) - Feature available
        // parseEntryExitElements(stateElement, stateNode);

        // 새로운 IActionNode 기반 액션 파싱
        parseEntryExitActionNodes(stateElement, stateNode);

        // 전환 파싱 (히스토리 상태가 아닌 경우에만)
        if (transitionParser_) {
            parseTransitions(stateElement, stateNode);
        } else {
            LOG_WARN("TransitionParser not set, skipping transitions");
        }

        parseReactiveGuards(stateElement, stateNode);
    }

    // 데이터 모델 파싱 - context 전달
    if (dataModelParser_) {
        auto dataItems = dataModelParser_->parseDataModelInState(stateElement, context);
        for (const auto &item : dataItems) {
            stateNode->addDataItem(item);
            LOG_DEBUG("Added data item: {}", item->getId());
        }
    } else {
        LOG_WARN("DataModelParser not set, skipping data model");
    }

    // 자식 상태 파싱 (compound 및 parallel 상태의 경우) - context 전달
    if (stateType != Type::FINAL && stateType != Type::HISTORY) {
        parseChildStates(stateElement, stateNode, context);
    }

    // invoke 요소 파싱
    if (invokeParser_) {
        parseInvokeElements(stateElement, stateNode);
    } else {
        LOG_WARN("InvokeParser not set, skipping invoke elements");
    }

    // <final> 상태에서 <donedata> 요소 파싱
    if (stateType == Type::FINAL && doneDataParser_) {
        const xmlpp::Element *doneDataElement = ParsingCommon::findFirstChildElement(stateElement, "donedata");
        if (doneDataElement) {
            bool success = doneDataParser_->parseDoneData(doneDataElement, stateNode.get());
            if (!success) {
                LOG_WARN("Failed to parse <donedata> in final state: {}", stateId);
                // 오류가 있어도 계속 진행 (치명적이지 않음)
            } else {
                LOG_DEBUG("Successfully parsed <donedata> in final state: {}", stateId);
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
                LOG_DEBUG("Parsed <initial> element for state: {}", stateId);
            } else {
                // initial 속성에서 초기 상태 설정
                auto initialAttr = stateElement->get_attribute("initial");
                if (initialAttr) {
                    stateNode->setInitialState(initialAttr->get_value());
                    LOG_DEBUG("StateNodeParser: State '{}' initial attribute='{}'", stateId, initialAttr->get_value());
                } else if (!stateNode->getChildren().empty()) {
                    // 초기 상태가 지정되지 않은 경우 첫 번째 자식을 사용
                    stateNode->setInitialState(stateNode->getChildren().front()->getId());
                    LOG_DEBUG("Set default initial state: {}", stateNode->getChildren().front()->getId());
                }
            }
        }
    }

    LOG_DEBUG("State {} parsed successfully with {} child states", stateId, stateNode->getChildren().size());
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

    LOG_DEBUG("State type: {}", (hasChildStates ? "Compound" : "Standard"));
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

    LOG_DEBUG("Parsed {} transitions", state->getTransitions().size());
}

void RSM::StateNodeParser::parseChildStates(const xmlpp::Element *stateElement,
                                            std::shared_ptr<RSM::IStateNode> parentState,
                                            const RSM::SCXMLContext &context) {
    LOG_DEBUG("Parsing child states");

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

    LOG_DEBUG("Found {} child states", childStateElements.size());
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
            // W3C SCXML 6.4: Set parent state ID for invoke ID generation (test 224)
            invokeNode->setStateId(state->getId());

            // Invoke 노드를 상태에 추가
            state->addInvoke(invokeNode);
            LOG_DEBUG("Added invoke: {}", invokeNode->getId());

            // Param 요소들로부터 데이터 모델 아이템 생성 및 추가
            auto dataItems = invokeParser_->parseParamElementsAndCreateDataItems(invokeElement, invokeNode);
            for (const auto &dataItem : dataItems) {
                state->addDataItem(dataItem);
                LOG_DEBUG("Added data item from param: {}", dataItem->getId());
            }
        }
    }

    LOG_DEBUG("Parsed {} invoke elements", state->getInvoke().size());
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

    LOG_DEBUG("History state {} type: {}", state->getId(), (isDeep ? "deep" : "shallow"));

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
            LOG_DEBUG("Added reactive guard: {}", guardId);
        } else {
            LOG_WARN("Reactive guard without ID");
        }
    }

    LOG_DEBUG("Parsed {} reactive guards", reactiveGuardElements.size());
}

void RSM::StateNodeParser::parseInitialElement(const xmlpp::Element *initialElement,
                                               std::shared_ptr<RSM::IStateNode> state) {
    if (!initialElement || !state || !transitionParser_) {
        return;
    }

    LOG_DEBUG("Parsing initial element for state: {}", state->getId());

    // <transition> 요소 찾기
    const xmlpp::Element *transitionElement = ParsingCommon::findFirstChildElement(initialElement, "transition");
    if (transitionElement) {
        // 전환 파싱 - 부모 상태도 함께 전달
        auto transition = transitionParser_->parseTransitionNode(transitionElement, state.get());
        if (transition) {
            // IStateNode 인터페이스를 통해 직접 호출
            state->setInitialTransition(transition);

            // initialState_ 설정 (W3C SCXML 3.3: space-separated targets for parallel regions)
            if (!transition->getTargets().empty()) {
                std::string allTargets;
                for (size_t i = 0; i < transition->getTargets().size(); ++i) {
                    if (i > 0) {
                        allTargets += " ";
                    }
                    allTargets += transition->getTargets()[i];
                }
                state->setInitialState(allTargets);
                LOG_DEBUG("StateNodeParser: State '{}' <initial> transition targets='{}'", state->getId(), allTargets);
            }

            LOG_DEBUG("Initial transition set for state: {}", state->getId());
        }
    }
}

void RSM::StateNodeParser::parseEntryExitActionNodes(const xmlpp::Element *parentElement,
                                                     std::shared_ptr<RSM::IStateNode> state) {
    if (!parentElement || !state) {
        return;
    }

    // W3C SCXML 3.8: onentry 요소 처리 - 각 onentry는 별도 블록
    auto onentryElements = ParsingCommon::findChildElements(parentElement, "onentry");
    for (auto *onentryElement : onentryElements) {
        std::vector<std::shared_ptr<RSM::IActionNode>> actionBlock;
        parseExecutableContentBlock(onentryElement, actionBlock);

        if (!actionBlock.empty()) {
            state->addEntryActionBlock(actionBlock);
            LOG_DEBUG("W3C SCXML 3.8: Added entry action block with {} actions", actionBlock.size());
        }
    }

    // W3C SCXML 3.9: onexit 요소 처리 - 각 onexit는 별도 블록
    auto onexitElements = ParsingCommon::findChildElements(parentElement, "onexit");
    for (auto *onexitElement : onexitElements) {
        std::vector<std::shared_ptr<RSM::IActionNode>> actionBlock;
        parseExecutableContentBlock(onexitElement, actionBlock);

        if (!actionBlock.empty()) {
            state->addExitActionBlock(actionBlock);
            LOG_DEBUG("W3C SCXML 3.9: Added exit action block with {} actions", actionBlock.size());
        }
    }
}

void RSM::StateNodeParser::parseExecutableContentBlock(const xmlpp::Element *parentElement,
                                                       std::vector<std::shared_ptr<RSM::IActionNode>> &actionBlock) {
    if (!parentElement) {
        return;
    }

    if (!actionParser_) {
        LOG_WARN("RSM::StateNodeParser::parseExecutableContentBlock() - ActionParser not available");
        return;
    }

    auto children = parentElement->get_children();
    for (auto child : children) {
        auto element = dynamic_cast<const xmlpp::Element *>(child);
        if (!element) {
            continue;
        }

        // W3C SCXML: Parse each executable content element into an action node
        auto action = actionParser_->parseActionNode(element);
        if (action) {
            actionBlock.push_back(action);

            std::string elementName = element->get_name();
            // 네임스페이스 접두사 제거
            size_t colonPos = elementName.find(':');
            if (colonPos != std::string::npos && colonPos + 1 < elementName.length()) {
                elementName = elementName.substr(colonPos + 1);
            }

            LOG_DEBUG("Parsed executable content '{}' into action block", elementName);
        } else {
            std::string elementName = element->get_name();
            LOG_DEBUG("Element '{}' not recognized as executable content by ActionParser", elementName);
        }
    }
}
