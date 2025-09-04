#include "core/SendNode.h"
#include "common/Logger.h"
#include "events/Event.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/LocationExpressionEvaluator.h"
#include <algorithm>
#include <chrono>
#include <regex>

namespace SCXML {
namespace Core {

SendNode::SendNode()
    : event_(""), target_(""), type_("scxml"), id_(""), delay_("0ms"), content_(""), contentExpr_("") {}

SCXML::Common::Result<void> SendNode::execute(SCXML::Model::IExecutionContext &context) {
    try {
        // Validate the send node before execution
        auto validationErrors = validate();
        if (!validationErrors.empty()) {
            std::string errorMsg = "Send node validation failed: ";
            for (const auto &error : validationErrors) {
                errorMsg += error + "; ";
            }
            return SCXML::Common::Result<void>::failure(errorMsg);
        }

        // Resolve event name
        auto eventNameResult = resolveEventName(context);
        if (!eventNameResult.isSuccess()) {
            return SCXML::Common::Result<void>::failure("Failed to resolve event name: " + eventNameResult.getErrors()[0].message);
        }
        const std::string &eventName = eventNameResult.getValue();

        // Resolve target
        auto targetResult = resolveTarget(context);
        if (!targetResult.isSuccess()) {
            return SCXML::Common::Result<void>::failure("Failed to resolve target: " + targetResult.getErrors()[0].message);
        }
        // Target resolved but not used in simplified implementation

        // Resolve event data
        auto eventDataResult = resolveEventData(context);
        if (!eventDataResult.isSuccess()) {
            return SCXML::Common::Result<void>::failure("Failed to resolve event data: " + eventDataResult.getErrors()[0].message);
        }
        const std::string &eventData = eventDataResult.getValue();

        // Create event to send
        SCXML::Events::Event eventToSend(eventName);
        eventToSend.setData(eventData);
        
        // TODO: Handle target, type, and send ID properly according to Event interface

        // Handle delay
        auto delayResult = resolveDelay(context);
        if (!delayResult.isSuccess()) {
            return SCXML::Common::Result<void>::failure("Failed to resolve delay: " + delayResult.getErrors()[0].message);
        }

        int delayMs = delayResult.getValue();
        if (delayMs > 0) {
            // Schedule delayed send - TODO: implement delayed send mechanism
            return SCXML::Common::Result<void>::success();
        } else {
            // Send immediately - TODO: implement event sending mechanism
            return SCXML::Common::Result<void>::success();
        }

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception during send execution: " + std::string(e.what()));
    }
}

const std::string &SendNode::getEvent() const {
    return event_;
}

void SendNode::setEvent(const std::string &event) {
    event_ = event;
}

const std::string &SendNode::getTarget() const {
    return target_;
}

void SendNode::setTarget(const std::string &target) {
    target_ = target;
}

const std::string &SendNode::getType() const {
    return type_;
}

void SendNode::setType(const std::string &type) {
    type_ = type;
}

const std::string &SendNode::getId() const {
    return id_;
}

void SendNode::setId(const std::string &id) {
    id_ = id;
}

const std::string &SendNode::getDelay() const {
    return delay_;
}

void SendNode::setDelay(const std::string &delay) {
    delay_ = delay;
}

const std::string &SendNode::getContent() const {
    return content_;
}

void SendNode::setContent(const std::string &content) {
    content_ = content;
}

const std::string &SendNode::getContentExpr() const {
    return contentExpr_;
}

void SendNode::setContentExpr(const std::string &contentExpr) {
    contentExpr_ = contentExpr;
}

const std::map<std::string, std::string> &SendNode::getNameLocationPairs() const {
    return nameLocationPairs_;
}

void SendNode::addNameLocationPair(const std::string &name, const std::string &location) {
    nameLocationPairs_[name] = location;
}

std::vector<std::string> SendNode::validate() const {
    std::vector<std::string> errors;

    // Event name is required
    if (event_.empty()) {
        errors.push_back("Event name is required");
    }

    // Cannot have both content and contentexpr
    if (!content_.empty() && !contentExpr_.empty()) {
        errors.push_back("Cannot specify both content and contentexpr");
    }

    // Cannot have content/contentexpr and namelocation pairs
    if ((!content_.empty() || !contentExpr_.empty()) && !nameLocationPairs_.empty()) {
        errors.push_back("Cannot specify content/contentexpr with namelocation pairs");
    }

    // Validate event processor type
    if (!isValidEventProcessorType(type_)) {
        errors.push_back("Invalid event processor type: " + type_);
    }

    // Validate delay format
    if (!delay_.empty() && delay_ != "0ms") {
        if (!isValidDelayFormat(delay_)) {
            errors.push_back("Invalid delay format: " + delay_);
        }
    }

    return errors;
}

std::shared_ptr<ISendNode> SendNode::clone() const {
    auto cloned = std::make_shared<SendNode>();
    cloned->event_ = event_;
    cloned->target_ = target_;
    cloned->type_ = type_;
    cloned->id_ = id_;
    cloned->delay_ = delay_;
    cloned->content_ = content_;
    cloned->contentExpr_ = contentExpr_;
    cloned->nameLocationPairs_ = nameLocationPairs_;
    return cloned;
}

bool SendNode::isDelayed() const {
    if (delay_.empty() || delay_ == "0ms") {
        return false;
    }

    // Check if delay resolves to > 0
    // For now, simple string check - in full implementation would evaluate expression
    return delay_ != "0" && delay_ != "0ms" && delay_ != "0s";
}

bool SendNode::isInternalTarget() const {
    return target_.empty() || target_ == "#_internal" || target_ == "_internal";
}

SCXML::Common::Result<std::string> SendNode::resolveEventData(SCXML::Model::IExecutionContext &/*context*/) const {
    try {
        // Priority: content > contentexpr > namelocation pairs
        if (!content_.empty()) {
            return SCXML::Common::Result<std::string>::success(content_);
        }

        if (!contentExpr_.empty()) {
            // Evaluate content expression using runtime context
            try {
                SCXML::Runtime::ExpressionEvaluator evaluator(context);
                auto result = evaluator.evaluateExpression(contentExpr_);
                if (result.isSuccess()) {
                    return SCXML::Common::Result<std::string>::success(result.getValue());
                } else {
                    return SCXML::Common::Result<std::string>::failure("Failed to evaluate contentexpr: " + result.getErrors()[0].message);
                }
            } catch (const std::exception& e) {
                return SCXML::Common::Result<std::string>::failure("Exception evaluating contentexpr: " + std::string(e.what()));
            }
        }

        if (!nameLocationPairs_.empty()) {
            // Build JSON object from name-location pairs
            try {
                std::string jsonData = "{";
                bool first = true;
                
                SCXML::Runtime::LocationExpressionEvaluator locationEvaluator(context);
                
                for (const auto& pair : nameLocationPairs_) {
                    if (!first) {
                        jsonData += ",";
                    }
                    first = false;
                    
                    // Evaluate location expression to get value
                    auto valueResult = locationEvaluator.evaluateLocation(pair.second);
                    std::string value;
                    
                    if (valueResult.isSuccess()) {
                        value = valueResult.getValue();
                        // Escape quotes in value for JSON
                        std::string escapedValue;
                        for (char c : value) {
                            if (c == '"') {
                                escapedValue += "\\\"";
                            } else if (c == '\\') {
                                escapedValue += "\\\\";
                            } else {
                                escapedValue += c;
                            }
                        }
                        value = escapedValue;
                    } else {
                        // If evaluation fails, use empty string
                        value = "";
                    }
                    
                    jsonData += "\"" + pair.first + "\":\"" + value + "\"";
                }
                
                jsonData += "}";
                return SCXML::Common::Result<std::string>::success(jsonData);
            } catch (const std::exception& e) {
                return SCXML::Common::Result<std::string>::failure("Exception resolving name-location pairs: " + std::string(e.what()));
            }
        }

        // No data specified
        return SCXML::Common::Result<std::string>::success("");

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::failure("Exception resolving event data: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::string> SendNode::resolveTarget(SCXML::Model::IExecutionContext &context) const {
    if (target_.empty()) {
        return SCXML::Common::Result<std::string>::success("#_internal");
    }

    try {
        // If target looks like an expression (contains spaces, operators, or starts with certain chars)
        if (target_.find(' ') != std::string::npos || 
            target_.find('+') != std::string::npos || 
            target_.find('*') != std::string::npos ||
            target_.find('(') != std::string::npos) {
            
            // Evaluate as expression
            SCXML::Runtime::ExpressionEvaluator evaluator(context);
            auto result = evaluator.evaluateExpression(target_);
            
            if (result.isSuccess()) {
                return SCXML::Common::Result<std::string>::success(result.getValue());
            } else {
                return SCXML::Common::Result<std::string>::failure("Failed to evaluate target expression: " + result.getErrors()[0].message);
            }
        } else {
            // Use as literal target URI
            return SCXML::Common::Result<std::string>::success(target_);
        }
    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::failure("Exception resolving target: " + std::string(e.what()));
    }
}

SCXML::Common::Result<std::string> SendNode::resolveEventName(SCXML::Model::IExecutionContext &context) const {
    if (event_.empty()) {
        return SCXML::Common::Result<std::string>::failure("Event name is empty");
    }

    try {
        // If event name looks like an expression (contains spaces, operators, or starts with certain chars)
        if (event_.find(' ') != std::string::npos || 
            event_.find('+') != std::string::npos || 
            event_.find('*') != std::string::npos ||
            event_.find('(') != std::string::npos ||
            event_.find('.') != std::string::npos) {
            
            // Evaluate as expression
            SCXML::Runtime::ExpressionEvaluator evaluator(context);
            auto result = evaluator.evaluateExpression(event_);
            
            if (result.isSuccess()) {
                return SCXML::Common::Result<std::string>::success(result.getValue());
            } else {
                return SCXML::Common::Result<std::string>::failure("Failed to evaluate event name expression: " + result.getErrors()[0].message);
            }
        } else {
            // Use as literal event name
            return SCXML::Common::Result<std::string>::success(event_);
        }
    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::failure("Exception resolving event name: " + std::string(e.what()));
    }
}

SCXML::Common::Result<int> SendNode::resolveDelay(SCXML::Model::IExecutionContext &context) const {
    if (delay_.empty() || delay_ == "0ms") {
        return SCXML::Common::Result<int>::success(0);
    }

    try {
        // Check if delay looks like an expression (contains spaces, operators, variables, etc.)
        if (delay_.find(' ') != std::string::npos || 
            delay_.find('+') != std::string::npos || 
            delay_.find('*') != std::string::npos ||
            delay_.find('(') != std::string::npos ||
            delay_.find('.') != std::string::npos ||
            (delay_.find_first_of("0123456789") != 0 && delay_ != "0")) {  // Doesn't start with digit
            
            // Evaluate as expression first
            SCXML::Runtime::ExpressionEvaluator evaluator(context);
            auto exprResult = evaluator.evaluateExpression(delay_);
            
            if (exprResult.isSuccess()) {
                // Parse the evaluated result as delay string
                return parseDelayString(exprResult.getValue());
            } else {
                return SCXML::Common::Result<int>::failure("Failed to evaluate delay expression: " + exprResult.getErrors()[0].message);
            }
        } else {
            // Parse as literal delay string
            return parseDelayString(delay_);
        }
    } catch (const std::exception &e) {
        return SCXML::Common::Result<int>::failure("Exception resolving delay: " + std::string(e.what()));
    }
}

SCXML::Common::Result<int> SendNode::parseDelayString(const std::string &delayStr) const {
    std::regex delayRegex(R"((\d+(?:\.\d+)?)\s*(ms|s|m|h)?)");
    std::smatch match;

    if (!std::regex_match(delayStr, match, delayRegex)) {
        return SCXML::Common::Result<int>::failure("Invalid delay format: " + delayStr);
    }

    double value = std::stod(match[1].str());
    std::string unit = match[2].str();

    if (unit.empty() || unit == "ms") {
        return SCXML::Common::Result<int>::success(static_cast<int>(value));
    } else if (unit == "s") {
        return SCXML::Common::Result<int>::success(static_cast<int>(value * 1000));
    } else if (unit == "m") {
        return SCXML::Common::Result<int>::success(static_cast<int>(value * 60000));
    } else if (unit == "h") {
        return SCXML::Common::Result<int>::success(static_cast<int>(value * 3600000));
    }

    return SCXML::Common::Result<int>::failure("Unknown time unit: " + unit);
}

bool SendNode::isValidEventProcessorType(const std::string &type) const {
    static const std::vector<std::string> validTypes = {"scxml", "basichttp", "http", "websocket", "internal"};

    return std::find(validTypes.begin(), validTypes.end(), type) != validTypes.end();
}

bool SendNode::isValidDelayFormat(const std::string &delay) const {
    std::regex delayRegex(R"(\d+(?:\.\d+)?\s*(?:ms|s|m|h)?)");
    return std::regex_match(delay, delayRegex);
}

// ============================================================================
// CancelNode Implementation
// ============================================================================

CancelNode::CancelNode() : sendId_(""), sendIdExpr_("") {}

SCXML::Common::Result<void> CancelNode::execute(SCXML::Model::IExecutionContext &context) {
    try {
        // Validate the cancel node before execution
        auto validationErrors = validate();
        if (!validationErrors.empty()) {
            std::string errorMsg = "Cancel node validation failed: ";
            for (const auto &error : validationErrors) {
                errorMsg += error + "; ";
            }
            return SCXML::Common::Result<void>::failure(errorMsg);
        }

        // Resolve send ID
        auto sendIdResult = resolveSendId(context);
        if (!sendIdResult.isSuccess()) {
            return SCXML::Common::Result<void>::failure("Failed to resolve send ID: " + sendIdResult.getErrors()[0].message);
        }

        // Send ID resolved but not used in simplified implementation

        // Cancel the scheduled send - TODO: implement cancel mechanism
        return SCXML::Common::Result<void>::success();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>::failure("Exception during cancel execution: " + std::string(e.what()));
    }
}

const std::string &CancelNode::getSendId() const {
    return sendId_;
}

void CancelNode::setSendId(const std::string &sendId) {
    sendId_ = sendId;
}

const std::string &CancelNode::getSendIdExpr() const {
    return sendIdExpr_;
}

void CancelNode::setSendIdExpr(const std::string &sendIdExpr) {
    sendIdExpr_ = sendIdExpr;
}

std::vector<std::string> CancelNode::validate() const {
    std::vector<std::string> errors;

    // Must have either sendId or sendIdExpr
    if (sendId_.empty() && sendIdExpr_.empty()) {
        errors.push_back("Either sendid or sendidexpr must be specified");
    }

    // Cannot have both sendId and sendIdExpr
    if (!sendId_.empty() && !sendIdExpr_.empty()) {
        errors.push_back("Cannot specify both sendid and sendidexpr");
    }

    return errors;
}

std::shared_ptr<ICancelNode> CancelNode::clone() const {
    auto cloned = std::make_shared<CancelNode>();
    cloned->sendId_ = sendId_;
    cloned->sendIdExpr_ = sendIdExpr_;
    return cloned;
}

SCXML::Common::Result<std::string> CancelNode::resolveSendId(SCXML::Model::IExecutionContext &/*context*/) const {
    try {
        if (!sendId_.empty()) {
            // TODO: implement expression evaluation with proper context
            return SCXML::Common::Result<std::string>::success(sendId_);
        }

        if (!sendIdExpr_.empty()) {
            // TODO: implement expression evaluation with proper context
            return SCXML::Common::Result<std::string>::success(sendIdExpr_);
        }

        return SCXML::Common::Result<std::string>::failure("No send ID specified");

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>::failure("Exception resolving send ID: " + std::string(e.what()));
    }
}

}  // namespace Core
}  // namespace SCXML
