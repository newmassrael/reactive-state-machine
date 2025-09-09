#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <quickjs.h>
#include <string>
#include <unordered_map>

namespace SCXML {

/**
 * Static class for tracking JSValue lifecycles
 */
class JSValueTracker {
private:
    static std::unordered_map<void *, std::string> tracked_values_;
    static std::mutex tracker_mutex_;

public:
    static void track(JSValue value, const std::string &name);
    static void untrack(JSValue value);
    static void clearAll(JSContext *context);
    static size_t getTrackedCount();
};

/**
 * RAII wrapper for JSAtom with automatic lifecycle management
 * Prevents atom leaks by ensuring JS_FreeAtom is always called
 */
class JSAtomWrapper {
private:
    JSContext *context_;
    JSAtom atom_;
    std::string debug_name_;

public:
    JSAtomWrapper(JSContext *ctx, JSAtom atom, const std::string &name = "")
        : context_(ctx), atom_(atom), debug_name_(name.empty() ? "atom" : name) {
        // JSAtom doesn't need tracking like JSValue
    }

    ~JSAtomWrapper() {
        free();
    }

    // Move constructor
    JSAtomWrapper(JSAtomWrapper &&other) noexcept
        : context_(other.context_), atom_(other.atom_), debug_name_(std::move(other.debug_name_)) {
        other.context_ = nullptr;
        other.atom_ = JS_ATOM_NULL;
    }

    // Move assignment
    JSAtomWrapper &operator=(JSAtomWrapper &&other) noexcept {
        if (this != &other) {
            free();
            context_ = other.context_;
            atom_ = other.atom_;
            debug_name_ = std::move(other.debug_name_);
            other.context_ = nullptr;
            other.atom_ = JS_ATOM_NULL;
        }
        return *this;
    }

    // Deleted copy operations
    JSAtomWrapper(const JSAtomWrapper &) = delete;
    JSAtomWrapper &operator=(const JSAtomWrapper &) = delete;

    // Access methods
    JSAtom get() const {
        return atom_;
    }

    operator JSAtom() const {
        return atom_;
    }

    // Check if valid
    bool isValid() const {
        return context_ && atom_ != JS_ATOM_NULL;
    }

    // Release ownership
    JSAtom release() {
        JSAtom a = atom_;
        context_ = nullptr;
        atom_ = JS_ATOM_NULL;
        return a;
    }

private:
    void free() {
        if (context_ && atom_ != JS_ATOM_NULL) {
            JS_FreeAtom(context_, atom_);
        }
        context_ = nullptr;
        atom_ = JS_ATOM_NULL;
    }
};

/**
 * RAII wrapper for JSValue with automatic lifecycle management
 * Prevents memory leaks by ensuring JS_FreeValue is always called
 */
class JSValueWrapper {
private:
    JSContext *context_;
    JSValue value_;
    std::string debug_name_;
    static thread_local int creation_counter_;

public:
    // Constructor
    JSValueWrapper(JSContext *ctx, JSValue val, const std::string &name = "")
        : context_(ctx), value_(val),
          debug_name_(name.empty() ? ("jsval_" + std::to_string(++creation_counter_)) : name) {
        if (context_ && !JS_IsUndefined(value_)) {
            JSValueTracker::track(value_, debug_name_);

#ifdef DEBUG_JSVALUE_LIFECYCLE
            std::cout << "[JSValue] Created: " << debug_name_ << std::endl;
#endif
        }
    }

    // Destructor - automatically frees the JSValue
    ~JSValueWrapper() {
        if (context_ && !JS_IsUndefined(value_)) {
            JSValueTracker::untrack(value_);

#ifdef DEBUG_JSVALUE_LIFECYCLE
            std::cout << "[JSValue] Freed: " << debug_name_ << std::endl;
#endif

            JS_FreeValue(context_, value_);
        }
    }

    // Move constructor
    JSValueWrapper(JSValueWrapper &&other) noexcept
        : context_(other.context_), value_(other.value_), debug_name_(std::move(other.debug_name_)) {
        other.context_ = nullptr;
        other.value_ = JS_UNDEFINED;

#ifdef DEBUG_JSVALUE_LIFECYCLE
        std::cout << "[JSValue] Moved: " << debug_name_ << std::endl;
#endif
    }

    // Move assignment
    JSValueWrapper &operator=(JSValueWrapper &&other) noexcept {
        if (this != &other) {
            if (context_ && !JS_IsUndefined(value_)) {
                JSValueTracker::untrack(value_);
                JS_FreeValue(context_, value_);
            }

            context_ = other.context_;
            value_ = other.value_;
            debug_name_ = std::move(other.debug_name_);

            other.context_ = nullptr;
            other.value_ = JS_UNDEFINED;
        }
        return *this;
    }

    // Delete copy constructor and assignment
    JSValueWrapper(const JSValueWrapper &) = delete;
    JSValueWrapper &operator=(const JSValueWrapper &) = delete;

    // Access methods
    JSValue get() const {
        return value_;
    }

    operator JSValue() const {
        return value_;
    }

    // Check if valid
    bool isValid() const {
        return context_ && !JS_IsUndefined(value_);
    }

    bool isNull() const {
        return JS_IsNull(value_);
    }

    bool isUndefined() const {
        return JS_IsUndefined(value_);
    }

    // Release ownership (caller takes responsibility for freeing)
    JSValue release() {
        JSValue val = value_;
        if (context_ && !JS_IsUndefined(value_)) {
            JSValueTracker::untrack(value_);

#ifdef DEBUG_JSVALUE_LIFECYCLE
            std::cout << "[JSValue] Released: " << debug_name_ << std::endl;
#endif
        }

        context_ = nullptr;
        value_ = JS_UNDEFINED;
        return val;
    }

    // Get debug name
    const std::string &getName() const {
        return debug_name_;
    }
};

// Convenience macros for common JSValue operations
#define JS_WRAP(ctx, val, name) JSValueWrapper(ctx, val, name)
#define JS_WRAP_AUTO(ctx, val) JSValueWrapper(ctx, val)

// Factory functions for common JSValue creation patterns
namespace JSValueFactory {
inline JSValueWrapper NewString(JSContext *ctx, const char *str, const std::string &name = "") {
    return JSValueWrapper(ctx, JS_NewString(ctx, str), name.empty() ? "string" : name);
}

inline JSValueWrapper NewNumber(JSContext *ctx, double num, const std::string &name = "") {
    return JSValueWrapper(ctx, JS_NewFloat64(ctx, num), name.empty() ? "number" : name);
}

inline JSValueWrapper NewBool(JSContext *ctx, bool val, const std::string &name = "") {
    return JSValueWrapper(ctx, JS_NewBool(ctx, val), name.empty() ? "boolean" : name);
}

inline JSValueWrapper NewObject(JSContext *ctx, const std::string &name = "") {
    return JSValueWrapper(ctx, JS_NewObject(ctx), name.empty() ? "object" : name);
}

inline JSValueWrapper NewArray(JSContext *ctx, const std::string &name = "") {
    return JSValueWrapper(ctx, JS_NewArray(ctx), name.empty() ? "array" : name);
}

inline JSValueWrapper GetGlobalObject(JSContext *ctx, const std::string &name = "") {
    return JSValueWrapper(ctx, JS_GetGlobalObject(ctx), name.empty() ? "global" : name);
}

inline JSValueWrapper GetProperty(JSContext *ctx, JSValue obj, const char *prop, const std::string &name = "") {
    return JSValueWrapper(ctx, JS_GetPropertyStr(ctx, obj, prop), name.empty() ? ("prop_" + std::string(prop)) : name);
}
}  // namespace JSValueFactory

/**
 * RAII wrapper for JSPropertyEnum arrays with automatic cleanup
 * Manages property enumeration lifecycle including all JSAtom objects
 */
class JSPropertyEnumWrapper {
private:
    JSContext *context_;
    JSPropertyEnum *tab_;
    uint32_t len_;
    std::string debug_name_;

public:
    JSPropertyEnumWrapper(JSContext *ctx, JSPropertyEnum *tab, uint32_t len, const std::string &name = "")
        : context_(ctx), tab_(tab), len_(len), debug_name_(name.empty() ? "prop_enum" : name) {}

    ~JSPropertyEnumWrapper() {
        free();
    }

    // Move constructor
    JSPropertyEnumWrapper(JSPropertyEnumWrapper &&other) noexcept
        : context_(other.context_), tab_(other.tab_), len_(other.len_), debug_name_(std::move(other.debug_name_)) {
        other.context_ = nullptr;
        other.tab_ = nullptr;
        other.len_ = 0;
    }

    // Move assignment
    JSPropertyEnumWrapper &operator=(JSPropertyEnumWrapper &&other) noexcept {
        if (this != &other) {
            free();
            context_ = other.context_;
            tab_ = other.tab_;
            len_ = other.len_;
            debug_name_ = std::move(other.debug_name_);
            other.context_ = nullptr;
            other.tab_ = nullptr;
            other.len_ = 0;
        }
        return *this;
    }

    // Deleted copy operations
    JSPropertyEnumWrapper(const JSPropertyEnumWrapper &) = delete;
    JSPropertyEnumWrapper &operator=(const JSPropertyEnumWrapper &) = delete;

    // Access methods
    JSPropertyEnum *get() const {
        return tab_;
    }

    uint32_t length() const {
        return len_;
    }

    JSPropertyEnum &operator[](uint32_t index) {
        return tab_[index];
    }

    const JSPropertyEnum &operator[](uint32_t index) const {
        return tab_[index];
    }

    // Check if valid
    bool isValid() const {
        return context_ && tab_ && len_ > 0;
    }

    // Release ownership
    JSPropertyEnum *release() {
        JSPropertyEnum *t = tab_;
        context_ = nullptr;
        tab_ = nullptr;
        len_ = 0;
        return t;
    }

private:
    void free() {
        if (context_ && tab_) {
            // Free all JSAtom objects in the enumeration
            for (uint32_t i = 0; i < len_; i++) {
                JS_FreeAtom(context_, tab_[i].atom);
            }
            // Free the enumeration array itself
            js_free(context_, tab_);
        }
        context_ = nullptr;
        tab_ = nullptr;
        len_ = 0;
    }
};

}  // namespace SCXML