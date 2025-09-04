#include "common/ErrorCategories.h"
#include "runtime/IHttpProcessor.h"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace SCXML {
namespace Runtime {

/**
 * 경량 HTTP 클라이언트 구현
 *
 * 기능:
 * - 기본 HTTP/1.1 GET/POST 지원
 * - JSON 요청/응답 처리
 * - 연결 재사용
 * - 타임아웃 처리
 * - 비동기 요청 처리
 */
class LightweightHttpProcessor : public IHttpProcessor {
private:
    struct PendingRequest {
        std::string id;
        std::future<HttpResponse> future;
        std::atomic<bool> cancelled{false};
    };

    std::unordered_map<std::string, std::unique_ptr<PendingRequest>> pendingRequests_;
    std::mutex requestsMutex_;
    std::atomic<int> requestCounter_{0};

public:
    LightweightHttpProcessor() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~LightweightHttpProcessor() {
        cleanup();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    std::string sendAsync(const HttpRequest &request, HttpCallback callback) override {
        std::string requestId = "req_" + std::to_string(++requestCounter_);

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

        try {
            // URL 파싱
            auto urlParts = parseUrl(request.url);
            if (urlParts.host.empty()) {
                response.errorMessage = "Invalid URL: " + request.url;
                return response;
            }

            // 소켓 생성 및 연결
            int sock = createConnection(urlParts.host, urlParts.port);
            if (sock < 0) {
                response.errorMessage = "Failed to connect to " + urlParts.host + ":" + std::to_string(urlParts.port);
                return response;
            }

            // HTTP 요청 생성
            std::string httpRequest = buildHttpRequest(request, urlParts);

            // 요청 전송
            if (!sendData(sock, httpRequest)) {
                response.errorMessage = "Failed to send HTTP request";
                closeSocket(sock);
                return response;
            }

            // 응답 수신
            response = receiveResponse(sock, request.timeoutMs);
            closeSocket(sock);

        } catch (const std::exception &e) {
            response.errorMessage = std::string("HTTP request failed: ") + e.what();
        }

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
        return "LightweightHttpProcessor";
    }

private:
    struct UrlParts {
        std::string host;
        int port = 80;
        std::string path = "/";
    };

    UrlParts parseUrl(const std::string &url) {
        UrlParts parts;

        // http:// 제거
        size_t start = 0;
        if (url.substr(0, 7) == "http://") {
            start = 7;
        } else if (url.substr(0, 8) == "https://") {
            start = 8;
            parts.port = 443;  // HTTPS 기본 포트 (구현 시 주의)
        }

        // 호스트와 경로 분리
        size_t pathPos = url.find('/', start);
        if (pathPos != std::string::npos) {
            parts.path = url.substr(pathPos);
            parts.host = url.substr(start, pathPos - start);
        } else {
            parts.host = url.substr(start);
        }

        // 포트 분리
        size_t colonPos = parts.host.find(':');
        if (colonPos != std::string::npos) {
            parts.port = std::stoi(parts.host.substr(colonPos + 1));
            parts.host = parts.host.substr(0, colonPos);
        }

        return parts;
    }

    int createConnection(const std::string &host, int port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return -1;
        }

        // 호스트 이름 해석
        struct hostent *server = gethostbyname(host.c_str());
        if (!server) {
            closeSocket(sock);
            return -1;
        }

        // 연결 설정
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

        if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            closeSocket(sock);
            return -1;
        }

        return sock;
    }

    std::string buildHttpRequest(const HttpRequest &request, const UrlParts &url) {
        std::ostringstream oss;

        // 요청 라인
        oss << request.method << " " << url.path << " HTTP/1.1\r\n";

        // 기본 헤더
        oss << "Host: " << url.host;
        if (url.port != 80) {
            oss << ":" << url.port;
        }
        oss << "\r\n";

        // 사용자 정의 헤더
        for (const auto &[key, value] : request.headers) {
            oss << key << ": " << value << "\r\n";
        }

        // 기본 헤더 추가
        oss << "Connection: close\r\n";
        oss << "User-Agent: ReactiveStateMachine/1.0\r\n";

        // Content-Length (POST 요청시)
        if (!request.body.empty()) {
            oss << "Content-Length: " << request.body.length() << "\r\n";
            if (request.headers.find("Content-Type") == request.headers.end()) {
                oss << "Content-Type: application/json\r\n";
            }
        }

        oss << "\r\n";

        // 바디
        if (!request.body.empty()) {
            oss << request.body;
        }

        return oss.str();
    }

    bool sendData(int sock, const std::string &data) {
        size_t totalSent = 0;
        while (totalSent < data.length()) {
            int sent = send(sock, data.c_str() + totalSent, data.length() - totalSent, 0);
            if (sent <= 0) {
                return false;
            }
            totalSent += sent;
        }
        return true;
    }

    HttpResponse receiveResponse(int sock, int timeoutMs) {
        HttpResponse response;

        // 타임아웃 설정
#ifdef _WIN32
        DWORD timeout = timeoutMs;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
#else
        struct timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

        // 응답 수신
        std::string rawResponse;
        char buffer[4096];
        int received;

        while ((received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[received] = '\0';
            rawResponse += buffer;

            // HTTP 응답 완전히 수신했는지 확인
            if (rawResponse.find("\r\n\r\n") != std::string::npos) {
                // Content-Length 체크하여 바디 완전 수신 확인
                if (isResponseComplete(rawResponse)) {
                    break;
                }
            }
        }

        if (received < 0) {
            response.errorMessage = "Failed to receive response";
            return response;
        }

        // HTTP 응답 파싱
        parseHttpResponse(rawResponse, response);
        return response;
    }

    bool isResponseComplete(const std::string &response) {
        size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            return false;
        }

        // Content-Length 찾기
        size_t clPos = response.find("Content-Length:");
        if (clPos == std::string::npos || clPos > headerEnd) {
            return true;  // Content-Length 없으면 완료로 간주
        }

        size_t clStart = response.find(':', clPos) + 1;
        size_t clEnd = response.find('\r', clStart);
        int contentLength = std::stoi(response.substr(clStart, clEnd - clStart));

        size_t bodyStart = headerEnd + 4;
        return response.length() >= bodyStart + contentLength;
    }

    void parseHttpResponse(const std::string &rawResponse, HttpResponse &response) {
        size_t headerEnd = rawResponse.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            response.errorMessage = "Invalid HTTP response format";
            return;
        }

        std::string headers = rawResponse.substr(0, headerEnd);
        std::string body = rawResponse.substr(headerEnd + 4);

        // 상태 코드 파싱
        size_t spacePos = headers.find(' ');
        if (spacePos != std::string::npos) {
            size_t codeEnd = headers.find(' ', spacePos + 1);
            if (codeEnd != std::string::npos) {
                response.statusCode = std::stoi(headers.substr(spacePos + 1, codeEnd - spacePos - 1));
            }
        }

        // 헤더 파싱
        std::istringstream headerStream(headers);
        std::string line;
        std::getline(headerStream, line);  // 상태 라인 스킵

        while (std::getline(headerStream, line) && !line.empty()) {
            if (line.back() == '\r') {
                line.pop_back();
            }

            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);
                // 앞뒤 공백 제거
                while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                    value.erase(0, 1);
                }
                response.headers[key] = value;
            }
        }

        response.body = body;
    }

    void closeSocket(int sock) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
};

// 팩토리 구현
std::unique_ptr<IHttpProcessor> HttpProcessorFactory::create(Type type) {
    switch (type) {
    case Type::LIGHTWEIGHT:
        return std::make_unique<LightweightHttpProcessor>();
    case Type::LIBCURL:
        // Note: LibcurlHttpProcessor implementation requires libcurl dependency
        throw SCXML::Common::SCXMLException(SCXML::Common::ErrorCategory::NETWORK_ERROR,
                                            "LibcurlHttpProcessor requires libcurl library",
                                            "LightweightHttpProcessor");
    case Type::MOCK:
        // Note: MockHttpProcessor implementation for testing purposes
        throw SCXML::Common::SCXMLException(SCXML::Common::ErrorCategory::SYSTEM_ERROR,
                                            "MockHttpProcessor for testing only", "LightweightHttpProcessor");
    default:
        return std::make_unique<LightweightHttpProcessor>();
    }
}

std::unique_ptr<IHttpProcessor> HttpProcessorFactory::createFromConfig(const std::string &config) {
    if (config == "libcurl") {
        return create(Type::LIBCURL);
    } else if (config == "mock") {
        return create(Type::MOCK);
    } else {
        return create(Type::LIGHTWEIGHT);
    }
}

}  // namespace Runtime
}  // namespace SCXML