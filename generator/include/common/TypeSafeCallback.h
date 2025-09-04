#ifndef TYPESAFECALLBACK_H
#define TYPESAFECALLBACK_H

#include <functional>
#include <memory>

namespace SCXML {
namespace Common {

// Type-safe callback wrapper for libcurl and other C APIs
template <typename CallbackType> class TypeSafeCallback {
public:
    using CallbackFunction = std::function<CallbackType>;

    explicit TypeSafeCallback(CallbackFunction callback) : callback_(std::move(callback)) {}

    // Get type-safe callback data pointer
    void *getCallbackData() {
        return static_cast<void *>(this);
    }

    // Static wrapper function for C APIs
    template <typename... Args> static auto wrapperFunction(Args... args, void *userData) {
        auto *self = static_cast<TypeSafeCallback *>(userData);
        return self->callback_(args...);
    }

private:
    CallbackFunction callback_;
};

// Specialized type for libcurl write callback
using CurlWriteCallback = TypeSafeCallback<size_t(void *, size_t, size_t)>;

// Specialized type for JavaScript callback
using JSCallback = TypeSafeCallback<void(const char *)>;

}  // namespace Common
}  // namespace SCXML

// Compatibility support
template <typename CallbackType> using TypeSafeCallback = SCXML::Common::TypeSafeCallback<CallbackType>;
using CurlWriteCallback = SCXML::Common::CurlWriteCallback;
using JSCallback = SCXML::Common::JSCallback;

#endif  // TYPESAFECALLBACK_H