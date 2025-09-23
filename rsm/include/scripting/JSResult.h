#pragma once

#include "SCXMLTypes.h"
#include <string>
#include <variant>

namespace RSM {

// ScriptValue is now defined in SCXMLTypes.h

/**
 * @brief JavaScript execution result
 */
struct JSResult {
private:
    // PRIVATE: 레거시 API 완전 차단 - 직접 접근 불가능
    bool success_internal = false;
    ScriptValue value_internal = ScriptUndefined{};
    std::string errorMessage_internal;

public:
    // ONLY ALLOW: 통합 API를 통한 접근만 허용
    // 생성자는 static factory methods를 통해서만 사용
    static JSResult createSuccess(const ScriptValue &val = ScriptUndefined{}) {
        JSResult result;
        result.success_internal = true;
        result.value_internal = val;
        return result;
    }

    static JSResult createError(const std::string &error) {
        JSResult result;
        result.success_internal = false;
        result.errorMessage_internal = error;
        return result;
    }

    // LEGACY API COMPLETELY BLOCKED: 레거시 필드 완전 삭제
    // success, value, errorMessage 필드는 더 이상 존재하지 않음

    // ONLY THESE METHODS ALLOWED: 통합 API만 사용 가능
    bool isSuccess() const {
        return success_internal;
    }

    bool isError() const {
        return !success_internal;
    }

    template <typename T> T getValue() const {
        if (std::holds_alternative<T>(value_internal)) {
            return std::get<T>(value_internal);
        }
        return T{};
    }

    /**
     * @brief Get value as array (returns nullptr if not an array)
     */
    std::shared_ptr<ScriptArray> getArray() const {
        if (std::holds_alternative<std::shared_ptr<ScriptArray>>(value_internal)) {
            return std::get<std::shared_ptr<ScriptArray>>(value_internal);
        }
        return nullptr;
    }

    /**
     * @brief Get value as object (returns nullptr if not an object)
     */
    std::shared_ptr<ScriptObject> getObject() const {
        if (std::holds_alternative<std::shared_ptr<ScriptObject>>(value_internal)) {
            return std::get<std::shared_ptr<ScriptObject>>(value_internal);
        }
        return nullptr;
    }

    /**
     * @brief Get array element by index
     */
    ScriptValue getArrayElement(size_t index) const {
        auto arr = getArray();
        if (arr && index < arr->elements.size()) {
            return arr->elements[index];
        }
        return ScriptUndefined{};
    }

    /**
     * @brief Get object property by key
     */
    ScriptValue getObjectProperty(const std::string &key) const {
        auto obj = getObject();
        if (obj) {
            auto it = obj->properties.find(key);
            if (it != obj->properties.end()) {
                return it->second;
            }
        }
        return ScriptUndefined{};
    }

    /**
     * @brief Check if value is an array
     */
    bool isArray() const {
        return std::holds_alternative<std::shared_ptr<ScriptArray>>(value_internal);
    }

    /**
     * @brief Check if value is an object
     */
    bool isObject() const {
        return std::holds_alternative<std::shared_ptr<ScriptObject>>(value_internal);
    }

    std::string getValueAsString() const {
        return std::visit(
            [](const auto &v) -> std::string {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    return v;
                } else if constexpr (std::is_same_v<T, bool>) {
                    return v ? "true" : "false";
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    return std::to_string(v);
                } else if constexpr (std::is_same_v<T, std::shared_ptr<ScriptArray>>) {
                    std::string result = "[";
                    for (size_t i = 0; i < v->elements.size(); ++i) {
                        if (i > 0) {
                            result += ",";
                        }
                        result += std::visit(
                            [](const auto &elem) -> std::string {
                                using ElemT = std::decay_t<decltype(elem)>;
                                if constexpr (std::is_same_v<ElemT, std::string>) {
                                    return "\"" + elem + "\"";
                                } else if constexpr (std::is_same_v<ElemT, bool>) {
                                    return elem ? "true" : "false";
                                } else if constexpr (std::is_same_v<ElemT, int64_t>) {
                                    return std::to_string(elem);
                                } else if constexpr (std::is_same_v<ElemT, double>) {
                                    return std::to_string(elem);
                                } else {
                                    return "null";
                                }
                            },
                            v->elements[i]);
                    }
                    result += "]";
                    return result;
                } else if constexpr (std::is_same_v<T, std::shared_ptr<ScriptObject>>) {
                    return "[object Object]";
                } else {
                    return "undefined";
                }
            },
            value_internal);
    }

    // Internal value accessor for friend classes only
    const ScriptValue &getInternalValue() const {
        return value_internal;
    }

    // FRIEND ACCESS: 통합 API에서만 내부 필드 접근 가능
    friend class JSEngine;
};

}  // namespace RSM
