#include "../../include/runtime/NativeHttpClient.h"
#include "../../include/Logger.h"
#include "common/ErrorCategories.h"
#include "common/GracefulJoin.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <regex>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

using namespace SCXML::Runtime;

NativeHttpClient::NativeHttpClient() {
    // Start worker thread for async requests
    workerThread_ = std::thread(&NativeHttpClient::workerThread, this);
    Logger::info("NativeHttpClient: HTTP client initialized with real network implementation");
}

NativeHttpClient::~NativeHttpClient() {
    shutdown_ = true;
    requestCondition_.notify_all();

    if (workerThread_.joinable()) {
        SCXML::GracefulJoin::joinWithTimeout(workerThread_, 3, "NativeHttpClient_Worker");
    }

    Logger::info("NativeHttpClient: HTTP client shutdown completed");
}

NativeHttpClient::Response NativeHttpClient::execute(const Request &request) {
    std::string msg = "NativeHttpClient: Executing synchronous HTTP " + request.method + " to " + request.url;
    Logger::debug(msg);

    try {
        return executeHTTPRequest(request);
    } catch (const std::exception &e) {
        Response errorResponse;
        errorResponse.success = false;
        errorResponse.error = std::string("HTTP request failed: ") + e.what();
        Logger::error("NativeHttpClient: " + errorResponse.error);
        return errorResponse;
    }
}

std::string NativeHttpClient::executeAsync(const Request &request, ResponseCallback callback) {
    std::string requestId = generateRequestId();

    std::string msg =
        "NativeHttpClient: Queuing async HTTP " + request.method + " to " + request.url + " (ID: " + requestId + ")";
    Logger::debug(msg);

    auto pendingRequest = std::make_unique<PendingRequest>();
    pendingRequest->id = requestId;
    pendingRequest->request = request;
    pendingRequest->callback = callback;
    pendingRequest->startTime = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(pendingRequestsMutex_);
        pendingRequests_.push(std::move(pendingRequest));
    }

    requestCondition_.notify_one();
    return requestId;
}

bool NativeHttpClient::cancelRequest(const std::string &requestId) {
    std::lock_guard<std::mutex> lock(pendingRequestsMutex_);

    auto it = activeRequests_.find(requestId);
    if (it != activeRequests_.end()) {
        it->second->cancelled = true;
        Logger::info("NativeHttpClient: Cancelled request: " + requestId);
        return true;
    }

    Logger::info("NativeHttpClient: Request not found for cancellation: " + requestId);
    return false;
}

size_t NativeHttpClient::getPendingRequestsCount() const {
    std::lock_guard<std::mutex> lock(pendingRequestsMutex_);
    return pendingRequests_.size() + activeRequests_.size();
}

void NativeHttpClient::setGlobalTimeout(std::chrono::milliseconds timeoutMs) {
    globalTimeout_ = timeoutMs;
    std::string msg = "NativeHttpClient: Global timeout set to " + std::to_string(timeoutMs.count()) + "ms";
    Logger::info(msg);
}

void NativeHttpClient::setMaxConcurrentRequests(size_t maxRequests) {
    maxConcurrentRequests_ = maxRequests;
    std::string msg = "NativeHttpClient: Max concurrent requests set to " + std::to_string(maxRequests);
    Logger::info(msg);
}

NativeHttpClient::ParsedURL NativeHttpClient::parseURL(const std::string &url) {
    ParsedURL parsed;

    // Parse protocol
    size_t protocolEnd = url.find("://");
    if (protocolEnd != std::string::npos) {
        parsed.protocol = url.substr(0, protocolEnd);
        parsed.isSSL = (parsed.protocol == "https");
        parsed.port = parsed.isSSL ? 443 : 80;
    } else {
        throw SCXML::Common::SCXMLException(SCXML::Common::ErrorCategory::NETWORK_ERROR, "Invalid URL format: " + url,
                                            "NativeHttpClient");
    }

    // Parse host and path
    size_t hostStart = protocolEnd + 3;
    size_t pathStart = url.find('/', hostStart);

    if (pathStart == std::string::npos) {
        parsed.host = url.substr(hostStart);
        parsed.path = "/";
    } else {
        parsed.host = url.substr(hostStart, pathStart - hostStart);
        parsed.path = url.substr(pathStart);
    }

    // Parse port if specified
    size_t portStart = parsed.host.find(':');
    if (portStart != std::string::npos) {
        parsed.port = std::stoi(parsed.host.substr(portStart + 1));
        parsed.host = parsed.host.substr(0, portStart);
    }

    std::string msg = "NativeHttpClient: Parsed URL - Host: " + parsed.host + ", Port: " + std::to_string(parsed.port) +
                      ", Path: " + parsed.path;
    Logger::debug(msg);

    return parsed;
}

NativeHttpClient::Response NativeHttpClient::executeHTTPRequest(const Request &request) {
    ParsedURL parsedUrl = parseURL(request.url);

    // SSL/TLS not implemented in this basic version - return error for HTTPS
    if (parsedUrl.isSSL) {
        Response response;
        response.success = false;
        response.error = "HTTPS not supported in basic implementation. Use HTTP for testing.";
        return response;
    }

    // Create connection
    int socket = createConnection(parsedUrl.host, parsedUrl.port);
    if (socket < 0) {
        Response response;
        response.success = false;
        response.error = "Failed to connect to " + parsedUrl.host + ":" + std::to_string(parsedUrl.port);
        return response;
    }

    // Send request
    bool sent = sendRequest(socket, request, parsedUrl);
    if (!sent) {
        close(socket);
        Response response;
        response.success = false;
        response.error = "Failed to send HTTP request";
        return response;
    }

    // Receive response
    Response response = receiveResponse(socket, request);
    close(socket);

    // Handle redirects if enabled and response is a redirect
    if (request.followRedirects && response.statusCode >= 300 && response.statusCode < 400) {
        return handleRedirect(response, request);
    }

    return response;
}

int NativeHttpClient::createConnection(const std::string &host, int port) {
    std::string msg = "NativeHttpClient: Creating connection to " + host + ":" + std::to_string(port);
    Logger::debug(msg);

    // Resolve hostname
    struct hostent *hostEntry = gethostbyname(host.c_str());
    if (!hostEntry) {
        Logger::error("NativeHttpClient: Failed to resolve hostname: " + host);
        return -1;
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        Logger::error("NativeHttpClient: Failed to create socket");
        return -1;
    }

    // Set non-blocking for timeout support
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Setup address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);

    // Connect with timeout
    int result = connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (result < 0 && errno != EINPROGRESS) {
        close(sock);
        Logger::error("NativeHttpClient: Connection failed: " + std::string(strerror(errno)));
        return -1;
    }

    // Wait for connection to complete
    if (result < 0) {
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLOUT;

        int pollResult = poll(&pfd, 1, 10000);  // 10 second timeout
        if (pollResult <= 0) {
            close(sock);
            Logger::error("NativeHttpClient: Connection timeout");
            return -1;
        }

        // Check if connection succeeded
        int error;
        socklen_t len = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            close(sock);
            Logger::error("NativeHttpClient: Connection failed: " + std::string(strerror(error)));
            return -1;
        }
    }

    // Set socket back to blocking
    fcntl(sock, F_SETFL, flags);

    Logger::debug("NativeHttpClient: Connection established");
    return sock;
}

bool NativeHttpClient::sendRequest(int socket, const Request &request, const ParsedURL &parsedUrl) {
    std::string requestString = buildRequestString(request, parsedUrl);

    Logger::debug("NativeHttpClient: Sending HTTP request");

    ssize_t sent = send(socket, requestString.c_str(), requestString.length(), 0);
    if (sent < 0) {
        Logger::error("NativeHttpClient: Failed to send request: " + std::string(strerror(errno)));
        return false;
    }

    return true;
}

NativeHttpClient::Response NativeHttpClient::receiveResponse(int socket, const Request &request) {
    Response response;
    std::string responseData;
    char buffer[4096];

    // Set receive timeout
    struct timeval timeout;
    timeout.tv_sec = request.timeout.count() / 1000;
    timeout.tv_usec = (request.timeout.count() % 1000) * 1000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Read response with safety limits
    const size_t MAX_RESPONSE_SIZE = 10 * 1024 * 1024;  // 10MB limit
    const int MAX_READ_ITERATIONS = 1000;               // Prevent infinite loops
    int readIterations = 0;

    while (readIterations < MAX_READ_ITERATIONS && responseData.size() < MAX_RESPONSE_SIZE) {
        ssize_t received = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                response.success = false;
                response.error = "Response timeout";
                Logger::error("NativeHttpClient: Response timeout");
                return response;
            } else if (received < 0) {
                response.success = false;
                response.error = "Network error: " + std::string(strerror(errno));
                Logger::error("NativeHttpClient: Network error: " + std::string(strerror(errno)));
                return response;
            }
            break;  // End of data (received == 0)
        }

        buffer[received] = ' ';
        responseData += buffer;
        readIterations++;

        // Check if we have complete headers
        size_t headerEnd = responseData.find("

");
        if (headerEnd != std::string::npos) {
            // Check Content-Length to determine if we have complete body
            auto headerLines = parseHeaders({responseData.substr(0, headerEnd)});
            auto headers = headerLines.second;

            auto contentLengthIt = headers.find("content-length");
            if (contentLengthIt != headers.end()) {
                try {
                    size_t contentLength = std::stoul(contentLengthIt->second);
                    size_t bodyStart = headerEnd + 4;
                    size_t currentBodySize = responseData.length() - bodyStart;

                    if (currentBodySize >= contentLength) {
                        break;  // We have complete response
                    }
                } catch (const std::exception &e) {
                    Logger::warn("NativeHttpClient: Invalid Content-Length header");
                    break;  // Treat as complete on invalid Content-Length
                }
            } else {
                // No Content-Length, continue reading until connection close
                continue;
            }
        }
    }

    if (readIterations >= MAX_READ_ITERATIONS) {
        Logger::warn("NativeHttpClient: Maximum read iterations reached, response may be incomplete");
    }

    if (responseData.size() >= MAX_RESPONSE_SIZE) {
        response.success = false;
        response.error = "Response too large (exceeded " + std::to_string(MAX_RESPONSE_SIZE) + " bytes)";
        return response;
    }

    // Parse response
    size_t headerEnd = responseData.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        response.success = false;
        response.error = "Invalid HTTP response format";
        return response;
    }

    // Parse headers
    std::string headerSection = responseData.substr(0, headerEnd);
    std::vector<std::string> headerLines;
    std::istringstream stream(headerSection);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        headerLines.push_back(line);
    }

    auto parsedHeaders = parseHeaders(headerLines);
    response.statusCode = parsedHeaders.first;
    response.headers = parsedHeaders.second;
    response.body = responseData.substr(headerEnd + 4);
    response.success = (response.statusCode >= 200 && response.statusCode < 300);

    std::string msg = "NativeHttpClient: Received response: " + std::to_string(response.statusCode) + " (" +
                      std::to_string(response.body.length()) + " bytes)";
    Logger::debug(msg);

    return response;
}

std::pair<int, std::unordered_map<std::string, std::string>>
NativeHttpClient::parseHeaders(const std::vector<std::string> &headerLines) {
    std::unordered_map<std::string, std::string> headers;
    int statusCode = 200;

    if (!headerLines.empty()) {
        // Parse status line (e.g., "HTTP/1.1 200 OK")
        const std::string &statusLine = headerLines[0];
        std::istringstream statusStream(statusLine);
        std::string httpVersion, statusCodeStr;

        if (statusStream >> httpVersion >> statusCodeStr) {
            try {
                statusCode = std::stoi(statusCodeStr);
            } catch (...) {
                statusCode = 500;
            }
        }

        // Parse headers
        for (size_t i = 1; i < headerLines.size(); ++i) {
            const std::string &line = headerLines[i];
            size_t colonPos = line.find(':');

            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);

                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                // Convert key to lowercase for case-insensitive access
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                headers[key] = value;
            }
        }
    }

    return {statusCode, headers};
}

std::string NativeHttpClient::buildRequestString(const Request &request, const ParsedURL &parsedUrl) {
    std::ostringstream requestStream;

    // Request line
    requestStream << request.method << " " << parsedUrl.path << " HTTP/1.1\r\n";

    // Host header (required for HTTP/1.1)
    requestStream << "Host: " << parsedUrl.host;
    if ((parsedUrl.isSSL && parsedUrl.port != 443) || (!parsedUrl.isSSL && parsedUrl.port != 80)) {
        requestStream << ":" << parsedUrl.port;
    }
    requestStream << "\r\n";

    // User-Agent
    requestStream << "User-Agent: SCXML-NativeHttpClient/1.0\r\n";

    // Connection header
    requestStream << "Connection: close\r\n";

    // Content-Length for requests with body
    if (!request.body.empty()) {
        requestStream << "Content-Length: " << request.body.length() << "\r\n";
    }

    // Custom headers
    for (const auto &header : request.headers) {
        requestStream << header.first << ": " << header.second << "\r\n";
    }

    // End of headers
    requestStream << "\r\n";

    // Body
    if (!request.body.empty()) {
        requestStream << request.body;
    }

    return requestStream.str();
}

NativeHttpClient::Response NativeHttpClient::handleRedirect(const Response &response, Request request,
                                                            int redirectCount) {
    if (redirectCount >= MAX_REDIRECTS) {
        Response errorResponse;
        errorResponse.success = false;
        errorResponse.error = "Too many redirects";
        return errorResponse;
    }

    auto locationIt = response.headers.find("location");
    if (locationIt == response.headers.end()) {
        Response errorResponse;
        errorResponse.success = false;
        errorResponse.error = "Redirect response missing Location header";
        return errorResponse;
    }

    // Update request URL and follow redirect
    request.url = locationIt->second;
    Logger::info("NativeHttpClient: Following redirect to: " + request.url);

    Response redirectResponse = executeHTTPRequest(request);

    // Check for further redirects
    if (request.followRedirects && redirectResponse.statusCode >= 300 && redirectResponse.statusCode < 400) {
        return handleRedirect(redirectResponse, request, redirectCount + 1);
    }

    return redirectResponse;
}

std::string NativeHttpClient::urlEncode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

std::string NativeHttpClient::generateRequestId() {
    std::lock_guard<std::mutex> lock(requestIdMutex_);
    return "req_" + std::to_string(nextRequestId_++);
}

void NativeHttpClient::workerThread() {
    Logger::info("NativeHttpClient: Worker thread started");

    while (!shutdown_) {
        std::unique_ptr<PendingRequest> request;

        // Get next request
        {
            std::unique_lock<std::mutex> lock(pendingRequestsMutex_);
            requestCondition_.wait(lock, [this] { return shutdown_ || !pendingRequests_.empty(); });

            if (shutdown_) {
                break;
            }

            if (!pendingRequests_.empty()) {
                request = std::move(pendingRequests_.front());
                pendingRequests_.pop();
                activeRequests_[request->id] = std::unique_ptr<PendingRequest>(request.get());
            }
        }

        if (request && !request->cancelled) {
            // Execute request
            Response response = executeHTTPRequest(request->request);

            // Set promise result
            request->promise.set_value(response);

            // Call callback if provided
            if (request->callback) {
                try {
                    request->callback(response);
                } catch (const std::exception &e) {
                    Logger::error("NativeHttpClient: Callback error: " + std::string(e.what()));
                }
            }

            // Remove from active requests
            {
                std::lock_guard<std::mutex> lock(pendingRequestsMutex_);
                activeRequests_.erase(request->id);
            }

            request.release();  // Request is managed by activeRequests_ map
        }
    }

    Logger::info("NativeHttpClient: Worker thread stopped");
}