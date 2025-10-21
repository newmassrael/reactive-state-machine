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

    // Parse state node
    std::shared_ptr<IStateNode> parseStateNode(const xmlpp::Element *stateElement,
                                               std::shared_ptr<IStateNode> parentState = nullptr,
                                               const SCXMLContext &context = SCXMLContext());

    // Set related parsers
    void setRelatedParsers(std::shared_ptr<TransitionParser> transitionParser,
                           std::shared_ptr<ActionParser> actionParser, std::shared_ptr<DataModelParser> dataModelParser,
                           std::shared_ptr<InvokeParser> invokeParser, std::shared_ptr<DoneDataParser> doneDataParser);

private:
    // Determine state type
    Type determineStateType(const xmlpp::Element *stateElement);

    // Parse child states
    void parseChildStates(const xmlpp::Element *stateElement, std::shared_ptr<IStateNode> parentState,
                          const SCXMLContext &context = SCXMLContext());

    // Parse transition elements
    void parseTransitions(const xmlpp::Element *parentElement, std::shared_ptr<IStateNode> state);

    // W3C SCXML 3.8/3.9: Parse onentry/onexit elements as IActionNode block-based
    void parseEntryExitActionNodes(const xmlpp::Element *parentElement, std::shared_ptr<IStateNode> state);

    // W3C SCXML 3.8/3.9: Block-based executable content parsing
    void parseExecutableContentBlock(const xmlpp::Element *parentElement,
                                     std::vector<std::shared_ptr<RSM::IActionNode>> &actionBlock);

    // Parse invoke elements
    void parseInvokeElements(const xmlpp::Element *parentElement, std::shared_ptr<IStateNode> state);

    // Parse history state type (shallow/deep)
    void parseHistoryType(const xmlpp::Element *historyElement, std::shared_ptr<IStateNode> state);

    // Parse reactive guards
    void parseReactiveGuards(const xmlpp::Element *parentElement, std::shared_ptr<IStateNode> state);

    // Parse initial element
    void parseInitialElement(const xmlpp::Element *initialElement, std::shared_ptr<IStateNode> state);

    std::shared_ptr<NodeFactory> nodeFactory_;
    std::shared_ptr<TransitionParser> transitionParser_;
    std::shared_ptr<ActionParser> actionParser_;
    std::shared_ptr<DataModelParser> dataModelParser_;
    std::shared_ptr<InvokeParser> invokeParser_;
    std::shared_ptr<DoneDataParser> doneDataParser_;
};

}  // namespace RSM