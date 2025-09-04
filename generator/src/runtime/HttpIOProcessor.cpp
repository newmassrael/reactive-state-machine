#include "runtime/HttpIOProcessor.h"
#include "runtime/IHttpProcessor.h"
#include <iostream>
#include <sstream>

namespace SCXML {
namespace Runtime {

/**
 * SCXML과 HTTP I/O Processor 통합
 *
 * SCXML 사양에 따른 HTTP I/O Processor:
 * - <send> 태그를 통한 HTTP 요청 전송
 * - 외부 서비스와의 비동기 통신
 * - done.invoke.* 이벤트 생성
 * - error.communication 이벤트 처리
 */
class HttpIOProcessor {
private:
    std::unique_ptr<IHttpProcessor> httpProcessor_;
    std::function<void(const std::string &, const std::map<std::string, std::string> &)> eventCallback_;

public:
    explicit HttpIOProcessor(HttpProcessorFactory::Type processorType = HttpProcessorFactory::Type::LIGHTWEIGHT)
        : httpProcessor_(HttpProcessorFactory::create(processorType)) {}

    /**
     * 이벤트 콜백 등록 (SCXML 상태 머신이 등록)
     */
    void
    setEventCallback(std::function<void(const std::string &, const std::map<std::string, std::string> &)> callback) {
        eventCallback_ = callback;
    }

    /**
     * SCXML <send> 태그 처리
     *
     * @param target HTTP URL
     * @param event 전송할 이벤트 이름
     * @param params 전송할 파라미터들
     * @param sendId 요청 식별자
     * @param delay 전송 지연시간 (ms)
     */
    void sendEvent(const std::string &target, const std::string &event,
                   const std::map<std::string, std::string> &params, const std::string &sendId = "", int delay = 0) {
        if (target.empty()) {
            fireErrorEvent("error.communication", "Empty target URL", sendId);
            return;
        }

        // HTTP 요청 구성
        HttpRequest request;
        request.url = target;
        request.method = "POST";    // SCXML 이벤트는 POST로 전송
        request.timeoutMs = 30000;  // 30초 타임아웃

        // SCXML 이벤트를 JSON으로 변환
        request.body = buildScxmlEventJson(event, params);
        request.headers["Content-Type"] = "application/json";
        request.headers["Accept"] = "application/json";

        // User-Agent 설정
        request.headers["User-Agent"] = "SCXML-HttpIOProcessor/1.0";

        // 지연 전송 처리
        if (delay > 0) {
            std::thread([this, request, sendId, delay]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                executeHttpRequest(request, sendId);
            }).detach();
        } else {
            executeHttpRequest(request, sendId);
        }
    }

    /**
     * HTTP GET 요청 (데이터 조회용)
     */
    void getData(const std::string &url, const std::map<std::string, std::string> &params = {},
                 const std::string &sendId = "") {
        HttpRequest request;
        request.method = "GET";
        request.url = buildUrlWithParams(url, params);
        request.headers["Accept"] = "application/json";
        request.headers["User-Agent"] = "SCXML-HttpIOProcessor/1.0";

        executeHttpRequest(request, sendId);
    }

    /**
     * HTTP POST 요청 (데이터 전송용)
     */
    void postData(const std::string &url, const std::map<std::string, std::string> &data,
                  const std::string &sendId = "") {
        HttpRequest request;
        request.method = "POST";
        request.url = url;
        request.body = buildJsonFromMap(data);
        request.headers["Content-Type"] = "application/json";
        request.headers["Accept"] = "application/json";
        request.headers["User-Agent"] = "SCXML-HttpIOProcessor/1.0";

        executeHttpRequest(request, sendId);
    }

    /**
     * 요청 취소
     */
    bool cancelRequest(const std::string &sendId) {
        return httpProcessor_->cancel(sendId);
    }

    /**
     * 프로세서 정리
     */
    void cleanup() {
        if (httpProcessor_) {
            httpProcessor_->cleanup();
        }
    }

    /**
     * 프로세서 타입 반환
     */
    std::string getProcessorType() const {
        return httpProcessor_ ? httpProcessor_->getType() : "None";
    }

private:
    void executeHttpRequest(const HttpRequest &request, const std::string &sendId) {
        if (!httpProcessor_) {
            fireErrorEvent("error.communication", "HTTP processor not initialized", sendId);
            return;
        }

        // 비동기 요청 실행
        httpProcessor_->sendAsync(
            request, [this, sendId](const HttpResponse &response) { handleHttpResponse(response, sendId); });
    }

    void handleHttpResponse(const HttpResponse &response, const std::string &sendId) {
        if (response.isSuccess()) {
            // 성공 이벤트 발생
            std::map<std::string, std::string> eventData;
            eventData["status"] = std::to_string(response.statusCode);
            eventData["data"] = response.body;
            eventData["sendId"] = sendId;

            // Content-Type에 따른 데이터 처리
            auto contentType = response.headers.find("Content-Type");
            if (contentType != response.headers.end()) {
                eventData["contentType"] = contentType->second;
            }

            fireSuccessEvent("done.invoke", eventData, sendId);

        } else {
            // 오류 이벤트 발생
            std::string errorMsg =
                response.errorMessage.empty() ? "HTTP " + std::to_string(response.statusCode) : response.errorMessage;

            fireErrorEvent("error.communication", errorMsg, sendId);
        }
    }

    void fireSuccessEvent(const std::string &eventName, const std::map<std::string, std::string> &data,
                          const std::string &sendId) {
        if (eventCallback_) {
            auto eventData = data;
            if (!sendId.empty()) {
                eventData["sendId"] = sendId;
            }
            eventCallback_(eventName, eventData);
        }
    }

    void fireErrorEvent(const std::string &eventName, const std::string &errorMsg, const std::string &sendId) {
        if (eventCallback_) {
            std::map<std::string, std::string> eventData;
            eventData["error"] = errorMsg;
            if (!sendId.empty()) {
                eventData["sendId"] = sendId;
            }
            eventCallback_(eventName, eventData);
        }
    }

    std::string buildScxmlEventJson(const std::string &event, const std::map<std::string, std::string> &params) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"event\": \"" << escapeJson(event) << "\",\n";
        json << "  \"type\": \"scxml\",\n";
        json << "  \"data\": {\n";

        bool first = true;
        for (const auto &[key, value] : params) {
            if (!first) {
                json << ",\n";
            }
            json << "    \"" << escapeJson(key) << "\": \"" << escapeJson(value) << "\"";
            first = false;
        }

        json << "\n  }\n";
        json << "}";
        return json.str();
    }

    std::string buildJsonFromMap(const std::map<std::string, std::string> &data) {
        std::ostringstream json;
        json << "{\n";

        bool first = true;
        for (const auto &[key, value] : data) {
            if (!first) {
                json << ",\n";
            }
            json << "  \"" << escapeJson(key) << "\": \"" << escapeJson(value) << "\"";
            first = false;
        }

        json << "\n}";
        return json.str();
    }

    std::string buildUrlWithParams(const std::string &baseUrl, const std::map<std::string, std::string> &params) {
        if (params.empty()) {
            return baseUrl;
        }

        std::ostringstream url;
        url << baseUrl;

        if (baseUrl.find('?') == std::string::npos) {
            url << "?";
        } else {
            url << "&";
        }

        bool first = true;
        for (const auto &[key, value] : params) {
            if (!first) {
                url << "&";
            }
            url << urlEncode(key) << "=" << urlEncode(value);
            first = false;
        }

        return url.str();
    }

    std::string escapeJson(const std::string &str) {
        std::string result;
        for (char c : str) {
            switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += c;
                break;
            }
        }
        return result;
    }

    std::string urlEncode(const std::string &str) {
        std::ostringstream encoded;
        for (char c : str) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded << c;
            } else {
                encoded << '%' << std::hex << std::uppercase << (unsigned char)c;
            }
        }
        return encoded.str();
    }
};

}  // namespace Runtime
}  // namespace SCXML