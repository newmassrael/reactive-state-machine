#include "runtime/ActionExecutorImpl.h"
#include "actions/AssignAction.h"
#include "actions/CancelAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "actions/SendAction.h"
#include "common/Logger.h"
#include "events/EventDescriptor.h"
#include "events/IEventDispatcher.h"
#include "runtime/ExecutionContextImpl.h"
#include "scripting/JSEngine.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <regex>
#include <sstream>

namespace RSM {

ActionExecutorImpl::ActionExecutorImpl(const std::string &sessionId, std::shared_ptr<IEventDispatcher> eventDispatcher)
    : sessionId_(sessionId), eventDispatcher_(std::move(eventDispatcher)) {
    // Create EventRaiser instance
    eventRaiser_ = std::make_shared<EventRaiserImpl>();
    Logger::debug("ActionExecutorImpl created for session: {} at address: {}", sessionId_, static_cast<void *>(this));
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

    // SCXML Compliance: Support both expression and literal values
    // If the value doesn't look like an expression, treat it as a literal
    if (!isExpression(expression)) {
        Logger::debug("Treating '{}' as literal value", expression);

        // Process literal values - remove quotes from string literals
        if (expression.length() >= 2) {
            bool isQuoted = (expression.front() == '"' && expression.back() == '"') ||
                            (expression.front() == '\'' && expression.back() == '\'');
            if (isQuoted) {
                // Remove surrounding quotes for string literals
                return expression.substr(1, expression.length() - 2);
            }
        }

        return expression;
    }

    if (!isSessionReady()) {
        Logger::error("Session {} not ready for expression evaluation", sessionId_);
        // SCXML Compliance: Return empty string for expressions when session not ready
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
        // SCXML 3.12.1: Generate error.execution event for infrastructure failures
        Logger::error(
            "ActionExecutorImpl: EventRaiseCallback not available - this indicates incomplete SCXML processor setup");

        // SCXML Compliance: Infrastructure failures should not crash the processor
        // Return false to indicate the specific operation failed, but don't crash
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

    // Also set the callback in EventRaiser for dependency injection
    if (eventRaiser_) {
        eventRaiser_->setEventCallback(callback);
    }
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
    // SCXML Compliance: Check if JSEngine is available without blocking
    try {
        auto &jsEngine = JSEngine::instance();
        Logger::debug("ActionExecutorImpl: Using JSEngine at address: {}", static_cast<void *>(&jsEngine));
        // Use a non-blocking check - if JSEngine is not properly initialized,
        // we should not block indefinitely
        bool hasSessionResult = jsEngine.hasSession(sessionId_);
        Logger::debug("ActionExecutorImpl: hasSession({}) returned: {}", sessionId_, hasSessionResult);

        // Additional verification: check active sessions
        auto activeSessions = jsEngine.getActiveSessions();
        Logger::debug("ActionExecutorImpl: Active sessions count: {}", activeSessions.size());
        for (const auto &session : activeSessions) {
            Logger::debug("ActionExecutorImpl: Active session: {}", session);
        }

        return hasSessionResult;
    } catch (const std::exception &e) {
        // If JSEngine is not available, consider session not ready
        Logger::warn("JSEngine not available for session check: {}", e.what());
        return false;
    }
}

void ActionExecutorImpl::setEventDispatcher(std::shared_ptr<IEventDispatcher> eventDispatcher) {
    eventDispatcher_ = std::move(eventDispatcher);
    Logger::debug("ActionExecutorImpl: Event dispatcher set for session: {}", sessionId_);
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

bool ActionExecutorImpl::isExpression(const std::string &value) const {
    if (value.empty()) {
        return false;
    }

    // 1단계: 명백한 리터럴들을 빠르게 판별
    if (isObviousLiteral(value)) {
        return false;
    }

    // 2단계: 명백한 표현식들을 빠르게 판별
    if (isObviousExpression(value)) {
        return true;
    }

    // 3단계: 애매한 경우만 JSEngine으로 정확히 검증
    return validateWithJSEngine(value);
}

bool ActionExecutorImpl::isObviousLiteral(const std::string &value) const {
    // 명백한 리터럴 값들 (빠른 판별)
    if (value == "true" || value == "false" || value == "null" || value == "undefined") {
        return true;
    }

    // 숫자 (정수 또는 소수)
    std::regex numberPattern(R"(^-?[0-9]+(\.[0-9]+)?$)");
    if (std::regex_match(value, numberPattern)) {
        return true;
    }

    // 따옴표로 둘러싸인 순수한 문자열 리터럴 (연산자나 함수 호출이 없는 경우만)
    if (value.length() >= 2) {
        bool isQuoted =
            (value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'');
        if (isQuoted) {
            // 내부에 연산자나 특수 문자가 있는지 확인
            std::string content = value.substr(1, value.length() - 2);
            // 연산자나 함수 호출 패턴이 있으면 표현식
            if (content.find('+') != std::string::npos || content.find('-') != std::string::npos ||
                content.find('*') != std::string::npos || content.find('/') != std::string::npos ||
                content.find('(') != std::string::npos || content.find(')') != std::string::npos ||
                content.find('[') != std::string::npos || content.find(']') != std::string::npos ||
                content.find('.') != std::string::npos) {
                return false;  // 표현식일 가능성이 높음
            }
            return true;  // 순수한 문자열 리터럴
        }
    }

    return false;
}

bool ActionExecutorImpl::isObviousExpression(const std::string &value) const {
    // SCXML 시스템 변수들
    if (value.find("_event") != std::string::npos || value.find("_sessionid") != std::string::npos ||
        value.find("_name") != std::string::npos || value.find("_ioprocessors") != std::string::npos) {
        return true;
    }

    // 함수 호출 패턴
    if (value.find('(') != std::string::npos && value.find(')') != std::string::npos) {
        return true;
    }

    // 명백한 연산자들
    if (value.find(" + ") != std::string::npos || value.find(" - ") != std::string::npos ||
        value.find(" * ") != std::string::npos || value.find(" / ") != std::string::npos ||
        value.find(" % ") != std::string::npos || value.find(" && ") != std::string::npos ||
        value.find(" || ") != std::string::npos || value.find(" == ") != std::string::npos ||
        value.find(" != ") != std::string::npos || value.find(" === ") != std::string::npos ||
        value.find(" !== ") != std::string::npos || value.find(" < ") != std::string::npos ||
        value.find(" > ") != std::string::npos || value.find(" <= ") != std::string::npos ||
        value.find(" >= ") != std::string::npos) {
        return true;
    }

    // 객체/배열 접근 패턴
    if (value.find('.') != std::string::npos ||
        (value.find('[') != std::string::npos && value.find(']') != std::string::npos)) {
        // 단순 숫자가 아닌 경우에만
        std::regex simpleNumberPattern(R"(^[0-9]+\.[0-9]+$)");
        if (!std::regex_match(value, simpleNumberPattern)) {
            return true;
        }
    }

    return false;
}

bool ActionExecutorImpl::validateWithJSEngine(const std::string &value) const {
    // 단일 lock으로 체크 후 바로 삽입 (race condition 방지)
    std::lock_guard<std::mutex> lock(expressionCacheMutex_);

    // 캐시에서 먼저 확인
    auto it = expressionCache_.find(value);
    if (it != expressionCache_.end()) {
        return it->second;
    }

    // 캐시에 없으면 JSEngine으로 검증 (lock 잡은 상태에서)
    bool isValidExpression = false;
    try {
        auto jsEngine = &JSEngine::instance();
        if (jsEngine && jsEngine->hasSession(sessionId_)) {
            // lock을 잡은 상태에서 JSEngine 호출 - 같은 표현식의 중복 검증 방지
            auto result = jsEngine->validateExpression(sessionId_, value).get();
            isValidExpression = result.isSuccess();
        }
    } catch (const std::exception &e) {
        Logger::debug("ActionExecutor: Expression validation failed for '{}': {}", value, e.what());
        isValidExpression = false;
    }

    // 결과를 캐시에 저장
    expressionCache_[value] = isValidExpression;

    // 캐시 크기 제한 (메모리 관리)
    if (expressionCache_.size() > 1000) {
        expressionCache_.clear();
    }

    return isValidExpression;
}

void ActionExecutorImpl::handleJSError(const std::string &operation, const std::string &errorMessage) const {
    Logger::error("JavaScript {} failed in session {}: {}", operation, sessionId_, errorMessage);
}

bool ActionExecutorImpl::ensureCurrentEventSet() {
    if (!isSessionReady()) {
        return false;
    }

    try {
        // Create _event object using internal _updateEvent function (SCXML W3C compliance)
        std::ostringstream eventScript;
        eventScript << "_updateEvent({ name: '" << currentEventName_ << "', data: ";

        if (currentEventData_.empty()) {
            eventScript << "null";
        } else {
            // Try to parse as JSON, fallback to string
            eventScript << currentEventData_;
        }

        eventScript << ", type: '', sendid: '', origin: '', origintype: '', invokeid: '' });";

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

bool ActionExecutorImpl::executeSendAction(const SendAction &action) {
    Logger::debug("ActionExecutorImpl::executeSendAction - Executing send action: {}", action.getId());

    try {
        // CRITICAL: Complete ALL JSEngine operations first to avoid deadlock
        // Evaluate all expressions before calling EventDispatcher

        // Determine event name
        std::string eventName;
        if (!action.getEvent().empty()) {
            eventName = action.getEvent();
        } else if (!action.getEventExpr().empty()) {
            eventName = evaluateExpression(action.getEventExpr());
            if (eventName.empty()) {
                Logger::error("Send action eventexpr evaluated to empty: {}", action.getEventExpr());
                return false;
            }
        } else {
            Logger::error("Send action has no event or eventexpr");
            return false;
        }

        // Determine target
        std::string target = action.getTarget();
        if (target.empty() && !action.getTargetExpr().empty()) {
            target = evaluateExpression(action.getTargetExpr());
        }
        // SCXML 6.2.4: If target is empty, event is sent to current session (session-scoped)
        if (target.empty()) {
            // Session-scoped target - event sent to current session's internal queue
            target = "#_scxml_" + sessionId_;
        }

        // Evaluate data if provided
        std::string eventData;
        if (!action.getData().empty()) {
            eventData = evaluateExpression(action.getData());
        }

        // Parse delay (evaluate delay expression if needed)
        std::chrono::milliseconds delay{0};
        if (!action.getDelay().empty()) {
            delay = action.parseDelayString(action.getDelay());
        } else if (!action.getDelayExpr().empty()) {
            std::string delayStr = evaluateExpression(action.getDelayExpr());
            if (!delayStr.empty()) {
                delay = action.parseDelayString(delayStr);
            }
        }

        // ALL JSEngine operations complete - now safe to call EventDispatcher

        // SCXML Event System: Use event dispatcher if available
        if (eventDispatcher_) {
            Logger::debug("ActionExecutorImpl: Using event dispatcher for send action");

            // Create event descriptor
            EventDescriptor event;
            event.eventName = eventName;
            event.target = target;
            event.data = eventData;
            event.delay = delay;
            // SCXML 6.2.4: Auto-generate sendid if not provided
            if (!action.getSendId().empty()) {
                event.sendId = action.getSendId();
            } else {
                // Generate unique sendid as required by SCXML specification
                event.sendId = generateUniqueSendId();
            }

            // Send via dispatcher (handles both immediate and delayed events)
            auto resultFuture = eventDispatcher_->sendEvent(event);

            // SCXML 6.2.4: "Fire and forget" semantics - return immediately after queuing
            Logger::debug("ActionExecutorImpl: Send action queued successfully for event: {}", eventName);

            // SCXML Compliance: Don't wait for delivery result in fire-and-forget model
            // The event has been successfully queued, which is all we need to know
            return true;
        } else {
            // SCXML 3.12.1: Generate error.execution event instead of throwing
            Logger::error("ActionExecutorImpl: EventDispatcher not available for send action - generating error event");

            // SCXML Compliance: Generate error event for infrastructure failures
            if (eventRaiseCallback_) {
                eventRaiseCallback_("error.execution", "EventDispatcher not available for send action");
            }

            // SCXML send actions should follow fire-and-forget - infrastructure failures don't affect action success
            return true;  // Fire and forget semantics
        }

    } catch (const std::exception &e) {
        Logger::error("Failed to execute send action: {}", e.what());
        return false;
    }
}

bool ActionExecutorImpl::executeCancelAction(const CancelAction &action) {
    Logger::debug("ActionExecutorImpl::executeCancelAction - Executing cancel action: {}", action.getId());

    try {
        // Determine sendId to cancel
        std::string sendId;
        if (!action.getSendId().empty()) {
            sendId = action.getSendId();
        } else if (!action.getSendIdExpr().empty()) {
            sendId = evaluateExpression(action.getSendIdExpr());
            if (sendId.empty()) {
                Logger::error("Cancel action sendidexpr evaluated to empty: {}", action.getSendIdExpr());
                return false;
            }
        } else {
            Logger::error("Cancel action has no sendid or sendidexpr");
            return false;
        }

        // SCXML Event System: Use event dispatcher if available
        if (eventDispatcher_) {
            Logger::debug("ActionExecutorImpl: Using event dispatcher for cancel action");

            bool cancelled = eventDispatcher_->cancelEvent(sendId);
            if (cancelled) {
                Logger::info("ActionExecutorImpl: Successfully cancelled event with sendId: {}", sendId);
                return true;
            } else {
                Logger::info("ActionExecutorImpl: Event with sendId '{}' not found or already executed", sendId);
                // W3C SCXML: Cancelling non-existent events is not an error
                return true;
            }
        } else {
            // Fallback to basic event raising behavior
            Logger::info("Cancel action for sendId: {} (no event dispatcher available - no-op)", sendId);
            // Without a dispatcher, we can't cancel anything, but this is not an error
            return true;
        }

    } catch (const std::exception &e) {
        Logger::error("Failed to execute cancel action: {}", e.what());
        return false;
    }
}

std::shared_ptr<IEventRaiser> ActionExecutorImpl::getEventRaiser() {
    return eventRaiser_;
}

std::string ActionExecutorImpl::generateUniqueSendId() const {
    // SCXML 6.2.4: Generate unique sendid for send actions
    // Use session ID + timestamp + atomic counter for uniqueness
    static std::atomic<uint64_t> counter{0};

    auto timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();

    auto currentCounter = counter.fetch_add(1);

    return "send_" + sessionId_ + "_" + std::to_string(timestamp) + "_" + std::to_string(currentCounter);
}

}  // namespace RSM