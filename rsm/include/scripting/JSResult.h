#pragma once

#include <string>
#include <variant>

/**
 * @brief JavaScript value type for cross-session data exchange
 */
using ScriptValue = std::variant<
    std::monostate,  // undefined/null
    bool,            // boolean
    int64_t,         // integer
    double,          // number
    std::string      // string
>;

/**
 * @brief JavaScript execution result
 */
struct JSResult {
    bool success = false;
    ScriptValue value = std::monostate{};
    std::string errorMessage;

    static JSResult createSuccess(const ScriptValue& val = std::monostate{}) {
        JSResult result;
        result.success = true;
        result.value = val;
        return result;
    }

    static JSResult createError(const std::string& error) {
        JSResult result;
        result.success = false;
        result.errorMessage = error;
        return result;
    }

    bool isSuccess() const { return success; }
    bool isError() const { return !success; }

    template<typename T>
    T getValue() const {
        if (std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }
        return T{};
    }

    std::string getValueAsString() const {
        return std::visit([](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return v;
            } else if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::to_string(v);
            } else {
                return "undefined";
            }
        }, value);
    }
};

