#pragma once

#include "runtime/DataModelEngine.h"
#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/IECMAScriptEngine.h"
#include "runtime/RuntimeContext.h"
#include <memory>

namespace SCXML {

namespace Model {
class IDataModelItem;
}

// Type aliases for compatibility
using ECMAValue = IECMAScriptEngine::ECMAValue;
using ECMAResult = IECMAScriptEngine::ECMAResult;

/**
 * @brief ECMAScript DataModel을 위한 JavaScript 엔진 통합
 *
 * 기존 DataModelEngine을 확장하여 JavaScript 엔진을 통합합니다.
 * 엔진은 런타임에 교체 가능하도록 설계되었습니다.
 */
class ECMAScriptDataModelEngine : public DataModelEngine {
public:
    /**
     * @brief JavaScript 엔진 타입 열거형 - dynamic_cast 대신 사용
     */
    enum class EngineType {
        UNKNOWN,

        V8,
        CHAKRA
    };

    // dynamic_cast 없이 타입별 동작을 지원하는 가상 함수들
    virtual bool supportsDataModelSync() const {
        return false;
    }

    virtual void syncDataModelVariables(const ::SCXML::Runtime::RuntimeContext & /* context */) {}

    virtual bool supportsContextHealthCheck() const {
        return false;
    }

    virtual bool testContextHealth() {
        return true;
    }

    virtual EngineType getEngineType() const {
        return EngineType::UNKNOWN;
    }

public:
    /**
     * @brief 생성자
     * @param engineType JavaScript 엔진 타입 (기본값: AUTO)
     */
    explicit ECMAScriptDataModelEngine(
        ECMAScriptEngineFactory::EngineType engineType = ECMAScriptEngineFactory::EngineType::AUTO);

    virtual ~ECMAScriptDataModelEngine();

    // DataModelEngine 확장 메소드들
    // ECMAScript는 자체적으로 context를 관리하므로 DataModelEngine의 방식을 따름
    DataModelResult initializeFromDataItems(const std::vector<std::shared_ptr<SCXML::Model::IDataModelItem>> &dataItems,
                                            const std::string &sessionId = "");

    DataModelResult setValue(const std::string &location, const DataValue &value, Scope scope = Scope::GLOBAL);

    DataModelResult getValue(const std::string &location, std::optional<Scope> scope = std::nullopt) const;

    DataModelResult evaluateExpression(const std::string &expression, Scope scope = Scope::GLOBAL) const;

    // ECMAScript specific method - not overriding base class
    DataModelResult executeScript(const std::string &script, Scope scope = Scope::GLOBAL);

    // ECMAScript 특화 기능
    /**
     * @brief JavaScript 엔진 교체
     * @param engineType 새로운 엔진 타입
     * @return 성공 여부
     */
    bool switchEngine(ECMAScriptEngineFactory::EngineType engineType);

    /**
     * @brief 현재 JavaScript 엔진 정보
     */
    std::string getCurrentEngineName() const;
    std::string getCurrentEngineVersion() const;

    /**
     * @brief JavaScript 함수 정의
     * @param name 함수명
     * @param params 매개변수 목록
     * @param body 함수 본문
     */
    DataModelResult defineFunction(const std::string &name, const std::vector<std::string> &params,
                                   const std::string &body);

    /**
     * @brief JavaScript 함수 호출
     * @param name 함수명
     * @param args 인수 목록
     */
    DataModelResult callFunction(const std::string &name, const std::vector<DataValue> &args);

    /**
     * @brief SCXML 시스템 변수 설정
     * @param sessionId 세션 ID
     * @param sessionName 세션 이름
     * @param ioProcessors I/O 프로세서 목록
     */
    void setupSCXMLSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                   const std::vector<std::string> &ioProcessors);

    /**
     * @brief 현재 이벤트 설정 (_event 변수)
     * @param event 현재 처리 중인 이벤트
     */
    void setCurrentEvent(const std::shared_ptr<::SCXML::Events::Event> &event);

    /**
     * @brief In() 함수용 상태 체크 콜백 설정
     */
    using StateCheckFunction = std::function<bool(const std::string &)>;
    void setStateCheckFunction(const StateCheckFunction &func);

    /**
     * @brief 메모리 사용량 및 성능 통계
     */
    struct EngineStats {
        size_t memoryUsage = 0;
        std::string engineName;
        std::string engineVersion;
        uint64_t expressionCount = 0;
        uint64_t executionTimeMs = 0;
    };

    EngineStats getEngineStats() const;

    /**
     * @brief 가비지 컬렉션 실행
     */
    void collectGarbage();

private:
    std::unique_ptr<IECMAScriptEngine> ecmaEngine_;
    ECMAScriptEngineFactory::EngineType currentEngineType_;
    StateCheckFunction stateCheckFunction_;

    // 통계
    mutable uint64_t expressionCount_ = 0;
    mutable uint64_t totalExecutionTimeMs_ = 0;

    // DataValue를 ECMAValue로 변환
    ECMAValue dataValueToECMAValue(const DataValue &value) const;

    // ECMAValue를 DataValue로 변환
    DataValue ecmaValueToDataValue(const ECMAValue &value) const;

    // SCXML 내장 함수들 등록
    void registerSCXMLBuiltins();

    // ECMAScript 내장 객체들 등록
    void registerECMAScriptObjects();

    // 시스템 변수들 설정
    void initializeSystemVariables();

    // 에러 변환
    DataModelResult ecmaResultToDataModelResult(const ECMAResult &result) const;
};

}  // namespace SCXML
