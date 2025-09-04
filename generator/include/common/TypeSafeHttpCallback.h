#ifndef TYPESAFEHTTPCALLBACK_H
#define TYPESAFEHTTPCALLBACK_H

#include <functional>
#include <memory>
#include <string>

namespace SCXML {
namespace Common {

/**
 * @brief Type-safe HTTP callback system
 */
class TypeSafeHttpCallback {
public:
    /**
     * @brief HTTP response data structure
     */
    struct HttpResponseData {
        std::string content;
        size_t totalSize;
        int statusCode;
        std::string contentType;

        HttpResponseData() : totalSize(0), statusCode(0) {}
    };

    /**
     * @brief Type-safe write callback for HTTP responses
     */
    class WriteCallback {
    public:
        using CallbackFunction = std::function<size_t(const char *, size_t, size_t, HttpResponseData *)>;

        WriteCallback(CallbackFunction callback) : callback_(std::move(callback)) {}

        size_t operator()(const char *ptr, size_t size, size_t nmemb, HttpResponseData *data) {
            return callback_(ptr, size, nmemb, data);
        }

        // Static wrapper for C APIs like libcurl
        static size_t staticCallback(void *contents, size_t size, size_t nmemb, void *userData) {
            auto *callbackData = static_cast<CallbackData *>(userData);
            return callbackData->callback(static_cast<char *>(contents), size, nmemb, callbackData->responseData);
        }

        struct CallbackData {
            WriteCallback callback;
            HttpResponseData *responseData;

            CallbackData(WriteCallback cb, HttpResponseData *data) : callback(std::move(cb)), responseData(data) {}
        };

    private:
        CallbackFunction callback_;
    };

    /**
     * @brief Type-safe header callback
     */
    class HeaderCallback {
    public:
        using CallbackFunction = std::function<size_t(const char *, size_t, size_t, HttpResponseData *)>;

        HeaderCallback(CallbackFunction callback) : callback_(std::move(callback)) {}

        size_t operator()(const char *buffer, size_t size, size_t nitems, HttpResponseData *data) {
            return callback_(buffer, size, nitems, data);
        }

        // Static wrapper for C APIs
        static size_t staticCallback(char *buffer, size_t size, size_t nitems, void *userData) {
            auto *callbackData = static_cast<CallbackData *>(userData);
            return callbackData->callback(buffer, size, nitems, callbackData->responseData);
        }

        struct CallbackData {
            HeaderCallback callback;
            HttpResponseData *responseData;

            CallbackData(HeaderCallback cb, HttpResponseData *data) : callback(std::move(cb)), responseData(data) {}
        };

    private:
        CallbackFunction callback_;
    };

    /**
     * @brief Create default write callback
     */
    static WriteCallback createDefaultWriteCallback() {
        return WriteCallback([](const char *ptr, size_t size, size_t nmemb, HttpResponseData *data) -> size_t {
            size_t totalSize = size * nmemb;
            data->content.append(ptr, totalSize);
            data->totalSize += totalSize;
            return totalSize;
        });
    }

    /**
     * @brief Create default header callback
     */
    static HeaderCallback createDefaultHeaderCallback() {
        return HeaderCallback([](const char *buffer, size_t size, size_t nitems, HttpResponseData *data) -> size_t {
            size_t totalSize = size * nitems;
            std::string header(buffer, totalSize);

            // Parse common headers
            if (header.find("Content-Type:") == 0) {
                size_t colonPos = header.find(':');
                if (colonPos != std::string::npos) {
                    data->contentType = header.substr(colonPos + 1);
                    // Trim whitespace
                    data->contentType.erase(0, data->contentType.find_first_not_of(" \t"));
                    data->contentType.erase(data->contentType.find_last_not_of(" \t\r\n") + 1);
                }
            }

            return totalSize;
        });
    }

    /**
     * @brief Create callback data wrapper
     */
    template <typename CallbackType>
    static std::unique_ptr<typename CallbackType::CallbackData> createCallbackData(CallbackType callback,
                                                                                   HttpResponseData *responseData) {
        return std::make_unique<typename CallbackType::CallbackData>(std::move(callback), responseData);
    }
};

/**
 * @brief RAII wrapper for HTTP callback data
 */
class HttpCallbackGuard {
public:
    template <typename CallbackType>
    HttpCallbackGuard(std::unique_ptr<typename CallbackType::CallbackData> data) : data_(std::move(data)) {}

    ~HttpCallbackGuard() = default;

    template <typename CallbackType> typename CallbackType::CallbackData *get() const {
        return static_cast<typename CallbackType::CallbackData *>(data_.get());
    }

private:
    std::unique_ptr<void, std::function<void(void *)>> data_;
};

}  // namespace Common

}  // namespace SCXML

#endif  // TYPESAFEHTTPCALLBACK_H