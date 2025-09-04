#pragma once

#include "events/Event.h"
#include "runtime/DataModelEngine.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace SCXML {

// Forward declarations - RuntimeContext is in global namespace
class DataModelEngine;
class ECMAScriptEngineFactory;

// Forward declarations for Events namespace
namespace Events {
class Event;
}

/**
 * @brief 통합된 ECMAScript 엔진 인터페이스
 *
 * 이 인터페이스를 통해 JavaScriptExpressionEvaluator, V8 등
 * 다양한 JavaScript 엔진을 graceful하게 교체할 수 있습니다.
 */
class IECMAScriptEngine {
public:
    /**
     * @brief ECMAScript 값 타입 (모든 엔진에서 공통 사용)
     */
    using ECMAValue = DataModelEngine::DataValue;

    /**
     * @brief ECMAScript 실행 결과
     */
    struct ECMAResult {
        bool success = false;
        ECMAValue value = std::monostate{};
        std::string errorMessage;

        static ECMAResult createSuccess(const ECMAValue &val = std::monostate{}) {
            ECMAResult result;
            result.success = true;
            result.value = val;
            return result;
        }

        static ECMAResult createError(const std::string &error) {
            ECMAResult result;
            result.success = false;
            result.errorMessage = error;
            return result;
        }
    };

    /**
     * @brief 상태 체크 함수 타입 (In() 함수용)
     */
    using StateCheckFunction = std::function<bool(const std::string &)>;

    virtual ~IECMAScriptEngine() = default;

    // === 핵심 인터페이스 ===

    /**
     * @brief 엔진 초기화
     */
    virtual bool initialize() = 0;

    /**
     * @brief 엔진 정리
     */
    virtual void shutdown() = 0;

    /**
     * @brief ECMAScript 표현식 평가
     * @param expression 평가할 표현식 (예: "_event.data.temp || 25.5")
     * @param context 런타임 컨텍스트 (현재 이벤트, 상태 등 포함)
     * @return 평가 결과
     */
    virtual ECMAResult evaluateExpression(const std::string &expression, ::SCXML::Runtime::RuntimeContext &context) = 0;

    /**
     * @brief ECMAScript 코드 실행
     * @param script 실행할 스크립트 코드
     * @param context 런타임 컨텍스트
     * @return 실행 결과
     */
    virtual ECMAResult executeScript(const std::string &script, ::SCXML::Runtime::RuntimeContext &context) = 0;

    /**
     * @brief 변수 값 설정
     * @param name 변수명
     * @param value 설정할 값
     * @return 설정 성공 여부
     */
    virtual bool setVariable(const std::string &name, const ECMAValue &value) = 0;

    /**
     * @brief 변수 값 조회
     * @param name 변수명
     * @return 조회 결과
     */
    virtual ECMAResult getVariable(const std::string &name) = 0;

    // === SCXML 특화 기능 ===

    /**
     * @brief 현재 이벤트 설정 (_event 변수)
     * @param event 현재 처리 중인 이벤트
     */
    virtual void setCurrentEvent(const std::shared_ptr<::SCXML::Events::Event> &event) = 0;

    /**
     * @brief 상태 체크 함수 설정 (In() 함수용)
     * @param func 상태 체크 콜백
     */
    virtual void setStateCheckFunction(const StateCheckFunction &func) = 0;

    /**
     * @brief SCXML 시스템 변수 설정
     * @param sessionId 세션 ID
     * @param sessionName 세션 이름
     * @param ioProcessors I/O 프로세서 목록
     */
    virtual void setupSCXMLSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                           const std::vector<std::string> &ioProcessors) = 0;

    /**
     * @brief 네이티브 함수 등록
     * @param name 함수명
     * @param func 네이티브 함수
     */
    using NativeFunction = std::function<ECMAValue(const std::vector<ECMAValue> &)>;
    virtual bool registerNativeFunction(const std::string &name, const NativeFunction &func) = 0;

    // === 엔진 정보 ===

    /**
     * @brief 엔진 이름 반환
     */
    virtual std::string getEngineName() const = 0;

    /**
     * @brief 엔진 버전 반환
     */
    virtual std::string getEngineVersion() const = 0;

    /**
     * @brief 메모리 사용량 반환 (바이트)
     */
    virtual size_t getMemoryUsage() const = 0;

    /**
     * @brief 가비지 컬렉션 실행
     */
    virtual void collectGarbage() = 0;

    // dynamic_cast 제거를 위한 가상 함수들
    virtual bool supportsDataModelSync() const {
        return false;
    }

    virtual bool supportsContextHealthCheck() const {
        return false;
    }

    virtual bool testContextHealth() {
        return true;
    }  // 기본 구현은 항상 성공

    virtual void syncDataModelVariables(const ::SCXML::Runtime::RuntimeContext & /* context */) {
    }  // 기본 구현은 아무것도 안 함

    virtual std::string getEngineTypeName() const {
        return "UNKNOWN";
    }
};

// ECMAScriptEngineFactory is defined in ECMAScriptEngineFactory.h

}  // namespace SCXML
