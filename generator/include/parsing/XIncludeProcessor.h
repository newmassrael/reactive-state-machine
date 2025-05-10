#pragma once

#include "IXIncludeProcessor.h"
#include <libxml++/libxml++.h>
#include <string>
#include <vector>
#include <unordered_map>

/**
 * @brief XInclude 처리를 담당하는 클래스
 *
 * 이 클래스는 SCXML 문서의 XInclude 지시문을 처리하는 기능을 제공합니다.
 * 외부 파일 로드 및 통합을 처리하며, 상대 경로와 절대 경로를 모두 지원합니다.
 */
class XIncludeProcessor : public IXIncludeProcessor
{
public:
    /**
     * @brief 생성자
     */
    XIncludeProcessor();

    /**
     * @brief 소멸자
     */
    ~XIncludeProcessor() override;

    /**
     * @brief XInclude 처리 실행
     * @param doc libxml++ 문서 객체
     * @return 성공 여부
     */
    bool process(xmlpp::Document *doc) override;

    /**
     * @brief 기본 검색 경로 설정
     * @param basePath 기본 검색 경로
     */
    void setBasePath(const std::string &basePath) override;

    /**
     * @brief 검색 경로 추가
     * @param searchPath 추가할 검색 경로
     */
    void addSearchPath(const std::string &searchPath);

    /**
     * @brief 처리 중 발생한 오류 메시지 반환
     * @return 오류 메시지 목록
     */
    const std::vector<std::string> &getErrorMessages() const override;

    /**
     * @brief 처리 중 발생한 경고 메시지 반환
     * @return 경고 메시지 목록
     */
    const std::vector<std::string> &getWarningMessages() const;

    /**
     * @brief 이미 처리된 파일 목록 반환
     * @return 처리된 파일 목록 (경로 -> 노드 개수)
     */
    const std::unordered_map<std::string, int> &getProcessedFiles() const;

private:
    /**
     * @brief XInclude 요소 찾기 및 처리
     * @param element 검색 시작 요소
     * @param baseDir 기준 디렉토리
     * @return 처리된 XInclude 요소 수
     */
    int findAndProcessXIncludes(xmlpp::Element *element, const std::string &baseDir);

    /**
     * @brief 단일 XInclude 요소 처리
     * @param xincludeElement XInclude 요소
     * @param baseDir 기준 디렉토리
     * @return 성공 여부
     */
    bool processXIncludeElement(xmlpp::Element *xincludeElement, const std::string &baseDir);

    /**
     * @brief 외부 파일 로드 및 병합
     * @param href 파일 경로
     * @param xincludeElement XInclude 요소
     * @param baseDir 기준 디렉토리
     * @return 성공 여부
     */
    bool loadAndMergeFile(const std::string &href, xmlpp::Element *xincludeElement, const std::string &baseDir);

    /**
     * @brief 파일 경로 해석
     * @param href 원본 경로
     * @param baseDir 기준 디렉토리
     * @return 해석된 절대 경로
     */
    std::string resolveFilePath(const std::string &href, const std::string &baseDir);

    /**
     * @brief 오류 메시지 추가
     * @param message 오류 메시지
     */
    void addError(const std::string &message);

    /**
     * @brief 경고 메시지 추가
     * @param message 경고 메시지
     */
    void addWarning(const std::string &message);

    std::string basePath_;
    std::vector<std::string> searchPaths_;
    std::vector<std::string> errorMessages_;
    std::vector<std::string> warningMessages_;
    std::unordered_map<std::string, int> processedFiles_;
    bool isProcessing_;
    int maxRecursionDepth_;
    int currentRecursionDepth_;
};
