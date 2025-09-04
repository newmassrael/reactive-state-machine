#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace SCXML {

/**
 * @brief JavaScript value types - 모든 엔진에서 공통으로 사용
 */
/**
 * @brief JavaScript value types - 비재귀 구조 (임시)
 * 나중에 재귀적 구조로 개선 예정
 */
// Forward declarations for recursive types
struct ScxmlObject;
struct ScxmlArray;

using ScxmlValue = std::variant<std::monostate,               // null/undefined
                                bool,                         // boolean
                                double,                       // number
                                std::string,                  // string
                                std::shared_ptr<ScxmlArray>,  // array (shared_ptr for recursion)
                                std::shared_ptr<ScxmlObject>  // object (shared_ptr for recursion)
                                >;

struct ScxmlArray {
    std::vector<ScxmlValue> values;
};

struct ScxmlObject {
    std::unordered_map<std::string, ScxmlValue> properties;
};

/**
 * @brief JavaScript 실행 결과
 */
struct JSResult {
    bool success = false;
    ScxmlValue value;
    std::string error;

    static JSResult createSuccess(const ScxmlValue &val = std::monostate{}) {
        JSResult result;
        result.success = true;
        result.value = val;
        return result;
    }

    static JSResult createError(const std::string &err) {
        JSResult result;
        result.success = false;
        result.error = err;
        return result;
    }
};

/**
 * @brief JavaScript 엔진 추상화 인터페이스
 *
 * 이 인터페이스를 통해 V8, 또는 다른 JavaScript 엔진을
 * 쉽게 교체할 수 있습니다.
 */
class IJavaScriptEngine {
public:
    virtual ~IJavaScriptEngine() = default;

    /**
     * @brief 엔진 초기화
     */
    virtual bool initialize() = 0;

    /**
     * @brief 엔진 정리
     */
    virtual void shutdown() = 0;

    /**
     * @brief JavaScript 코드 실행
     * @param code 실행할 JavaScript 코드
     * @return 실행 결과
     */
    virtual JSResult execute(const std::string &code) = 0;

    /**
     * @brief JavaScript 표현식 평가
     * @param expression 평가할 표현식
     * @return 평가 결과
     */
    virtual JSResult evaluate(const std::string &expression) = 0;

    /**
     * @brief 변수 설정
     * @param name 변수명
     * @param value 값
     */
    virtual bool setVariable(const std::string &name, const ScxmlValue &value) = 0;

    /**
     * @brief 변수 가져오기
     * @param name 변수명
     * @return 변수 값
     */
    virtual JSResult getVariable(const std::string &name) = 0;

    /**
     * @brief 함수 정의
     * @param name 함수명
     * @param params 파라미터 목록
     * @param body 함수 본문
     */
    virtual bool defineFunction(const std::string &name, const std::vector<std::string> &params,
                                const std::string &body) = 0;

    /**
     * @brief 함수 호출
     * @param name 함수명
     * @param args 인수 목록
     * @return 함수 실행 결과
     */
    virtual JSResult callFunction(const std::string &name, const std::vector<ScxmlValue> &args) = 0;

    /**
     * @brief 네이티브 함수 등록
     * @param name 함수명
     * @param func 네이티브 함수
     */
    using NativeFunction = std::function<JSResult(const std::vector<ScxmlValue> &)>;
    virtual bool registerNativeFunction(const std::string &name, const NativeFunction &func) = 0;

    /**
     * @brief 전역 객체 설정
     * @param name 객체명
     * @param object 객체 값
     */
    virtual bool setGlobalObject(const std::string &name,
                                 const std::unordered_map<std::string, ScxmlValue> &object) = 0;

    /**
     * @brief 엔진 정보 반환
     */
    virtual std::string getEngineName() const = 0;
    virtual std::string getEngineVersion() const = 0;

    /**
     * @brief 메모리 사용량 정보
     */
    virtual size_t getMemoryUsage() const = 0;

    /**
     * @brief 가비지 컬렉션 실행
     */
    virtual void collectGarbage() = 0;

    /**
     * @brief 실행 시간 제한 설정 (밀리초)
     */
    virtual void setExecutionTimeout(uint32_t timeoutMs) = 0;
};

/**
 * @brief JavaScript 엔진 팩토리
 */
class JavaScriptEngineFactory {
public:
    enum class EngineType {

        V8,   // V8 엔진 (미래 확장용)
        AUTO  // 자동 선택
    };

    /**
     * @brief JavaScript 엔진 생성
     * @param type 엔진 타입
     * @return 생성된 엔진 인스턴스
     */
    static std::unique_ptr<IJavaScriptEngine> create(EngineType type = EngineType::AUTO);

    /**
     * @brief 사용 가능한 엔진 목록
     */
    static std::vector<EngineType> getAvailableEngines();

    /**
     * @brief 엔진 타입을 문자열로 변환
     */
    static std::string engineTypeToString(EngineType type);

    /**
     * @brief 문자열을 엔진 타입으로 변환
     */
    static EngineType stringToEngineType(const std::string &str);
};

}  // namespace SCXML