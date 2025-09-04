#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>

namespace SCXML {

// Forward declaration
namespace Core {
class IHttpProcessor;
}

namespace Runtime {

enum class HttpProcessorType;

/**
 * SCXML HTTP I/O Processor
 *
 * W3C SCXML 사양의 HTTP I/O Processor 구현:
 * - HTTP 요청을 통한 외부 서비스 통신
 * - <send> 태그를 통한 이벤트 전송
 * - done.invoke.* 및 error.communication 이벤트 생성
 * - 비동기 요청 처리 및 응답 이벤트 매핑
 */
class HttpIOProcessor {
private:
    std::unique_ptr<Model::IHttpProcessor> httpProcessor_;
    std::function<void(const std::string &, const std::map<std::string, std::string> &)> eventCallback_;

public:
    /**
     * HTTP 프로세서 타입
     */
    enum class ProcessorType {
        LIGHTWEIGHT,  // 기본 경량 구현 (의존성 없음)
        LIBCURL,      // libcurl 기반 구현 (고성능)
        MOCK          // 테스트용 모크 구현
    };

    /**
     * 생성자
     * @param processorType 사용할 HTTP 프로세서 타입
     */
    explicit HttpIOProcessor(ProcessorType processorType = ProcessorType::LIGHTWEIGHT);

    /**
     * 소멸자
     */
    ~HttpIOProcessor();

    /**
     * 이벤트 콜백 함수 등록
     * SCXML 상태 머신에서 이벤트를 받기 위해 호출
     *
     * @param callback 이벤트 수신 콜백 함수
     */
    void
    setEventCallback(std::function<void(const std::string &, const std::map<std::string, std::string> &)> callback);

    /**
     * SCXML 이벤트 전송 (<send> 태그 처리)
     *
     * @param target 대상 HTTP URL
     * @param event 전송할 이벤트 이름
     * @param params 이벤트 파라미터들
     * @param sendId 요청 식별자 (선택적)
     * @param delay 전송 지연시간 (밀리초, 선택적)
     */
    void sendEvent(const std::string &target, const std::string &event,
                   const std::map<std::string, std::string> &params = {}, const std::string &sendId = "",
                   int delay = 0);

    /**
     * HTTP GET 요청 (데이터 조회)
     *
     * @param url 요청 URL
     * @param params URL 쿼리 파라미터들 (선택적)
     * @param sendId 요청 식별자 (선택적)
     */
    void getData(const std::string &url, const std::map<std::string, std::string> &params = {},
                 const std::string &sendId = "");

    /**
     * HTTP POST 요청 (데이터 전송)
     *
     * @param url 요청 URL
     * @param data 전송할 JSON 데이터
     * @param sendId 요청 식별자 (선택적)
     */
    void postData(const std::string &url, const std::map<std::string, std::string> &data,
                  const std::string &sendId = "");

    /**
     * HTTP PUT 요청 (데이터 업데이트)
     *
     * @param url 요청 URL
     * @param data 업데이트할 JSON 데이터
     * @param sendId 요청 식별자 (선택적)
     */
    void putData(const std::string &url, const std::map<std::string, std::string> &data,
                 const std::string &sendId = "");

    /**
     * HTTP DELETE 요청 (데이터 삭제)
     *
     * @param url 요청 URL
     * @param sendId 요청 식별자 (선택적)
     */
    void deleteData(const std::string &url, const std::string &sendId = "");

    /**
     * 진행중인 요청 취소
     *
     * @param sendId 요청 식별자
     * @return 취소 성공 여부
     */
    bool cancelRequest(const std::string &sendId);

    /**
     * 모든 리소스 정리
     */
    void cleanup();

    /**
     * 현재 사용중인 HTTP 프로세서 타입 반환
     *
     * @return 프로세서 타입 문자열
     */
    std::string getProcessorType() const;

    /**
     * 프로세서 설정 정보 반환
     *
     * @return 설정 정보 맵
     */
    std::map<std::string, std::string> getConfiguration() const;

private:
    // 내부 구현 메서드들
    void executeHttpRequest(const struct HttpRequest &request, const std::string &sendId);
    void handleHttpResponse(const struct HttpResponse &response, const std::string &sendId);

    // 이벤트 발생 메서드들
    void fireSuccessEvent(const std::string &eventName, const std::map<std::string, std::string> &data,
                          const std::string &sendId);
    void fireErrorEvent(const std::string &eventName, const std::string &errorMsg, const std::string &sendId);

    // 유틸리티 메서드들
    std::string buildScxmlEventJson(const std::string &event, const std::map<std::string, std::string> &params);
    std::string buildJsonFromMap(const std::map<std::string, std::string> &data);
    std::string buildUrlWithParams(const std::string &baseUrl, const std::map<std::string, std::string> &params);
    std::string escapeJson(const std::string &str);
    std::string urlEncode(const std::string &str);
};

/**
 * HTTP I/O Processor 팩토리
 */
class HttpIOProcessorFactory {
public:
    /**
     * 기본 HTTP I/O Processor 생성
     */
    static std::unique_ptr<HttpIOProcessor> createDefault();

    /**
     * 설정 기반 HTTP I/O Processor 생성
     *
     * @param config 설정 문자열 ("lightweight", "libcurl", "mock")
     */
    static std::unique_ptr<HttpIOProcessor> createFromConfig(const std::string &config);

    /**
     * 타입 기반 HTTP I/O Processor 생성
     *
     * @param type 프로세서 타입
     */
    static std::unique_ptr<HttpIOProcessor> create(HttpIOProcessor::ProcessorType type);
};

}  // namespace Runtime
}  // namespace SCXML