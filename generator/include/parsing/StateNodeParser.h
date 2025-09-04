#pragma once

#include "model/INodeFactory.h"
#include "model/IStateNode.h"
#include "model/ParsingContext.h"
#include "core/types.h"
#include <libxml++/libxml++.h>
#include <memory>
#include <string>

using SCXML::Model::IStateNode;
using SCXML::Model::DoneData;
using SCXML::Type;


// Forward declarations
namespace SCXML {

namespace Model {
class IStateNode;
class DoneData;
}
namespace Parsing {
// Forward declarations
class TransitionParser;
class ActionParser;
class DataModelParser;
class InvokeParser;
class DoneDataParser;
class GuardParser;

class StateNodeParser {
public:
    explicit StateNodeParser(::std::shared_ptr<::SCXML::Model::INodeFactory> nodeFactory);
    ~StateNodeParser();

    // Parse state node
    ::std::shared_ptr<Model::IStateNode>
    parseStateNode(const xmlpp::Element *stateElement, ::std::shared_ptr<Model::IStateNode> parentState = nullptr,
                   const SCXML::Model::ParsingContext &context = SCXML::Model::ParsingContext());

    // Set related parsers
    void setRelatedParsers(::std::shared_ptr<TransitionParser> transitionParser,
                           ::std::shared_ptr<ActionParser> actionParser,
                           ::std::shared_ptr<SCXML::Parsing::DataModelParser> dataModelParser,
                           ::std::shared_ptr<SCXML::Parsing::InvokeParser> invokeParser,
                           ::std::shared_ptr<DoneDataParser> doneDataParser);

private:
    // Determine state type
    Type determineStateType(const xmlpp::Element *stateElement);

    // Parse child states
    void parseChildStates(const xmlpp::Element *stateElement, ::std::shared_ptr<Model::IStateNode> parentState,
                          const SCXML::Model::ParsingContext &context = SCXML::Model::ParsingContext());

    // 전환 요소 파싱
    void parseTransitions(const xmlpp::Element *parentElement, ::std::shared_ptr<Model::IStateNode> state);

    // onentry/onexit 요소 파싱
    void parseEntryExitElements(const xmlpp::Element *parentElement, ::std::shared_ptr<Model::IStateNode> state);

    // invoke 요소 파싱
    void parseInvokeElements(const xmlpp::Element *parentElement, ::std::shared_ptr<Model::IStateNode> state);

    // Parse history state type (shallow/deep)
    void parseHistoryType(const xmlpp::Element *historyElement, ::std::shared_ptr<Model::IStateNode> state);

    // 반응형 가드 파싱 메서드
    void parseReactiveGuards(const xmlpp::Element *parentElement, ::std::shared_ptr<Model::IStateNode> state);

    // initial 요소 파싱 메서드 추가
    void parseInitialElement(const xmlpp::Element *initialElement, ::std::shared_ptr<Model::IStateNode> state);

    ::std::shared_ptr<::SCXML::Model::INodeFactory> nodeFactory_;
    ::std::shared_ptr<SCXML::Parsing::TransitionParser> transitionParser_;
    ::std::shared_ptr<SCXML::Parsing::ActionParser> actionParser_;
    ::std::shared_ptr<SCXML::Parsing::DataModelParser> dataModelParser_;
    ::std::shared_ptr<SCXML::Parsing::InvokeParser> invokeParser_;
    ::std::shared_ptr<SCXML::Parsing::DoneDataParser> doneDataParser_;
};

}  // namespace Parsing
}  // namespace SCXML