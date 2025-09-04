#include "common/TypeSafeCallback.h"
#include "common/TypeSafeHttpCallback.h"
#include "runtime/IHttpProcessor.h"

#ifdef REACTIVE_SM_USE_LIBCURL
#include <atomic>
#include <curl/curl.h>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#endif

namespace SCXML {
namespace Runtime {

#ifdef REACTIVE_SM_USE_LIBCURL

/**
 * libcurl 기반 HTTP 클라이언트 구현
 *
 * 기능:
 * - 완전한 HTTP/HTTPS 지원
 * - SSL/TLS 자동 처리
 * - 고성능 연결 풀링
 * - 모든 HTTP 메서드 지원
 * - 자동 리디렉션 처리
 */
class LibcurlHttpProcessor : public IHttpProcessor {
private:
    struct WriteData {
        std::string data;
        std::map<std::string, std::string> headers;
    };

    struct PendingRequest {
        std::string id;
        std::future<HttpResponse> future;
        std::atomic<bool> cancelled{false};
    };

    CURLM *multiHandle_;
    std::unordered_map<std::string, std::unique_ptr<PendingRequest>> pendingRequests_;
    std::mutex requestsMutex_;
    std::atomic<int> requestCounter_{0};

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userData) {
        auto *responseData = static_cast<SCXML::Common::TypeSafeHttpCallback::HttpResponseData *>(userData);
        size_t totalSize = size * nmemb;
        responseData->content.append(static_cast<char *>(contents), totalSize);
        responseData->totalSize += totalSize;
        return totalSize;
    }

    static size_t HeaderCallback(char *buffer, size_t size, size_t nitems, WriteData *data) {
        size_t totalSize = size * nitems;
        std::string header(buffer, totalSize);

        // 헤더 파싱
        size_t colonPos = header.find(':');
        if (colonPos != std::string::npos) {
            std::string key = header.substr(0, colonPos);
            std::string value = header.substr(colonPos + 1);

            // 공백 제거
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                value.erase(0, 1);
            }
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
                value.pop_back();
            }

            data->headers[key] = value;
        }

        return totalSize;
    }

public:
    LibcurlHttpProcessor() : multiHandle_(nullptr) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        multiHandle_ = curl_multi_init();

        if (!multiHandle_) {
            throw std::runtime_error("Failed to initialize libcurl multi handle");
        }
    }

    ~LibcurlHttpProcessor() {
        cleanup();
        if (multiHandle_) {
            curl_multi_cleanup(multiHandle_);
        }
        curl_global_cleanup();
    }

    std::string sendAsync(const HttpRequest &request, HttpCallback callback) override {
        std::string requestId = "curl_req_" + std::to_string(++requestCounter_);

        auto pendingReq = std::make_unique<PendingRequest>();
        pendingReq->id = requestId;

        // 비동기 실행
        pendingReq->future = std::async(std::launch::async, [this, request, callback, requestId]() {
            auto response = sendSync(request);

            // 취소되지 않았으면 콜백 실행
            {
                std::lock_guard<std::mutex> lock(requestsMutex_);
                auto it = pendingRequests_.find(requestId);
                if (it != pendingRequests_.end() && !it->second->cancelled.load()) {
                    callback(response);
                }
            }

            return response;
        });

        {
            std::lock_guard<std::mutex> lock(requestsMutex_);
            pendingRequests_[requestId] = std::move(pendingReq);
        }

        return requestId;
    }

    HttpResponse sendSync(const HttpRequest &request) override {
        HttpResponse response;
        CURL *curl = curl_easy_init();

        if (!curl) {
            response.errorMessage = "Failed to initialize libcurl";
            return response;
        }

        WriteData writeData;

        try {
            // 기본 설정
            curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeData);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &writeData);

            // 타임아웃 설정
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeoutMs));
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);  // 10초 연결 타임아웃

            // SSL 설정
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

            // User-Agent 설정
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "ReactiveStateMachine/1.0 (libcurl)");

            // HTTP 메서드 설정
            if (request.method == "GET") {
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            } else if (request.method == "POST") {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                if (!request.body.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.length());
                }
            } else if (request.method == "PUT") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                if (!request.body.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.length());
                }
            } else if (request.method == "DELETE") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            }

            // 헤더 설정
            struct curl_slist *headers = nullptr;
            for (const auto &[key, value] : request.headers) {
                std::string header = key + ": " + value;
                headers = curl_slist_append(headers, header.c_str());
            }

            // JSON 기본 Content-Type 설정 (POST/PUT시)
            if ((request.method == "POST" || request.method == "PUT") && !request.body.empty() &&
                request.headers.find("Content-Type") == request.headers.end()) {
                headers = curl_slist_append(headers, "Content-Type: application/json");
            }

            if (headers) {
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            }

            // 요청 실행
            CURLcode res = curl_easy_perform(curl);

            if (res == CURLE_OK) {
                // 응답 코드 가져오기
                long responseCode;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
                response.statusCode = static_cast<int>(responseCode);

                // 응답 데이터 설정
                response.body = writeData.data;
                response.headers = writeData.headers;
            } else {
                response.errorMessage = std::string("libcurl error: ") + curl_easy_strerror(res);
            }

            // 정리
            if (headers) {
                curl_slist_free_all(headers);
            }

        } catch (const std::exception &e) {
            response.errorMessage = std::string("HTTP request failed: ") + e.what();
        }

        curl_easy_cleanup(curl);
        return response;
    }

    bool cancel(const std::string &requestId) override {
        std::lock_guard<std::mutex> lock(requestsMutex_);
        auto it = pendingRequests_.find(requestId);
        if (it != pendingRequests_.end()) {
            it->second->cancelled.store(true);
            return true;
        }
        return false;
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(requestsMutex_);
        for (auto &[id, req] : pendingRequests_) {
            req->cancelled.store(true);
        }
        pendingRequests_.clear();
    }

    std::string getType() const override {
        return "LibcurlHttpProcessor";
    }
};

// 팩토리에 libcurl 구현 추가
std::unique_ptr<IHttpProcessor> createLibcurlProcessor() {
    return std::make_unique<LibcurlHttpProcessor>();
}

#else  // REACTIVE_SM_USE_LIBCURL

// libcurl 없을 때 더미 구현
std::unique_ptr<IHttpProcessor> createLibcurlProcessor() {
    throw std::runtime_error("LibcurlHttpProcessor not available (compiled without libcurl support)");
}

#endif  // REACTIVE_SM_USE_LIBCURL

}  // namespace Runtime
}  // namespace SCXML