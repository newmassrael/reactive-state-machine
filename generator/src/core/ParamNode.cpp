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
    // Validate that we have a name
    if (name_.empty()) {
        SCXML::Common::Logger::error("ParamNode: Parameter must have a name");
        return false;
    }

    // W3C SCXML 사양: 매개변수 초기화 시 초기값을 데이터 모델에 설정
    if (!value_.empty() && !hasExpression_ && !hasLocation_) {
        // 리터럴 값이 있는 경우 데이터 모델에 설정
        auto result = context.setDataValue(name_, value_);
        if (result.isSuccess()) {
            SCXML::Common::Logger::debug("ParamNode: Initialized parameter '" + name_ + "' with literal value: " + value_);
        } else {
            SCXML::Common::Logger::error("ParamNode: Failed to initialize parameter '" + name_ + "': " + result.getErrors()[0].message);
            return false;
        }
    }

    SCXML::Common::Logger::debug("ParamNode: Initialized parameter: " + name_ + (id_.empty() ? "" : " with id: " + id_));
    return true;
}

std::string ParamNode::getValue(SCXML::Model::IExecutionContext &context) const {
    try {
        // W3C SCXML 사양: Priority: location > expression > literal value
        if (hasLocation_ && !location_.empty()) {
            SCXML::Common::Logger::debug("ParamNode: Evaluating location reference: " + location_);
            
            // 실제 데이터 모델에서 값 가져오기
            auto result = context.getDataValue(location_);
            if (result.isSuccess()) {
                SCXML::Common::Logger::debug("ParamNode: Retrieved value from location '" + location_ + "': " + result.getValue());
                return result.getValue();
            } else {
                std::string errorMsg = "ParamNode: Failed to get value from location '" + location_ + "': " + result.getErrors()[0].message;
                SCXML::Common::Logger::error(errorMsg);
                
                // W3C SCXML 사양: 평가 실패 시 error.execution 이벤트 생성
                context.raiseEvent("error.execution", errorMsg);
                return "";  // W3C: 실패 시 빈 문자열 반환
            }
        } else if (hasExpression_ && !expression_.empty()) {
            SCXML::Common::Logger::debug("ParamNode: Evaluating expression: " + expression_);
            
            // 실제 표현식 평가
            auto result = context.evaluateExpression(expression_);
            if (result.isSuccess()) {
                SCXML::Common::Logger::debug("ParamNode: Expression '" + expression_ + "' evaluated to: " + result.getValue());
                return result.getValue();
            } else {
                std::string errorMsg = "ParamNode: Failed to evaluate expression '" + expression_ + "': " + result.getErrors()[0].message;
                SCXML::Common::Logger::error(errorMsg);
                
                // W3C SCXML 사양: 평가 실패 시 error.execution 이벤트 생성
                context.raiseEvent("error.execution", errorMsg);
                return "";  // W3C: 실패 시 빈 문자열 반환
            }
        } else {
            // 리터럴 값 반환
            SCXML::Common::Logger::debug("ParamNode: Returning literal value: " + value_);
            return value_;
        }
    } catch (const std::exception &e) {
        std::string error = "ParamNode: Exception in getValue: " + std::string(e.what());
        SCXML::Common::Logger::error(error);
        
        // W3C SCXML 사양: 예외 발생 시에도 error.execution 이벤트 생성
        context.raiseEvent("error.execution", error);
        return "";  // W3C: 예외 발생 시 빈 문자열 반환
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
    // Don't clear hasLocation_ - let priority rules handle this in getValue()
    SCXML::Common::Logger::debug("ParamNode: Set expression for parameter: " + name_ + ", expr: " + expr);
}

void ParamNode::setLocation(const std::string &location) {
    location_ = location;
    hasLocation_ = true;
    // Don't clear hasExpression_ - let priority rules handle this in getValue()
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

        // W3C SCXML 사양: 매개변수 값을 데이터 모델에 설정
        SCXML::Common::Logger::debug("ParamNode: Processing parameter " + name_ + " = " + paramValue);
        
        // 실제 데이터 모델에 매개변수 설정
        auto result = context.setDataValue(name_, paramValue);
        if (!result.isSuccess()) {
            SCXML::Common::Logger::error("ParamNode: Failed to set parameter '" + name_ + "': " + result.getErrors()[0].message);
            return result;
        }

        // 이벤트 관련 처리 로그
        if (!eventName.empty()) {
            SCXML::Common::Logger::debug("ParamNode: Set parameter '" + name_ + "' for event: " + eventName);
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

    // SCXML W3C 사양: NMTOKEN 검증
    if (!name_.empty()) {
        if (!isValidNMTOKEN(name_)) {
            errors.push_back("ParamNode: Parameter name '" + name_ + "' is not a valid NMTOKEN");
        }
    }

    // Must have some form of value specification
    bool hasValue = !value_.empty();
    bool hasExpr = hasExpression_ && !expression_.empty();
    bool hasLoc = hasLocation_ && !location_.empty();

    if (!hasValue && !hasExpr && !hasLoc) {
        errors.push_back("ParamNode: Parameter '" + name_ + "' must have a value, expression, or location");
    }

    // SCXML W3C 사양: expr과 location 상호 배타성 검증
    if (hasExpr && hasLoc) {
        errors.push_back("ParamNode: Parameter '" + name_ + "' has both expr and location attributes - they are mutually exclusive per SCXML W3C specification");
    }

    // Check for conflicting value specifications (기존 로직 유지 - 우선순위 경고)
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

    if (valueSourceCount > 1 && !(hasExpr && hasLoc)) { // 상호 배타성은 이미 위에서 처리
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
    
    // W3C SCXML 사양: 모든 속성을 복사해야 함 - 직접 내부 상태 복사
    cloned->value_ = value_;
    cloned->expression_ = expression_;
    cloned->location_ = location_;
    cloned->hasExpression_ = hasExpression_;
    cloned->hasLocation_ = hasLocation_;
    
    SCXML::Common::Logger::debug("ParamNode: Cloned parameter '" + name_ + "' with all attributes");
    return cloned;
}

bool ParamNode::isValidNMTOKEN(const std::string &token) {
    if (token.empty()) {
        return false;
    }
    
    // NMTOKEN은 문자, 숫자, '.', '-', '_', ':'로 구성되며
    // 첫 문자는 문자, '_', ':'만 가능 (숫자나 '-', '.'로 시작 불가)
    char firstChar = token[0];
    if (!std::isalpha(firstChar) && firstChar != '_' && firstChar != ':') {
        return false;
    }
    
    // 나머지 문자들은 문자, 숫자, '.', '-', '_', ':'만 가능
    for (size_t i = 1; i < token.length(); ++i) {
        char c = token[i];
        if (!std::isalnum(c) && c != '.' && c != '-' && c != '_' && c != ':') {
            return false;
        }
    }
    
    return true;
}

}  // namespace Core
}  // namespace SCXML
