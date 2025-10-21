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

    // Get state ID
    std::string stateId;
    auto idAttr = stateElement->get_attribute("id");
    if (idAttr) {
        stateId = idAttr->get_value();
    } else {
        // Auto-generate if ID is missing
        stateId = "state_" + std::to_string(reinterpret_cast<uintptr_t>(stateElement));
        LOG_WARN("State has no ID, generated: {}", stateId);
    }

    // Determine state type
    Type stateType = determineStateType(stateElement);
    LOG_DEBUG("Parsing state: {} ({})", stateId,
              (stateType == Type::PARALLEL  ? "parallel"
               : stateType == Type::FINAL   ? "final"
               : stateType == Type::HISTORY ? "history"
                                            : "state"));

    // Create state node
    auto stateNode = nodeFactory_->createStateNode(stateId, stateType);
    if (!stateNode) {
        LOG_ERROR("Failed to create state node");
        return nullptr;
    }

    // Set parent state
    stateNode->setParent(parentState.get());
    if (!parentState) {
        LOG_DEBUG("No parent state (root)");
    }

    // Additional processing for history states
    if (stateType == Type::HISTORY) {
        parseHistoryType(stateElement, stateNode);
    } else {
        // Parse onentry/onexit elements (only for non-history states) - Feature available
        // parseEntryExitElements(stateElement, stateNode);

        // Parse new IActionNode-based actions
        parseEntryExitActionNodes(stateElement, stateNode);

        // Parse transitions (only for non-history states)
        if (transitionParser_) {
            parseTransitions(stateElement, stateNode);
        } else {
            LOG_WARN("TransitionParser not set, skipping transitions");
        }

        parseReactiveGuards(stateElement, stateNode);
    }

    // Parse data model - pass context
    if (dataModelParser_) {
        auto dataItems = dataModelParser_->parseDataModelInState(stateElement, context);
        for (const auto &item : dataItems) {
            stateNode->addDataItem(item);
            LOG_DEBUG("Added data item: {}", item->getId());
        }
    } else {
        LOG_WARN("DataModelParser not set, skipping data model");
    }

    // Parse child states (for compound and parallel states) - pass context
    if (stateType != Type::FINAL && stateType != Type::HISTORY) {
        parseChildStates(stateElement, stateNode, context);
    }

    // Parse invoke elements
    if (invokeParser_) {
        parseInvokeElements(stateElement, stateNode);
    } else {
        LOG_WARN("InvokeParser not set, skipping invoke elements");
    }

    // Parse <donedata> element in <final> state
    if (stateType == Type::FINAL && doneDataParser_) {
        const xmlpp::Element *doneDataElement = ParsingCommon::findFirstChildElement(stateElement, "donedata");
        if (doneDataElement) {
            bool success = doneDataParser_->parseDoneData(doneDataElement, stateNode.get());
            if (!success) {
                LOG_WARN("Failed to parse <donedata> in final state: {}", stateId);
                // Continue even with errors (not fatal)
            } else {
                LOG_DEBUG("Successfully parsed <donedata> in final state: {}", stateId);
            }
        }
    }

    // Set initial state (for compound states)
    if (stateType == Type::COMPOUND && !stateNode->getChildren().empty()) {
        if (stateType == Type::COMPOUND && !stateNode->getChildren().empty()) {
            // Check for <initial> element
            auto initialElement = ParsingCommon::findFirstChildElement(stateElement, "initial");
            if (initialElement) {
                // Parse <initial> element
                parseInitialElement(initialElement, stateNode);
                LOG_DEBUG("Parsed <initial> element for state: {}", stateId);
            } else {
                // Set initial state from initial attribute
                auto initialAttr = stateElement->get_attribute("initial");
                if (initialAttr) {
                    stateNode->setInitialState(initialAttr->get_value());
                    LOG_DEBUG("StateNodeParser: State '{}' initial attribute='{}'", stateId, initialAttr->get_value());
                } else if (!stateNode->getChildren().empty()) {
                    // Use first child if initial state is not specified
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

    // Get node name
    std::string nodeName = stateElement->get_name();

    // Check for history element
    if (ParsingCommon::matchNodeName(nodeName, "history")) {
        return Type::HISTORY;
    }

    // Check for final element
    if (ParsingCommon::matchNodeName(nodeName, "final")) {
        return Type::FINAL;
    }

    // Check for parallel element
    if (ParsingCommon::matchNodeName(nodeName, "parallel")) {
        return Type::PARALLEL;
    }

    // Distinguish between compound and atomic states
    // Compound if has child states, atomic otherwise
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

    // Search for child elements like state, parallel, final, history
    std::vector<const xmlpp::Element *> childStateElements;
    auto stateElements = ParsingCommon::findChildElements(stateElement, "state");
    childStateElements.insert(childStateElements.end(), stateElements.begin(), stateElements.end());

    auto parallelElements = ParsingCommon::findChildElements(stateElement, "parallel");
    childStateElements.insert(childStateElements.end(), parallelElements.begin(), parallelElements.end());

    auto finalElements = ParsingCommon::findChildElements(stateElement, "final");
    childStateElements.insert(childStateElements.end(), finalElements.begin(), finalElements.end());

    auto historyElements = ParsingCommon::findChildElements(stateElement, "history");
    childStateElements.insert(childStateElements.end(), historyElements.begin(), historyElements.end());

    // Recursively parse each discovered child state
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

            // Add invoke node to state
            state->addInvoke(invokeNode);
            LOG_DEBUG("Added invoke: {}", invokeNode->getId());

            // Create and add data model items from param elements
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

    // Default is shallow
    bool isDeep = false;

    // Check type attribute
    auto typeAttr = historyElement->get_attribute("type");
    if (typeAttr && typeAttr->get_value() == "deep") {
        isDeep = true;
    }

    // Set history type
    state->setHistoryType(isDeep);

    LOG_DEBUG("History state {} type: {}", state->getId(), (isDeep ? "deep" : "shallow"));

    // Parse default transition for history state
    if (transitionParser_) {
        parseTransitions(historyElement, state);
    }
}

void RSM::StateNodeParser::parseReactiveGuards(const xmlpp::Element *parentElement,
                                               std::shared_ptr<RSM::IStateNode> state) {
    if (!parentElement || !state) {
        return;
    }

    // Find code:reactive-guard elements
    auto reactiveGuardElements =
        ParsingCommon::findChildElementsWithNamespace(parentElement, "reactive-guard", "http://example.org/code");

    for (auto *reactiveGuardElement : reactiveGuardElements) {
        // Get id attribute
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

    // Find <transition> elements
    const xmlpp::Element *transitionElement = ParsingCommon::findFirstChildElement(initialElement, "transition");
    if (transitionElement) {
        // Parse transition - pass parent state together
        auto transition = transitionParser_->parseTransitionNode(transitionElement, state.get());
        if (transition) {
            // Call directly through IStateNode interface
            state->setInitialTransition(transition);

            // Set initialState_ (W3C SCXML 3.3: space-separated targets for parallel regions)
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

    // W3C SCXML 3.8: Process onentry elements - each onentry is a separate block
    auto onentryElements = ParsingCommon::findChildElements(parentElement, "onentry");
    for (auto *onentryElement : onentryElements) {
        std::vector<std::shared_ptr<RSM::IActionNode>> actionBlock;
        parseExecutableContentBlock(onentryElement, actionBlock);

        if (!actionBlock.empty()) {
            state->addEntryActionBlock(actionBlock);
            LOG_DEBUG("W3C SCXML 3.8: Added entry action block with {} actions", actionBlock.size());
        }
    }

    // W3C SCXML 3.9: Process onexit elements - each onexit is a separate block
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
            // Remove namespace prefix
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
