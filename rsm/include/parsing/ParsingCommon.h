#pragma once

#include "common/Logger.h"
#include <libxml++/libxml++.h>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief 파싱 관련 공통 유틸리티 클래스
 *
 * 이 클래스는 여러 파서 클래스에서 공통으로 사용하는
 * 함수와 상수를 제공합니다. 네임스페이스 처리, 속성 검증,
 * 경로 처리 등을 위한 유틸리티가 포함됩니다.
 */

namespace RSM {

class ParsingCommon {
public:
    /**
     * @brief SCXML 관련 상수
     */
    struct Constants {
        static const std::string SCXML_NAMESPACE;
        static const std::string CODE_NAMESPACE;
        static const std::string CTX_NAMESPACE;
        static const std::string DI_NAMESPACE;
    };

    /**
     * @brief 네임스페이스를 고려하여 노드 이름 비교
     * @param nodeName 검사할 노드 이름
     * @param baseName 기본 이름 (네임스페이스 없음)
     * @return 일치 여부
     */
    static bool matchNodeName(const std::string &nodeName, const std::string &baseName);

    /**
     * @brief 지정된 이름의 자식 요소 찾기 (네임스페이스 고려)
     * @param element 부모 요소
     * @param childName 찾을 자식 이름
     * @return 찾은 자식 요소들
     */
    static std::vector<const xmlpp::Element *> findChildElements(const xmlpp::Element *element,
                                                                 const std::string &childName);

    /**
     * @brief 지정된 이름의 첫 번째 자식 요소 찾기 (네임스페이스 고려)
     * @param element 부모 요소
     * @param childName 찾을 자식 이름
     * @return 찾은 자식 요소, 없으면 nullptr
     */
    static const xmlpp::Element *findFirstChildElement(const xmlpp::Element *element, const std::string &childName);

    /**
     * @brief 요소 또는 상위 요소에서 ID 속성 찾기
     * @param element 검사할 요소
     * @return 찾은 ID, 없으면 빈 문자열
     */
    static std::string findElementId(const xmlpp::Element *element);

    /**
     * @brief 속성 값 가져오기 (여러 이름 시도)
     * @param element 속성을 가진 요소
     * @param attrNames 시도할 속성 이름 목록
     * @return 속성 값, 없으면 빈 문자열
     */
    static std::string getAttributeValue(const xmlpp::Element *element, const std::vector<std::string> &attrNames);

    /**
     * @brief 모든 속성을 맵으로 수집
     * @param element 속성을 가진 요소
     * @param excludeAttrs 제외할 속성 이름 목록
     * @return 속성 맵 (이름 -> 값)
     */
    static std::unordered_map<std::string, std::string>
    collectAttributes(const xmlpp::Element *element, const std::vector<std::string> &excludeAttrs = {});

    /**
     * @brief 상대 경로를 절대 경로로 변환
     * @param basePath 기준 경로
     * @param relativePath 상대 경로
     * @return 절대 경로
     */
    static std::string resolveRelativePath(const std::string &basePath, const std::string &relativePath);

    /**
     * @brief 텍스트 노드의 내용을 추출
     * @param element 텍스트를 포함하는 요소
     * @param trimWhitespace 공백 제거 여부
     * @return 추출된 텍스트
     */
    static std::string extractTextContent(const xmlpp::Element *element, bool trimWhitespace = true);

    /**
     * @brief XML 요소 이름 추출 (네임스페이스 제거)
     * @param element XML 요소
     * @return 네임스페이스 없는 요소 이름
     */
    static std::string getLocalName(const xmlpp::Element *element);

    static std::vector<const xmlpp::Element *> findChildElementsWithNamespace(const xmlpp::Element *parent,
                                                                              const std::string &elementName,
                                                                              const std::string &namespaceURI);

    static std::string trimString(const std::string &str);

private:
    // 인스턴스 생성 방지
    ParsingCommon() = delete;
};

}  // namespace RSM