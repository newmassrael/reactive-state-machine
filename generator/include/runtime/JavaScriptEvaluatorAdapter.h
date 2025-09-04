#pragma once

#include "runtime/IECMAScriptEngine.h"
#include "runtime/JavaScriptExpressionEvaluator.h"
#include "runtime/RuntimeContext.h"
#include <memory>
#include <optional>

namespace SCXML {

/**
 * @brief JavaScriptExpressionEvaluator를 IECMAScriptEngine 인터페이스로 어댑팅
 *
 * 기존의 JavaScriptExpressionEvaluator를 새로운 통합 인터페이스에 맞게
 * 어댑팅하여 graceful한 엔진 교체를 가능하게 합니다.
 */
class JavaScriptEvaluatorAdapter : public IECMAScriptEngine {
public:
    /**
     * @brief 생성자
     */
    JavaScriptEvaluatorAdapter();

    /**
     * @brief 소멸자
     */
    virtual ~JavaScriptEvaluatorAdapter();

    // IECMAScriptEngine 인터페이스 구현
    bool initialize() override;
    void shutdown() override;

    ECMAResult evaluateExpression(const std::string &expression, ::SCXML::Runtime::RuntimeContext &context) override;

    ECMAResult executeScript(const std::string &script, ::SCXML::Runtime::RuntimeContext &context) override;

    bool setVariable(const std::string &name, const ECMAValue &value) override;
    ECMAResult getVariable(const std::string &name) override;

    // SCXML 특화 기능
    void setCurrentEvent(const std::shared_ptr<::SCXML::Events::Event> &event) override;
    void setStateCheckFunction(const StateCheckFunction &func) override;
    void setupSCXMLSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                   const std::vector<std::string> &ioProcessors) override;

    bool registerNativeFunction(const std::string &name, const NativeFunction &func) override;

    // 엔진 정보
    std::string getEngineName() const override;
    std::string getEngineVersion() const override;
    size_t getMemoryUsage() const override;
    void collectGarbage() override;

private:
    std::unique_ptr<JavaScriptExpressionEvaluator> evaluator_;
    StateCheckFunction stateCheckFunction_;
    std::shared_ptr<::SCXML::Events::Event> currentEvent_;
    std::unordered_map<std::string, ECMAValue> variables_;

    // SCXML 시스템 변수들
    std::string sessionId_;
    std::string sessionName_;
    std::vector<std::string> ioProcessors_;

    // 타입 변환 헬퍼 메서드들
    JavaScriptExpressionEvaluator::JSValue ecmaValueToJSValue(const ECMAValue &value);
    ECMAValue jsValueToECMAValue(const JavaScriptExpressionEvaluator::JSValue &value);

    // 컨텍스트 구성 헬퍼
    JavaScriptExpressionEvaluator::JSEvaluationContext
    createEvaluationContext(::SCXML::Runtime::RuntimeContext &context);

    // 내장 함수 등록
    void registerBuiltinFunctions();

    // 표현식 처리 헬퍼들
    std::string preprocessExpression(const std::string &expression);
    std::optional<ECMAValue> handleDirectExpressions(const std::string &expression);
    std::optional<ECMAValue> handleEventDataAccess(const std::string &expression);
    std::optional<ECMAValue> handleLogicalOrExpression(const std::string &expression);
};

}  // namespace SCXML