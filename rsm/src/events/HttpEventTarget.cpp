#include "events/HttpEventTarget.h"
#include "common/Logger.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <thread>

namespace RSM {

HttpEventTarget::HttpEventTarget(const std::string &targetUri, std::chrono::milliseconds timeoutMs, int maxRetries)
    : targetUri_(targetUri), port_(80), timeoutMs_(timeoutMs), maxRetries_(maxRetries), sslVerification_(true) {
    if (!parseTargetUri()) {
        LOG_ERROR("HttpEventTarget: Invalid target URI: {}", targetUri_);
    }

    LOG_DEBUG("HttpEventTarget: Created for URI '{}' with timeout {}ms, {} retries", targetUri_, timeoutMs_.count(),
              maxRetries_);
}

std::future<SendResult> HttpEventTarget::send(const EventDescriptor &event) {
    return std::async(std::launch::async, [this, event]() -> SendResult {
        try {
            LOG_DEBUG("HttpEventTarget: Sending event '{}' to '{}'", event.eventName, targetUri_);

            // Create HTTP client
            auto client = createHttpClient();
            if (!client) {
                return SendResult::error("Failed to create HTTP client for " + targetUri_,
                                         SendResult::ErrorType::NETWORK_ERROR);
            }

            // Create JSON payload
            std::string payload = createJsonPayload(event);
            LOG_DEBUG("HttpEventTarget: JSON payload: {}", payload);

            // Perform request with retry
            auto result = performRequestWithRetry(*client, path_, payload);

            // Convert response to SendResult
            return convertHttpResponse(result, event);

        } catch (const std::exception &e) {
            LOG_ERROR("HttpEventTarget: Exception during send: {}", e.what());
            return SendResult::error("HTTP send exception: " + std::string(e.what()),
                                     SendResult::ErrorType::INTERNAL_ERROR);
        }
    });
}

std::string HttpEventTarget::getTargetType() const {
    return scheme_;
}

bool HttpEventTarget::canHandle(const std::string &targetUri) const {
    // Extract scheme from target URI
    std::string targetScheme;
    size_t schemeEnd = targetUri.find("://");
    if (schemeEnd != std::string::npos) {
        targetScheme = targetUri.substr(0, schemeEnd);
        // Convert to lowercase for comparison
        std::transform(targetScheme.begin(), targetScheme.end(), targetScheme.begin(), ::tolower);
    }

    // Only handle URLs that match our specific scheme
    return targetScheme == scheme_;
}

std::vector<std::string> HttpEventTarget::validate() const {
    std::vector<std::string> errors;

    if (targetUri_.empty()) {
        errors.push_back("Target URI cannot be empty");
    }

    if (scheme_ != "http" && scheme_ != "https") {
        errors.push_back("Only HTTP and HTTPS schemes are supported");
    }

    if (host_.empty()) {
        errors.push_back("Host cannot be empty");
    }

    if (port_ <= 0 || port_ > 65535) {
        errors.push_back("Port must be between 1 and 65535");
    }

    if (timeoutMs_.count() <= 0) {
        errors.push_back("Timeout must be positive");
    }

    if (maxRetries_ < 0) {
        errors.push_back("Max retries cannot be negative");
    }

    return errors;
}

std::string HttpEventTarget::getDebugInfo() const {
    std::ostringstream info;
    info << "HttpEventTarget{"
         << "uri='" << targetUri_ << "'"
         << ", scheme='" << scheme_ << "'"
         << ", host='" << host_ << "'"
         << ", port=" << port_ << ", path='" << path_ << "'"
         << ", timeout=" << timeoutMs_.count() << "ms"
         << ", retries=" << maxRetries_ << ", ssl_verify=" << (sslVerification_ ? "true" : "false") << "}";
    return info.str();
}

void HttpEventTarget::setCustomHeaders(const std::map<std::string, std::string> &headers) {
    customHeaders_ = headers;
    LOG_DEBUG("HttpEventTarget: Set {} custom headers", headers.size());
}

void HttpEventTarget::setTimeout(std::chrono::milliseconds timeoutMs) {
    timeoutMs_ = timeoutMs;
    LOG_DEBUG("HttpEventTarget: Set timeout to {}ms", timeoutMs_.count());
}

void HttpEventTarget::setMaxRetries(int maxRetries) {
    maxRetries_ = maxRetries;
    LOG_DEBUG("HttpEventTarget: Set max retries to {}", maxRetries_);
}

void HttpEventTarget::setSSLVerification(bool verify) {
    sslVerification_ = verify;
    LOG_DEBUG("HttpEventTarget: SSL verification {}", verify ? "enabled" : "disabled");
}

bool HttpEventTarget::parseTargetUri() {
    // Parse URI using regex: scheme://host:port/path
    std::regex uriPattern(R"(^(https?)://([^:/\s]+)(?::(\d+))?(/.*)?$)", std::regex_constants::icase);
    std::smatch match;

    if (!std::regex_match(targetUri_, match, uriPattern)) {
        return false;
    }

    scheme_ = match[1].str();
    std::transform(scheme_.begin(), scheme_.end(), scheme_.begin(), ::tolower);

    host_ = match[2].str();

    // Parse port
    if (match[3].matched) {
        try {
            port_ = std::stoi(match[3].str());
        } catch (const std::exception &) {
            return false;
        }
    } else {
        port_ = (scheme_ == "https") ? 443 : 80;
    }

    // Parse path
    path_ = match[4].matched ? match[4].str() : "/";

    LOG_DEBUG("HttpEventTarget: Parsed URI - scheme='{}', host='{}', port={}, path='{}'", scheme_, host_, port_, path_);

    return true;
}

std::unique_ptr<httplib::Client> HttpEventTarget::createHttpClient() const {
    try {
        std::string baseUrl = scheme_ + "://" + host_;
        if ((scheme_ == "http" && port_ != 80) || (scheme_ == "https" && port_ != 443)) {
            baseUrl += ":" + std::to_string(port_);
        }

        auto client = std::make_unique<httplib::Client>(baseUrl);

        // Set timeout
        auto timeoutSec = std::chrono::duration_cast<std::chrono::seconds>(timeoutMs_);
        client->set_connection_timeout(timeoutSec.count());
        client->set_read_timeout(timeoutSec.count());
        client->set_write_timeout(timeoutSec.count());

        // SSL settings for HTTPS
        if (scheme_ == "https") {
            client->enable_server_certificate_verification(sslVerification_);
        }

        // Set custom headers
        for (const auto &header : customHeaders_) {
            client->set_default_headers({{header.first, header.second}});
        }

        LOG_DEBUG("HttpEventTarget: Created HTTP client for '{}'", baseUrl);
        return client;

    } catch (const std::exception &e) {
        LOG_ERROR("HttpEventTarget: Failed to create HTTP client: {}", e.what());
        return nullptr;
    }
}

std::string HttpEventTarget::createJsonPayload(const EventDescriptor &event) const {
    std::ostringstream json;
    json << "{"
         << "\"event\":\"" << escapeJsonString(event.eventName) << "\""
         << ",\"source\":\"scxml\"";

    if (!event.sendId.empty()) {
        json << ",\"sendid\":\"" << escapeJsonString(event.sendId) << "\"";
    }

    if (!event.data.empty()) {
        // Try to parse as JSON, otherwise treat as string
        if (event.data.front() == '{' || event.data.front() == '[' || event.data.front() == '"' ||
            event.data == "true" || event.data == "false" || event.data == "null" ||
            std::regex_match(event.data, std::regex(R"(^-?\d+(\.\d+)?([eE][+-]?\d+)?$)"))) {
            // Looks like JSON, include as-is
            json << ",\"data\":" << event.data;
        } else {
            // Treat as string literal
            json << ",\"data\":\"" << escapeJsonString(event.data) << "\"";
        }
    }

    if (!event.target.empty() && event.target != targetUri_) {
        json << ",\"target\":\"" << escapeJsonString(event.target) << "\"";
    }

    json << "}";
    return json.str();
}

httplib::Result HttpEventTarget::performRequestWithRetry(httplib::Client &client, const std::string &path,
                                                         const std::string &payload) const {
    httplib::Result result;
    int attempts = 0;

    while (attempts <= maxRetries_) {
        attempts++;

        LOG_DEBUG("HttpEventTarget: HTTP POST attempt {} to '{}' with payload: {}", attempts, path, payload);

        // Perform POST request
        LOG_DEBUG("HttpEventTarget: Executing client.Post('{}', payload, 'application/json')", path);
        result = client.Post(path, payload, "application/json");

        if (result) {
            LOG_DEBUG("HttpEventTarget: HTTP POST completed, status: {}, response body: {}", result->status,
                      result->body);
        } else {
            LOG_ERROR("HttpEventTarget: HTTP POST failed, error: {}", static_cast<int>(result.error()));
        }

        if (result && result->status >= 200 && result->status < 300) {
            // Success
            LOG_DEBUG("HttpEventTarget: HTTP POST successful, status {}", result->status);
            break;
        }

        if (attempts <= maxRetries_) {
            // Wait before retry (exponential backoff)
            auto waitTime = std::chrono::milliseconds(100 * attempts);
            LOG_DEBUG("HttpEventTarget: Retrying in {}ms (attempt {} of {})", waitTime.count(), attempts,
                      maxRetries_ + 1);
            std::this_thread::sleep_for(waitTime);
        }
    }

    return result;
}

SendResult HttpEventTarget::convertHttpResponse(const httplib::Result &result, const EventDescriptor &event) const {
    if (!result) {
        std::string errorMsg = "HTTP request failed";
        SendResult::ErrorType errorType = SendResult::ErrorType::NETWORK_ERROR;

        switch (result.error()) {
        case httplib::Error::Connection:
            errorMsg = "Connection failed";
            break;
        case httplib::Error::BindIPAddress:
            errorMsg = "Failed to bind IP address";
            break;
        case httplib::Error::Read:
            errorMsg = "Read error";
            break;
        case httplib::Error::Write:
            errorMsg = "Write error";
            break;
        // Note: ExceedsMaxRedirects may not be available in all httplib versions
        case httplib::Error::Canceled:
            errorMsg = "Request canceled";
            break;
        case httplib::Error::SSLConnection:
            errorMsg = "SSL connection failed";
            break;
        case httplib::Error::SSLLoadingCerts:
            errorMsg = "SSL certificate loading failed";
            break;
        case httplib::Error::SSLServerVerification:
            errorMsg = "SSL server verification failed";
            break;
        case httplib::Error::UnsupportedMultipartBoundaryChars:
            errorMsg = "Unsupported multipart boundary characters";
            break;
        default:
            errorMsg = "Unknown HTTP error";
            errorType = SendResult::ErrorType::INTERNAL_ERROR;
            break;
        }

        LOG_ERROR("HttpEventTarget: {}", errorMsg);
        return SendResult::error(errorMsg, errorType);
    }

    // Check HTTP status code
    if (result->status >= 200 && result->status < 300) {
        LOG_INFO("HttpEventTarget: Event '{}' sent successfully to '{}', status {}", event.eventName, targetUri_,
                 result->status);
        return SendResult::success(event.sendId);
    } else {
        std::string errorMsg = "HTTP " + std::to_string(result->status) + ": " + result->reason;
        if (!result->body.empty()) {
            errorMsg += " - " + result->body;
        }

        SendResult::ErrorType errorType = SendResult::ErrorType::TARGET_NOT_FOUND;
        if (result->status >= 500) {
            errorType = SendResult::ErrorType::NETWORK_ERROR;
        }

        LOG_ERROR("HttpEventTarget: HTTP error for event '{}': {}", event.eventName, errorMsg);
        return SendResult::error(errorMsg, errorType);
    }
}

std::string HttpEventTarget::escapeJsonString(const std::string &input) const {
    std::ostringstream escaped;
    for (char c : input) {
        switch (c) {
        case '"':
            escaped << "\\\"";
            break;
        case '\\':
            escaped << "\\\\";
            break;
        case '\b':
            escaped << "\\b";
            break;
        case '\f':
            escaped << "\\f";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        default:
            if (c < 0x20) {
                escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
            } else {
                escaped << c;
            }
            break;
        }
    }
    return escaped.str();
}

}  // namespace RSM