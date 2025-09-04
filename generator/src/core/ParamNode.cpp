#include "core/ParamNode.h"
#include "common/Logger.h"
#include "core/ParamNode.h"
#include "runtime/DataModelEngine.h"
#include "runtime/ExpressionEvaluator.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {
namespace Core {

ParamNode::ParamNode(const std::string &name, const std::string &id)
    : id_(id), name_(name), hasExpression_(false), hasLocation_(false) {}

bool ParamNode::initialize(SCXML::Model::IExecutionContext &context) {
    (void)context;  // Suppress unused parameter warning

    // Validate that we have a name
    if (name_.empty()) {
        SCXML::Common::Logger::error("ParamNode: Parameter must have a name");
        return false;
    }

    SCXML::Common::Logger::debug("ParamNode: Initialized parameter: " + name_ + (id_.empty() ? "" : " with id: " + id_));
    return true;
}

std::string ParamNode::getValue(SCXML::Model::IExecutionContext &context) const {
    (void)context;  // Suppress unused parameter warning

    try {
        // Priority: location > expression > literal value
        if (hasLocation_ && !location_.empty()) {
            // Note: Data model access requires concrete runtime context
            // For now, return location as placeholder
            SCXML::Common::Logger::debug("ParamNode: Location placeholder: " + location_);
            return location_;  // Placeholder - should get value from data model
        } else if (hasExpression_ && !expression_.empty()) {
            // Note: Expression evaluation requires concrete runtime context
            // For now, return expression as placeholder
            SCXML::Common::Logger::debug("ParamNode: Expression placeholder: " + expression_);
            return expression_;  // Placeholder - should evaluate expression
        } else {
            // Return literal value
            SCXML::Common::Logger::debug("ParamNode: Returning literal value: " + value_);
            return value_;
        }
    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ParamNode: Exception in getValue: " + std::string(e.what()));
        return value_;
    }
}

void ParamNode::setName(const std::string &name) {
    name_ = name;
    SCXML::Common::Logger::debug("ParamNode: Set parameter name: " + name);
}

void ParamNode::setValue(const std::string &value) {
    value_ = value;
    hasExpression_ = false;
    hasLocation_ = false;
    SCXML::Common::Logger::debug("ParamNode: Set literal value for parameter: " + name_);
}

void ParamNode::setExpression(const std::string &expr) {
    expression_ = expr;
    hasExpression_ = true;
    hasLocation_ = false;
    SCXML::Common::Logger::debug("ParamNode: Set expression for parameter: " + name_ + ", expr: " + expr);
}

void ParamNode::setLocation(const std::string &location) {
    location_ = location;
    hasLocation_ = true;
    hasExpression_ = false;
    SCXML::Common::Logger::debug("ParamNode: Set location for parameter: " + name_ + ", location: " + location);
}

bool ParamNode::hasExpression() const {
    return hasExpression_ && !expression_.empty();
}

bool ParamNode::hasLocation() const {
    return hasLocation_ && !location_.empty();
}

SCXML::Common::Result<void> ParamNode::processParameter(SCXML::Model::IExecutionContext &context,
                                                        const std::string &eventName) const {
    try {
        if (name_.empty()) {
            return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo(SCXML::Common::ErrorSeverity::ERROR,
                                                                        "PARAM_NO_NAME", "Parameter must have a name"));
        }

        // Get the parameter value
        std::string paramValue = getValue(context);

        // Note: Data model operations require concrete runtime context
        // For now, just log the parameter processing
        (void)context;  // Suppress unused parameter warning
        SCXML::Common::Logger::debug("ParamNode: Processing parameter " + name_ + " = " + paramValue);

        // For placeholder implementation, just return success
        if (!eventName.empty()) {
            SCXML::Common::Logger::debug("ParamNode: Would set event parameter for: " + eventName);
        }

        return SCXML::Common::Result<void>();

    } catch (const std::exception &e) {
        std::string error = "ParamNode: Exception processing parameter: " + name_ + ", error: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        return SCXML::Common::Result<void>(
            SCXML::Common::ErrorInfo(SCXML::Common::ErrorSeverity::ERROR, "PARAM_EXCEPTION", error));
    }
}

std::vector<std::string> ParamNode::validate() const {
    std::vector<std::string> errors;

    // Name is required
    if (name_.empty()) {
        errors.push_back("ParamNode: Parameter must have a name");
    }

    // Must have some form of value specification
    bool hasValue = !value_.empty();
    bool hasExpr = hasExpression_ && !expression_.empty();
    bool hasLoc = hasLocation_ && !location_.empty();

    if (!hasValue && !hasExpr && !hasLoc) {
        errors.push_back("ParamNode: Parameter '" + name_ + "' must have a value, expression, or location");
    }

    // Check for conflicting value specifications
    int valueSourceCount = 0;
    if (hasValue) {
        valueSourceCount++;
    }
    if (hasExpr) {
        valueSourceCount++;
    }
    if (hasLoc) {
        valueSourceCount++;
    }

    if (valueSourceCount > 1) {
        errors.push_back("ParamNode: Parameter '" + name_ +
                         "' has multiple value sources, location will take precedence");
    }

    return errors;
}

const std::string &ParamNode::getName() const {
    return name_;
}

const std::string &ParamNode::getId() const {
    return id_;
}

const std::string &ParamNode::getLiteralValue() const {
    return value_;
}

const std::string &ParamNode::getExpression() const {
    return expression_;
}

const std::string &ParamNode::getLocation() const {
    return location_;
}

std::string ParamNode::getValueSource() const {
    if (hasLocation_ && !location_.empty()) {
        return "location";
    }
    if (hasExpression_ && !expression_.empty()) {
        return "expression";
    }
    if (!value_.empty()) {
        return "literal";
    }
    return "none";
}

void ParamNode::clear() {
    value_.clear();
    expression_.clear();
    location_.clear();
    hasExpression_ = false;
    hasLocation_ = false;
    SCXML::Common::Logger::debug("ParamNode: Cleared all values for parameter: " + name_);
}

std::shared_ptr<Model::IDataNode> ParamNode::clone() const {
    auto cloned = std::make_shared<ParamNode>(name_, id_);
    if (hasLocation_) {
        cloned->setLocation(location_);
    } else if (hasExpression_) {
        cloned->setExpression(expression_);
    } else {
        cloned->setValue(value_);
    }
    return cloned;
}

}  // namespace Core
}  // namespace SCXML
