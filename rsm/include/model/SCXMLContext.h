// SCXMLContext.h
#pragma once

#include <string>
#include <unordered_map>

/**
 * @brief SCXML 파싱 컨텍스트 정보를 담는 클래스
 *
 * 이 클래스는 SCXML 파싱 과정에서 필요한 문맥 정보를 포함합니다.
 * 예를 들어 전역 데이터 모델 타입, 네임스페이스 정보 등이 포함됩니다.
 */

namespace RSM {

class SCXMLContext {
public:
    /**
     * @brief 기본 생성자
     */
    SCXMLContext() = default;

    /**
     * @brief 데이터 모델 타입 설정
     * @param datamodelType 데이터 모델 타입 (예: "ecmascript", "xpath", "null")
     */
    void setDatamodelType(const std::string &datamodelType);

    /**
     * @brief 데이터 모델 타입 반환
     * @return 데이터 모델 타입
     */
    const std::string &getDatamodelType() const;

    /**
     * @brief 바인딩 모드 설정
     * @param binding 바인딩 모드 (예: "early", "late")
     */
    void setBinding(const std::string &binding);

    /**
     * @brief 바인딩 모드 반환
     * @return 바인딩 모드
     */
    const std::string &getBinding() const;

    /**
     * @brief 네임스페이스 추가
     * @param prefix 네임스페이스 접두사
     * @param uri 네임스페이스 URI
     */
    void addNamespace(const std::string &prefix, const std::string &uri);

    /**
     * @brief 네임스페이스 URI 조회
     * @param prefix 네임스페이스 접두사
     * @return 네임스페이스 URI (없을 경우 빈 문자열)
     */
    const std::string &getNamespaceURI(const std::string &prefix) const;

    /**
     * @brief 추가 속성 설정
     * @param name 속성 이름
     * @param value 속성 값
     */
    void setAttribute(const std::string &name, const std::string &value);

    /**
     * @brief 속성 값 반환
     * @param name 속성 이름
     * @return 속성 값 (없을 경우 빈 문자열)
     */
    const std::string &getAttribute(const std::string &name) const;

private:
    std::string datamodelType_;                                ///< 데이터 모델 타입
    std::string binding_;                                      ///< 바인딩 모드
    std::unordered_map<std::string, std::string> namespaces_;  ///< 네임스페이스 매핑
    std::unordered_map<std::string, std::string> attributes_;  ///< 추가 속성들
    std::string emptyString_;                                  ///< 빈 문자열 반환용
};

}  // namespace RSM