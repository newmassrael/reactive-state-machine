#pragma once

#ifndef GUARDEVALUATOR_H
#define GUARDEVALUATOR_H

#include "runtime/ExpressionEvaluator.h"
#include <memory>
#include <string>
#include <vector>

// Forward declarations
namespace SCXML {

namespace Model {
class ITransitionNode;
class IGuardNode;
}
// Forward declarations
namespace Core {
class DocumentModel;
}

namespace Core {
// Forward declaration moved to Model namespace
// Forward declaration moved to Model namespace
}  // namespace Core

namespace Runtime {
class RuntimeContext;
}

class DataModelEngine;

namespace Events {
using EventPtr = std::shared_ptr<class Event>;
}

namespace Core {
class Event;
}

/**
 * @brief Guard Condition Evaluator for SCXML
 *
 * This class evaluates guard conditions on SCXML transitions according to
 * the W3C specification. It integrates with the expression evaluator to
 * provide boolean evaluation of guard expressions.
 */
class GuardEvaluator {
public:
    /**
     * @brief Guard evaluation result
     */
    struct GuardResult {
        bool satisfied;               // Whether guard condition is satisfied
        bool hasGuard;                // Whether transition has a guard condition
        std::string guardExpression;  // The guard expression that was evaluated
        std::string errorMessage;     // Error message if evaluation failed

        GuardResult() : satisfied(true), hasGuard(false) {}
    };

    /**
     * @brief Guard evaluation context
     */
    struct GuardContext {
        SCXML::Events::EventPtr currentEvent;            // Current triggering event
        std::string sourceState;                         // Source state of transition
        std::string targetState;                         // Target state of transition
        SCXML::Runtime::RuntimeContext *runtimeContext;  // Runtime context

        GuardContext() : runtimeContext(nullptr) {}
    };

    /**
     * @brief Construct a new Guard Evaluator
     */
    GuardEvaluator();

    /**
     * @brief Destructor
     */
    ~GuardEvaluator() = default;

    /**
     * @brief Evaluate guard condition for a transition
     * @param transition Transition to evaluate guard for
     * @param context Guard evaluation context
     * @return Guard evaluation result
     */
    GuardResult evaluateTransitionGuard(std::shared_ptr<SCXML::Model::ITransitionNode> transition,
                                        const GuardContext &context);

    /**
     * @brief Evaluate guard condition for a guard node
     * @param guard Guard node to evaluate
     * @param context Guard evaluation context
     * @return Guard evaluation result
     */
    GuardResult evaluateGuardNode(std::shared_ptr<SCXML::Model::IGuardNode> guard, const GuardContext &context);

    /**
     * @brief Evaluate guard expression directly
     * @param expression Guard expression string
     * @param context Guard evaluation context
     * @return Guard evaluation result
     */
    GuardResult evaluateExpression(const std::string &expression, const GuardContext &context);

    /**
     * @brief Check if transition has a guard condition
     * @param transition Transition to check
     * @return true if transition has guard condition
     */
    bool hasGuardCondition(std::shared_ptr<SCXML::Model::ITransitionNode> transition) const;

    /**
     * @brief Get guard expression from transition
     * @param transition Transition to get guard from
     * @return Guard expression string, empty if no guard
     */
    std::string getGuardExpression(std::shared_ptr<SCXML::Model::ITransitionNode> transition) const;

    /**
     * @brief Set expression evaluator to use
     * @param evaluator Expression evaluator instance
     */
    void setExpressionEvaluator(std::shared_ptr<SCXML::Runtime::ExpressionEvaluator> evaluator);

    /**
     * @brief Set DataModel engine for ECMAScript evaluation (architectural fix)
     * @param dataEngine DataModel engine instance
     */
    void setDataModelEngine(DataModelEngine* dataEngine);

protected:
    /**
     * @brief Create evaluation context for expressions
     * @param guardCtx Guard context
     * @return Expression evaluation context
     */
    SCXML::Runtime::ExpressionEvaluator::EventContext createEvaluationContext(const GuardContext &guardCtx);

    /**
     * @brief Validate guard expression syntax
     * @param expression Expression to validate
     * @return true if expression is valid
     */
    bool isValidGuardExpression(const std::string &expression) const;

    /**
     * @brief Get all guard expressions from transition
     * @param transition Transition to analyze
     * @return Vector of guard expressions
     */
    std::vector<std::string> getAllGuardExpressions(std::shared_ptr<SCXML::Model::ITransitionNode> transition) const;

    /**
     * @brief Evaluate multiple guard expressions (AND logic)
     * @param expressions Guard expressions to evaluate
     * @param context Guard evaluation context
     * @return Combined evaluation result
     */
    GuardResult evaluateMultipleExpressions(const std::vector<std::string> &expressions, const GuardContext &context);

    /**
     * @brief Handle guard evaluation error
     * @param expression Expression that caused error
     * @param error Error message
     * @param context Guard context
     * @return Error result
     */
    GuardResult handleError(const std::string &expression, const std::string &error, const GuardContext &context);

    /**
     * @brief Evaluate complex guard expressions with logical operators
     * @param expression Guard expression with AND/OR/NOT operators
     * @param context Guard evaluation context
     * @return Guard evaluation result
     */
    GuardResult evaluateComplexExpression(const std::string &expression, const GuardContext &context);

    /**
     * @brief Evaluate event-based guards
     * @param expression Event-based guard expression
     * @param context Guard evaluation context
     * @return Guard evaluation result
     */
    GuardResult evaluateEventGuard(const std::string &expression, const GuardContext &context);

    /**
     * @brief Evaluate state-based guards (In() function)
     * @param expression State-based guard expression
     * @param context Guard evaluation context
     * @return Guard evaluation result
     */
    GuardResult evaluateStateGuard(const std::string &expression, const GuardContext &context);

    /**
     * @brief Evaluate data model guards
     * @param expression Data model guard expression
     * @param context Guard evaluation context
     * @return Guard evaluation result
     */
    GuardResult evaluateDataModelGuard(const std::string &expression, const GuardContext &context);

private:
    // Expression evaluator for guard conditions
    std::shared_ptr<SCXML::Runtime::ExpressionEvaluator> expressionEvaluator_;

    // DataModel engine for ECMAScript evaluation (architectural enhancement)
    DataModelEngine* dataModelEngine_;

    // Error handling
    std::string lastError_;

    /**
     * @brief Initialize default expression evaluator
     */
    void initializeDefaultEvaluator();

    /**
     * @brief Set error message
     * @param message Error message
     */
    void setError(const std::string &message);
};

}  // namespace SCXML

#endif  // GUARDEVALUATOR_H