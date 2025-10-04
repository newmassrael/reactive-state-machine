#pragma once

#include "SCXMLContext.h"
#include "actions/IActionNode.h"
#include "factory/NodeFactory.h"
#include "model/IStateNode.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>

namespace RSM {

class TransitionParser;
class ActionParser;
class DataModelParser;
class InvokeParser;
class DoneDataParser;

class StateNodeParser {
public:
    explicit StateNodeParser(std::shared_ptr<NodeFactory> nodeFactory);
    ~StateNodeParser();

    // 상태 노드 파싱
    std::shared_ptr<IStateNode> parseStateNode(const xmlpp::Element *stateElement,
                                               std::shared_ptr<IStateNode> parentState = nullptr,
                                               const SCXMLContext &context = SCXMLContext());

    // 관련 파서 설정
    void setRelatedParsers(std::shared_ptr<TransitionParser> transitionParser,
                           std::shared_ptr<ActionParser> actionParser, std::shared_ptr<DataModelParser> dataModelParser,
                           std::shared_ptr<InvokeParser> invokeParser, std::shared_ptr<DoneDataParser> doneDataParser);

private:
    // 상태 유형 결정
    Type determineStateType(const xmlpp::Element *stateElement);

    // 자식 상태 파싱
    void parseChildStates(const xmlpp::Element *stateElement, std::shared_ptr<IStateNode> parentState,
                          const SCXMLContext &context = SCXMLContext());

    // 전환 요소 파싱
    void parseTransitions(const xmlpp::Element *parentElement, std::shared_ptr<IStateNode> state);

    // W3C SCXML 3.8/3.9: onentry/onexit 요소를 IActionNode 블록 기반으로 파싱
    void parseEntryExitActionNodes(const xmlpp::Element *parentElement, std::shared_ptr<IStateNode> state);

    // W3C SCXML 3.8/3.9: Block-based executable content parsing
    void parseExecutableContentBlock(const xmlpp::Element *parentElement,
                                     std::vector<std::shared_ptr<RSM::IActionNode>> &actionBlock);

    // invoke 요소 파싱
    void parseInvokeElements(const xmlpp::Element *parentElement, std::shared_ptr<IStateNode> state);

    // 히스토리 상태 타입(shallow/deep) 파싱
    void parseHistoryType(const xmlpp::Element *historyElement, std::shared_ptr<IStateNode> state);

    // 반응형 가드 파싱 메서드
    void parseReactiveGuards(const xmlpp::Element *parentElement, std::shared_ptr<IStateNode> state);

    // initial 요소 파싱 메서드 추가
    void parseInitialElement(const xmlpp::Element *initialElement, std::shared_ptr<IStateNode> state);

    std::shared_ptr<NodeFactory> nodeFactory_;
    std::shared_ptr<TransitionParser> transitionParser_;
    std::shared_ptr<ActionParser> actionParser_;
    std::shared_ptr<DataModelParser> dataModelParser_;
    std::shared_ptr<InvokeParser> invokeParser_;
    std::shared_ptr<DoneDataParser> doneDataParser_;
};

}  // namespace RSM