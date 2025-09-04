
#include "parsing/ExecutableContentParser.h"
#include "core/actions/IfActionNode.h"
#include "core/actions/SendActionNode.h"
#include "core/actions/RaiseActionNode.h"
#include "core/actions/AssignActionNode.h"
#include "core/actions/LogActionNode.h"
#include "core/actions/CancelActionNode.h"
#include "core/actions/ScriptActionNode.h"
#include "core/actions/ForeachActionNode.h"
#include "common/Logger.h"
#include <algorithm>
#include <sstream>

using namespace SCXML::Parsing;
using namespace std;
using namespace SCXML::Core;

// Static member initialization
size_t ExecutableContentParser::actionIdCounter_ = 0;

ExecutableContentParser::ExecutableContentParser() {
    SCXML::Common::Logger::debug("ExecutableContentParser::Constructor - Creating SCXML action parser");
}

std::vector<std::shared_ptr<SCXML::Model::IActionNode>> ExecutableContentParser::parseExecutableContent(const xmlpp::Element* element) {
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> actions;
    
    if (!element) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseExecutableContent - Null element");
        return actions;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseExecutableContent - Parsing executable content in: " + element->get_name());
    
    // Parse all child elements as potential actions
    for (const auto& child : element->get_children()) {
        const auto* childElement = dynamic_cast<const xmlpp::Element*>(child);
        if (childElement) {
            auto action = parseAction(childElement);
            if (action) {
                actions.push_back(action);
                SCXML::Common::Logger::debug("ExecutableContentParser::parseExecutableContent - Added action: " + action->getId());
            }
        }
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseExecutableContent - Parsed " + std::to_string(actions.size()) + " actions");
    return actions;
}

std::shared_ptr<SCXML::Model::IActionNode> ExecutableContentParser::parseAction(const xmlpp::Element* element) {
    if (!element) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseAction - Null element");
        return nullptr;
    }
    
    std::string tagName = element->get_name();
    SCXML::Common::Logger::debug("ExecutableContentParser::parseAction - Parsing action: " + tagName);
    
    // Dispatch to specific parsers based on tag name
    if (tagName == "if") {
        return parseIfAction(element);
    } else if (tagName == "send") {
        return parseSendAction(element);
    } else if (tagName == "raise") {
        return parseRaiseAction(element);
    } else if (tagName == "assign") {
        return parseAssignAction(element);
    } else if (tagName == "log") {
        return parseLogAction(element);
    } else if (tagName == "cancel") {
        return parseCancelAction(element);
    } else if (tagName == "script") {
        return parseScriptAction(element);
    } else if (tagName == "foreach") {
        return parseForeachAction(element);
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseAction - Unsupported action type: " + tagName);
    return nullptr;
}

bool ExecutableContentParser::isSupportedAction(const xmlpp::Element* element) const {
    if (!element) {
        return false;
    }
    
    std::string tagName = element->get_name();
    return tagName == "if" || tagName == "send" || tagName == "raise" || 
           tagName == "assign" || tagName == "log" || tagName == "cancel" ||
           tagName == "script" || tagName == "foreach";
}

std::shared_ptr<SCXML::Model::IActionNode> ExecutableContentParser::parseIfAction(const xmlpp::Element* ifElement) {
    if (!ifElement) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseIfAction - Null if element");
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseIfAction - Parsing if action");
    
    // Create IfActionNode
    auto ifAction = std::make_shared<SCXML::Core::IfActionNode>(generateActionId("if"));
    
    // Parse main if condition
    std::string condition = getAttributeValue(ifElement, "cond");
    if (condition.empty()) {
        SCXML::Common::Logger::error("ExecutableContentParser::parseIfAction - Missing 'cond' attribute in if element");
        return nullptr;
    }
    
    ifAction->setIfCondition(condition);
    SCXML::Common::Logger::debug("ExecutableContentParser::parseIfAction - If condition: " + condition);
    
    // Parse conditional structure (if/elseif/else branches)
    parseConditionalStructure(ifAction, ifElement);
    
    // Validate the structure
    auto validationErrors = ifAction->validate();
    if (!validationErrors.empty()) {
        SCXML::Common::Logger::error("ExecutableContentParser::parseIfAction - Validation errors:");
        for (const auto& error : validationErrors) {
            SCXML::Common::Logger::error("  " + error);
        }
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseIfAction - If action parsed successfully");
    return ifAction;
}

std::shared_ptr<SCXML::Model::IActionNode> ExecutableContentParser::parseSendAction(const xmlpp::Element* sendElement) {
    if (!sendElement) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseSendAction - Null send element");
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseSendAction - Parsing send action");
    
    auto sendAction = std::make_shared<SCXML::Core::SendActionNode>(generateActionId("send"));
    
    // Parse send attributes
    sendAction->setEvent(getAttributeValue(sendElement, "event"));
    sendAction->setTarget(getAttributeValue(sendElement, "target", "#_internal"));
    sendAction->setData(getAttributeValue(sendElement, "data"));
    sendAction->setDelay(getAttributeValue(sendElement, "delay", "0ms"));
    sendAction->setSendId(getAttributeValue(sendElement, "id"));
    sendAction->setType(getAttributeValue(sendElement, "type", "platform"));
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseSendAction - Send action parsed: event=" + 
                 sendAction->getEvent() + ", target=" + sendAction->getTarget());
    
    return sendAction;
}

std::shared_ptr<SCXML::Model::IActionNode> ExecutableContentParser::parseRaiseAction(const xmlpp::Element* raiseElement) {
    if (!raiseElement) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseRaiseAction - Null raise element");
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseRaiseAction - Parsing raise action");
    
    auto raiseAction = std::make_shared<SCXML::Core::RaiseActionNode>(generateActionId("raise"));
    
    // Parse raise attributes
    std::string event = getAttributeValue(raiseElement, "event");
    if (event.empty()) {
        SCXML::Common::Logger::error("ExecutableContentParser::parseRaiseAction - Missing 'event' attribute");
        return nullptr;
    }
    
    raiseAction->setEvent(event);
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseRaiseAction - Raise action parsed: event=" + event);
    return raiseAction;
}

std::shared_ptr<SCXML::Model::IActionNode> ExecutableContentParser::parseAssignAction(const xmlpp::Element* assignElement) {
    if (!assignElement) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseAssignAction - Null assign element");
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseAssignAction - Parsing assign action");

    
    auto assignAction = std::make_shared<SCXML::Core::AssignActionNode>(generateActionId("assign"));

    
    // Parse assign attributes
    std::string location = getAttributeValue(assignElement, "location");
    std::string expr = getAttributeValue(assignElement, "expr");


    assignAction->setLocation(location);
    assignAction->setExpr(expr);
    assignAction->setAttr(getAttributeValue(assignElement, "attr"));
    assignAction->setType(getAttributeValue(assignElement, "type"));
    
    // Validation now handled by ActionExecutor at runtime
    // auto errors = assignAction->validate();
    // if (!errors.empty()) {
    //     SCXML::Common::Logger::error("ExecutableContentParser::parseAssignAction - Validation errors:");
    //     for (const auto& error : errors) {
    //         SCXML::Common::Logger::error("  " + error);
    //     }
    //     return nullptr;
    // }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseAssignAction - Assign action parsed successfully");
    return assignAction;
}

std::shared_ptr<SCXML::Model::IActionNode> ExecutableContentParser::parseLogAction(const xmlpp::Element* logElement) {
    if (!logElement) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseLogAction - Null log element");
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseLogAction - Parsing log action");
    
    auto logAction = std::make_shared<SCXML::Core::LogActionNode>(generateActionId("log"));
    
    // Parse log attributes
    logAction->setExpr(getAttributeValue(logElement, "expr"));
    logAction->setLabel(getAttributeValue(logElement, "label"));
    logAction->setLevel(getAttributeValue(logElement, "level", "info"));
    
    // Validation now handled by ActionExecutor at runtime
    // auto errors = logAction->validate();
    // if (!errors.empty()) {
    //     SCXML::Common::Logger::error("ExecutableContentParser::parseLogAction - Validation errors:");
    //     for (const auto& error : errors) {
    //         SCXML::Common::Logger::error("  " + error);
    //     }
    //     return nullptr;
    // }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseLogAction - Log action parsed successfully");
    return logAction;
}

std::shared_ptr<SCXML::Model::IActionNode> ExecutableContentParser::parseCancelAction(const xmlpp::Element* cancelElement) {
    if (!cancelElement) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseCancelAction - Null cancel element");
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseCancelAction - Parsing cancel action");
    
    auto cancelAction = std::make_shared<SCXML::Core::CancelActionNode>(generateActionId("cancel"));
    
    // Parse cancel attributes
    std::string sendId = getAttributeValue(cancelElement, "sendid");
    std::string sendIdExpr = getAttributeValue(cancelElement, "sendidexpr");
    
    if (!sendId.empty()) {
        cancelAction->setSendId(sendId);
    } else if (!sendIdExpr.empty()) {
        cancelAction->setSendIdExpr(sendIdExpr);
    } else {
        SCXML::Common::Logger::error("ExecutableContentParser::parseCancelAction - Missing 'sendid' or 'sendidexpr' attribute");
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseCancelAction - Cancel action parsed successfully");
    return cancelAction;
}

std::shared_ptr<SCXML::Model::IActionNode> ExecutableContentParser::parseScriptAction(const xmlpp::Element* scriptElement) {
    if (!scriptElement) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseScriptAction - Null script element");
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseScriptAction - Parsing script action");
    
    auto scriptAction = std::make_shared<SCXML::Core::ScriptActionNode>(generateActionId("script"));
    
    // Parse script attributes
    std::string src = getAttributeValue(scriptElement, "src");
    std::string lang = getAttributeValue(scriptElement, "type", "ecmascript");
    
    if (!src.empty()) {
        scriptAction->setSrc(src);
    } else {
        // Get inline script content
        std::string content = getTextContent(scriptElement);
        scriptAction->setContent(content);
    }
    
    scriptAction->setLang(lang);
    
    // Validate the action
    auto errors = scriptAction->validate();
    if (!errors.empty()) {
        SCXML::Common::Logger::error("ExecutableContentParser::parseScriptAction - Validation errors:");
        for (const auto& error : errors) {
            SCXML::Common::Logger::error("  " + error);
        }
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseScriptAction - Script action parsed successfully");
    return scriptAction;
}

std::shared_ptr<SCXML::Model::IActionNode> ExecutableContentParser::parseForeachAction(const xmlpp::Element* foreachElement) {
    if (!foreachElement) {
        SCXML::Common::Logger::warning("ExecutableContentParser::parseForeachAction - Null foreach element");
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseForeachAction - Parsing foreach action");
    
    auto foreachAction = std::make_shared<SCXML::Core::ForeachActionNode>(generateActionId("foreach"));
    
    // Parse foreach attributes
    foreachAction->setArray(getAttributeValue(foreachElement, "array"));
    foreachAction->setItem(getAttributeValue(foreachElement, "item"));
    foreachAction->setIndex(getAttributeValue(foreachElement, "index"));
    
    // Parse child actions to execute in each iteration
    auto childActions = parseChildActions(foreachElement);
    for (auto& childAction : childActions) {
        foreachAction->addIterationAction(childAction);
    }
    
    // Validate the action
    auto errors = foreachAction->validate();
    if (!errors.empty()) {
        SCXML::Common::Logger::error("ExecutableContentParser::parseForeachAction - Validation errors:");
        for (const auto& error : errors) {
            SCXML::Common::Logger::error("  " + error);
        }
        return nullptr;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseForeachAction - Foreach action parsed with " + 
                 std::to_string(childActions.size()) + " iteration actions");
    return foreachAction;
}

std::string ExecutableContentParser::getAttributeValue(const xmlpp::Element* element, 
                                               const std::string& attributeName, 
                                               const std::string& defaultValue) const {
    if (!element) {
        return defaultValue;
    }
    
    auto attribute = element->get_attribute(attributeName);
    return attribute ? attribute->get_value() : defaultValue;
}

std::string ExecutableContentParser::getTextContent(const xmlpp::Element* element) const {
    if (!element) {
        return "";
    }
    
    std::ostringstream content;
    
    for (const auto& child : element->get_children()) {
        const auto* textNode = dynamic_cast<const xmlpp::TextNode*>(child);
        if (textNode) {
            content << textNode->get_content();
        }
    }
    
    return content.str();
}

std::vector<const xmlpp::Element*> ExecutableContentParser::getChildElements(const xmlpp::Element* parent, 
                                                                       const std::string& childName) const {
    std::vector<const xmlpp::Element*> children;
    
    if (!parent) {
        return children;
    }
    
    for (const auto& child : parent->get_children()) {
        const auto* element = dynamic_cast<const xmlpp::Element*>(child);
        if (element && element->get_name() == childName) {
            children.push_back(element);
        }
    }
    
    return children;
}

void ExecutableContentParser::parseConditionalStructure(std::shared_ptr<SCXML::Core::IfActionNode> ifAction, 
                                                  const xmlpp::Element* ifElement) {
    if (!ifAction || !ifElement) {
        SCXML::Common::Logger::error("ExecutableContentParser::parseConditionalStructure - Null parameters");
        return;
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseConditionalStructure - Parsing conditional branches");
    
    // Parse content in the if element
    bool currentlyInIf = true;
    bool hasElseIfOrElse = false;
    
    for (const auto& child : ifElement->get_children()) {
        const auto* element = dynamic_cast<const xmlpp::Element*>(child);
        if (!element) {
            continue;
        }
        
        std::string tagName = element->get_name();
        
        if (tagName == "elseif") {
            hasElseIfOrElse = true;
            currentlyInIf = false;
            
            // Add elseif branch
            std::string condition = getAttributeValue(element, "cond");
            if (condition.empty()) {
                SCXML::Common::Logger::error("ExecutableContentParser::parseConditionalStructure - Missing 'cond' in elseif");
                continue;
            }
            
            auto& branch = ifAction->addElseIfBranch(condition);
            
            // Parse actions in elseif branch
            auto elseifActions = parseChildActions(element);
            for (auto& action : elseifActions) {
                branch.actions.push_back(action);
            }
            
            SCXML::Common::Logger::debug("ExecutableContentParser::parseConditionalStructure - Added elseif branch: " + condition);
            
        } else if (tagName == "else") {
            hasElseIfOrElse = true;
            currentlyInIf = false;
            
            // Add else branch
            auto& branch = ifAction->addElseBranch();
            
            // Parse actions in else branch
            auto elseActions = parseChildActions(element);
            for (auto& action : elseActions) {
                branch.actions.push_back(action);
            }
            
            SCXML::Common::Logger::debug("ExecutableContentParser::parseConditionalStructure - Added else branch");
            
        } else if (currentlyInIf) {
            // This is an action in the if branch
            auto action = parseAction(element);
            if (action) {
                ifAction->addIfAction(action);
                SCXML::Common::Logger::debug("ExecutableContentParser::parseConditionalStructure - Added action to if branch: " + 
                             action->getId());
            }
        }
    }
    
    // If no elseif/else were found, all direct child actions belong to if branch
    if (!hasElseIfOrElse) {
        auto ifActions = parseChildActions(ifElement);
        for (auto& action : ifActions) {
            ifAction->addIfAction(action);
        }
    }
    
    SCXML::Common::Logger::debug("ExecutableContentParser::parseConditionalStructure - Conditional structure parsed");
}

std::vector<std::shared_ptr<SCXML::Model::IActionNode>> ExecutableContentParser::parseChildActions(const xmlpp::Element* parent) {
    std::vector<std::shared_ptr<SCXML::Model::IActionNode>> actions;
    
    if (!parent) {
        return actions;
    }
    
    for (const auto& child : parent->get_children()) {
        const auto* element = dynamic_cast<const xmlpp::Element*>(child);
        if (element && isSupportedAction(element)) {
            auto action = parseAction(element);
            if (action) {
                actions.push_back(action);
            }
        }
    }
    
    return actions;
}

std::string ExecutableContentParser::generateActionId(const std::string& actionType) {
    return actionType + "_" + std::to_string(++actionIdCounter_);
}
