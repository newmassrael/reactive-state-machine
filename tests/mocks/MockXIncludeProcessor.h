#pragma once

#include "model/IXIncludeProcessor.h"
#include <gmock/gmock.h>
#include <libxml++/libxml++.h>
#include <string>
#include <vector>

/**
 * @brief XIncludeProcessor의 모의 (Mock) 구현
 *
 * 테스트에서 사용하기 위한 IXIncludeProcessor의 목 객체입니다.
 * XInclude 처리 호출을 추적하고 시뮬레이션합니다.
 */
class MockXIncludeProcessor : public IXIncludeProcessor
{
public:
    /**
     * @brief 생성자
     */
    MockXIncludeProcessor() {}

    /**
     * @brief 소멸자
     */
    ~MockXIncludeProcessor() override {}

    /**
     * @brief XML 문서의 XInclude 태그 처리를 위한 모의 메서드
     * @param document 처리할 XML 문서
     * @return 성공 여부
     */
    MOCK_METHOD(bool, process, (xmlpp::Document * document), (override));

    /**
     * @brief 기본 검색 경로 설정을 위한 모의 메서드
     * @param basePath 기본 검색 경로
     */
    MOCK_METHOD(void, setBasePath, (const std::string &basePath), (override));

    /**
     * @brief 오류 메시지 반환을 위한 모의 메서드
     * @return 오류 메시지 목록
     */
    MOCK_METHOD(const std::vector<std::string> &, getErrorMessages, (), (const, override));
};
