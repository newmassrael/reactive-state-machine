#pragma once

#include <libxml++/libxml++.h>
#include <string>
#include <vector>

/**
 * @brief XInclude 처리를 위한 인터페이스
 */

namespace RSM {

class IXIncludeProcessor {
public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~IXIncludeProcessor() = default;

    /**
     * @brief XInclude 처리 실행
     * @param doc libxml++ 문서 객체
     * @return 성공 여부
     */
    virtual bool process(xmlpp::Document *doc) = 0;

    /**
     * @brief 기본 검색 경로 설정
     * @param basePath 기본 검색 경로
     */
    virtual void setBasePath(const std::string &basePath) = 0;

    /**
     * @brief 처리 중 발생한 오류 메시지 반환
     * @return 오류 메시지 목록
     */
    virtual const std::vector<std::string> &getErrorMessages() const = 0;
};

}  // namespace RSM