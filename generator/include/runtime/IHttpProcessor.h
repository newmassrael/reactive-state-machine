#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace SCXML {
namespace Runtime {

/**
 * HTTP 요청/응답 데이터 구조
 */
struct HttpRequest {
    std::string method;  // GET, POST, PUT, DELETE
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    int timeoutMs = 30000;  // 30초 기본 타임아웃
};

struct HttpResponse {
    int statusCode = 0;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string errorMessage;

    bool isSuccess() const {
        return statusCode >= 200 && statusCode < 300;
    }
};

/**
 * HTTP 처리 결과 콜백
 */
using HttpCallback = std::function<void(const HttpResponse &)>;

/**
 * HTTP I/O Processor 인터페이스
 *
 * 다양한 HTTP 구현체를 플러그인 방식으로 선택할 수 있도록 추상화
 * - LightweightHttpProcessor: 의존성 없는 기본 구현
 * - LibcurlHttpProcessor: libcurl 기반 고성능 구현
 * - MockHttpProcessor: 테스트용 시뮬레이션
 */
class IHttpProcessor {
public:
    virtual ~IHttpProcessor() = default;

    /**
     * 비동기 HTTP 요청 실행
     * @param request HTTP 요청 정보
     * @param callback 완료시 호출될 콜백
     * @return 요청 ID (취소용)
     */
    virtual std::string sendAsync(const HttpRequest &request, HttpCallback callback) = 0;

    /**
     * 동기 HTTP 요청 실행 (테스트용)
     * @param request HTTP 요청 정보
     * @return HTTP 응답
     */
    virtual HttpResponse sendSync(const HttpRequest &request) = 0;

    /**
     * 진행중인 요청 취소
     * @param requestId 요청 ID
     * @return 취소 성공 여부
     */
    virtual bool cancel(const std::string &requestId) = 0;

    /**
     * 프로세서 정리 및 리소스 해제
     */
    virtual void cleanup() = 0;

    /**
     * 프로세서 타입 식별
     */
    virtual std::string getType() const = 0;
};

/**
 * HTTP Processor 팩토리
 */
class HttpProcessorFactory {
public:
    enum class Type {
        LIGHTWEIGHT,  // 기본 경량 구현
        LIBCURL,      // libcurl 기반 구현
        MOCK          // 테스트용 모크
    };

    static std::unique_ptr<IHttpProcessor> create(Type type = Type::LIGHTWEIGHT);
    static std::unique_ptr<IHttpProcessor> createFromConfig(const std::string &config);
};

}  // namespace Runtime
}  // namespace SCXML