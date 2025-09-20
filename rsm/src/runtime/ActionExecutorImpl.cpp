#include "runtime/ActionExecutorImpl.h"
#include "actions/AssignAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "common/Logger.h"
#include "runtime/ExecutionContextImpl.h"
#include "scripting/JSEngine.h"
#include <regex>
#include <sstream>

namespace RSM {

ActionExecutorImpl::ActionExecutorImpl(const std::string &sessionId) : sessionId_(sessionId) {
    Logger::debug("ActionExecutorImpl created for session: {}", sessionId_);
}

bool ActionExecutorImpl::executeScript(const std::string &script) {
    if (script.empty()) {
        Logger::warn("Attempted to execute empty script");
        return true;  // Empty script is considered successful
    }

    if (!isSessionReady()) {
        Logger::error("Session {} not ready for script execution", sessionId_);
        return false;
    }

    try {
        // Ensure current event is available in JavaScript context
        ensureCurrentEventSet();

        auto result = JSEngine::instance().executeScript(sessionId_, script).get();

        if (!result.isSuccess()) {
            handleJSError("script execution", result.errorMessage);
            return false;
        }

        Logger::debug("Script executed successfully in session {}", sessionId_);
        return true;

    } catch (const std::exception &e) {
        handleJSError("script execution", e.what());
        return false;
    }
}

bool ActionExecutorImpl::assignVariable(const std::string &location, const std::string &expr) {
    if (location.empty()) {
        Logger::error("Cannot assign to empty location");
        return false;
    }

    if (!isValidLocation(location)) {
        Logger::error("Invalid variable location: {}", location);
        return false;
    }

    if (!isSessionReady()) {
        Logger::error("Session {} not ready for variable assignment", sessionId_);
        return false;
    }

    try {
        // First evaluate the expression
        auto evalResult = JSEngine::instance().evaluateExpression(sessionId_, expr).get();
        if (!evalResult.isSuccess()) {
            handleJSError("expression evaluation for assignment", evalResult.errorMessage);
            return false;
        }

        // Then assign the result to the location
        // For simple variable names, use setVariable
        // For complex paths like "data.field", use executeScript
        if (std::regex_match(location, std::regex("^[a-zA-Z_][a-zA-Z0-9_]*$"))) {
            // Simple variable name - use setVariable
            auto setResult = JSEngine::instance().setVariable(sessionId_, location, evalResult.value).get();
            if (!setResult.isSuccess()) {
                handleJSError("variable assignment", setResult.errorMessage);
                return false;
            }
        } else {
            // Complex path - use script assignment
            std::ostringstream assignScript;
            assignScript << location << " = (" << expr << ");";

            auto scriptResult = JSEngine::instance().executeScript(sessionId_, assignScript.str()).get();
            if (!scriptResult.isSuccess()) {
                handleJSError("complex variable assignment", scriptResult.errorMessage);
                return false;
            }
        }

        Logger::debug("Variable assigned: {} = {}", location, expr);
        return true;

    } catch (const std::exception &e) {
        handleJSError("variable assignment", e.what());
        return false;
    }
}

std::string ActionExecutorImpl::evaluateExpression(const std::string &expression) {
    if (expression.empty()) {
        return "";
    }

    if (!isSessionReady()) {
        Logger::error("Session {} not ready for expression evaluation", sessionId_);
        return "";
    }

    try {
        ensureCurrentEventSet();

        auto result = JSEngine::instance().evaluateExpression(sessionId_, expression).get();

        if (!result.isSuccess()) {
            handleJSError("expression evaluation", result.errorMessage);
            return "";
        }

        // Convert result to string based on type
        if (std::holds_alternative<std::string>(result.value)) {
            return result.getValue<std::string>();
        } else if (std::holds_alternative<double>(result.value)) {
            double val = result.getValue<double>();
            // Check if it's an integer value
            if (val == std::floor(val)) {
                return std::to_string(static_cast<int64_t>(val));
            } else {
                return std::to_string(val);
            }
        } else if (std::holds_alternative<int64_t>(result.value)) {
            return std::to_string(result.getValue<int64_t>());
        } else if (std::holds_alternative<bool>(result.value)) {
            return result.getValue<bool>() ? "true" : "false";
        } else {
            // For objects, try JSON.stringify
            std::string stringifyExpr = "JSON.stringify(" + expression + ")";
            auto stringifyResult = JSEngine::instance().evaluateExpression(sessionId_, stringifyExpr).get();
            if (stringifyResult.isSuccess()) {
                return stringifyResult.getValue<std::string>();
            }
            return "[object]";
        }

    } catch (const std::exception &e) {
        handleJSError("expression evaluation", e.what());
        return "";
    }
}

void ActionExecutorImpl::log(const std::string &level, const std::string &message) {
    // Map SCXML log levels to our logging system
    if (level == "error") {
        Logger::error("SCXML: {}", message);
    } else if (level == "warn") {
        Logger::warn("SCXML: {}", message);
    } else if (level == "debug") {
        Logger::debug("SCXML: {}", message);
    } else {
        Logger::info("SCXML: {}", message);
    }
}

bool ActionExecutorImpl::raiseEvent(const std::string &eventName, const std::string &eventData) {
    if (eventName.empty()) {
        Logger::error("Cannot raise event with empty name");
        return false;
    }

    if (eventRaiseCallback_) {
        bool result = eventRaiseCallback_(eventName, eventData);
        if (result) {
            Logger::debug("Event raised: {} with data: {}", eventName, eventData);
        } else {
            Logger::error("Failed to raise event: {}", eventName);
        }
        return result;
    } else {
        Logger::warn("No event raise callback set, cannot raise event: {}", eventName);
        return false;
    }
}

bool ActionExecutorImpl::hasVariable(const std::string &location) {
    if (location.empty() || !isSessionReady()) {
        return false;
    }

    try {
        // Use typeof to check if variable exists
        std::string checkExpr = "typeof " + location + " !== 'undefined'";
        auto result = JSEngine::instance().evaluateExpression(sessionId_, checkExpr).get();

        if (result.isSuccess() && std::holds_alternative<bool>(result.value)) {
            return result.getValue<bool>();
        }

        return false;

    } catch (const std::exception &e) {
        Logger::debug("Error checking variable existence: {}", e.what());
        return false;
    }
}

std::string ActionExecutorImpl::getSessionId() const {
    return sessionId_;
}

void ActionExecutorImpl::setEventRaiseCallback(std::function<bool(const std::string &, const std::string &)> callback) {
    eventRaiseCallback_ = callback;
}

void ActionExecutorImpl::setCurrentEvent(const std::string &eventName, const std::string &eventData) {
    currentEventName_ = eventName;
    currentEventData_ = eventData;

    // Update _event variable in JavaScript context
    ensureCurrentEventSet();
}

void ActionExecutorImpl::clearCurrentEvent() {
    currentEventName_.clear();
    currentEventData_.clear();

    // Update _event variable in JavaScript context
    ensureCurrentEventSet();
}

bool ActionExecutorImpl::isSessionReady() const {
    return JSEngine::instance().hasSession(sessionId_);
}

bool ActionExecutorImpl::isValidLocation(const std::string &location) const {
    if (location.empty()) {
        return false;
    }

    // Allow simple variable names and dot notation paths
    // This is a basic validation - could be enhanced
    std::regex locationPattern("^[a-zA-Z_][a-zA-Z0-9_]*(\\.[a-zA-Z_][a-zA-Z0-9_]*)*$");
    return std::regex_match(location, locationPattern);
}

void ActionExecutorImpl::handleJSError(const std::string &operation, const std::string &errorMessage) const {
    Logger::error("JavaScript {} failed in session {}: {}", operation, sessionId_, errorMessage);
}

bool ActionExecutorImpl::ensureCurrentEventSet() {
    if (!isSessionReady()) {
        return false;
    }

    try {
        // Create _event object with current event data
        std::ostringstream eventScript;
        eventScript << "_event = { name: '" << currentEventName_ << "', data: ";

        if (currentEventData_.empty()) {
            eventScript << "null";
        } else {
            // Try to parse as JSON, fallback to string
            eventScript << currentEventData_;
        }

        eventScript << ", type: '', sendid: '', origin: '', origintype: '', invokeid: '' };";

        auto result = JSEngine::instance().executeScript(sessionId_, eventScript.str()).get();
        return result.isSuccess();

    } catch (const std::exception &e) {
        Logger::debug("Error setting current event: {}", e.what());
        return false;
    }
}

// High-level action execution methods (Command pattern)

bool ActionExecutorImpl::executeScriptAction(const ScriptAction &action) {
    Logger::debug("ActionExecutorImpl::executeScriptAction - Executing script action: {}", action.getId());
    return executeScript(action.getContent());
}

bool ActionExecutorImpl::executeAssignAction(const AssignAction &action) {
    Logger::debug("ActionExecutorImpl::executeAssignAction - Executing assign action: {}", action.getId());
    return assignVariable(action.getLocation(), action.getExpr());
}

bool ActionExecutorImpl::executeLogAction(const LogAction &action) {
    Logger::debug("ActionExecutorImpl::executeLogAction - Executing log action: {}", action.getId());

    try {
        // Evaluate the expression to get the log message
        std::string message;
        if (!action.getExpr().empty()) {
            message = evaluateExpression(action.getExpr());
            if (message.empty()) {
                Logger::warn("Log expression evaluated to empty string: {}", action.getExpr());
                message = action.getExpr();  // Fallback to raw expression
            }
        }

        // Add label prefix if specified
        if (!action.getLabel().empty()) {
            message = action.getLabel() + ": " + message;
        }

        // Log with specified level
        std::string level = action.getLevel().empty() ? "info" : action.getLevel();
        log(level, message);

        return true;
    } catch (const std::exception &e) {
        Logger::error("Failed to execute log action: {}", e.what());
        return false;
    }
}

bool ActionExecutorImpl::executeRaiseAction(const RaiseAction &action) {
    Logger::debug("ActionExecutorImpl::executeRaiseAction - Executing raise action: {}", action.getId());

    if (action.getEvent().empty()) {
        Logger::error("Raise action has empty event name");
        return false;
    }

    try {
        // Evaluate data expression if provided
        std::string eventData;
        if (!action.getData().empty()) {
            eventData = evaluateExpression(action.getData());
            if (eventData.empty()) {
                Logger::warn("Raise action data expression evaluated to empty: {}", action.getData());
                eventData = action.getData();  // Fallback to raw data
            }
        }

        return raiseEvent(action.getEvent(), eventData);
    } catch (const std::exception &e) {
        Logger::error("Failed to execute raise action: {}", e.what());
        return false;
    }
}

bool ActionExecutorImpl::executeIfAction(const IfAction &action) {
    Logger::debug("ActionExecutorImpl::executeIfAction - Executing if action: {}", action.getId());

    try {
        const auto &branches = action.getBranches();
        if (branches.empty()) {
            Logger::warn("If action has no branches");
            return true;  // Empty if is valid but does nothing
        }

        // Evaluate conditions in order and execute first matching branch
        for (const auto &branch : branches) {
            bool shouldExecute = false;

            if (branch.isElseBranch) {
                // Else branch - always execute
                shouldExecute = true;
                Logger::debug("Executing else branch");
            } else if (!branch.condition.empty()) {
                // Evaluate condition
                shouldExecute = evaluateCondition(branch.condition);
                Logger::debug("Condition '{}' evaluated to: {}", branch.condition, shouldExecute);
            } else {
                Logger::warn("Branch has empty condition and is not else branch");
                continue;
            }

            if (shouldExecute) {
                // Execute all actions in this branch
                bool allSucceeded = true;

                // Create execution context for nested actions
                auto sharedThis = std::shared_ptr<IActionExecutor>(this, [](IActionExecutor *) {});
                ExecutionContextImpl context(sharedThis, sessionId_);

                for (const auto &branchAction : branch.actions) {
                    if (branchAction && !branchAction->execute(context)) {
                        Logger::error("Failed to execute action in if branch");
                        allSucceeded = false;
                    }
                }
                return allSucceeded;  // Stop after first matching branch
            }
        }

        // No branch matched
        Logger::debug("No branch condition matched in if action");
        return true;
    } catch (const std::exception &e) {
        Logger::error("Failed to execute if action: {}", e.what());
        return false;
    }
}

bool ActionExecutorImpl::evaluateCondition(const std::string &condition) {
    if (condition.empty()) {
        return true;  // Empty condition is always true
    }

    try {
        auto result = JSEngine::instance().evaluateExpression(sessionId_, condition).get();

        if (!result.success) {
            Logger::error("Failed to evaluate condition '{}': {}", condition, result.errorMessage);
            return false;
        }

        // Convert result to boolean
        if (std::holds_alternative<bool>(result.value)) {
            return std::get<bool>(result.value);
        } else if (std::holds_alternative<long>(result.value)) {
            return std::get<long>(result.value) != 0;
        } else if (std::holds_alternative<double>(result.value)) {
            return std::get<double>(result.value) != 0.0;
        } else if (std::holds_alternative<std::string>(result.value)) {
            return !std::get<std::string>(result.value).empty();
        }

        return false;
    } catch (const std::exception &e) {
        Logger::error("Exception evaluating condition '{}': {}", condition, e.what());
        return false;
    }
}

}  // namespace RSM