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
    bool success = false;
    ScriptValue value = std::monostate{};
    std::string errorMessage;

    static JSResult createSuccess(const ScriptValue &val = std::monostate{}) {
        JSResult result;
        result.success = true;
        result.value = val;
        return result;
    }

    static JSResult createError(const std::string &error) {
        JSResult result;
        result.success = false;
        result.errorMessage = error;
        return result;
    }

    bool isSuccess() const {
        return success;
    }

    bool isError() const {
        return !success;
    }

    template <typename T> T getValue() const {
        if (std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }
        return T{};
    }

    /**
     * @brief Get value as array (returns nullptr if not an array)
     */
    std::shared_ptr<ScriptArray> getArray() const {
        if (std::holds_alternative<std::shared_ptr<ScriptArray>>(value)) {
            return std::get<std::shared_ptr<ScriptArray>>(value);
        }
        return nullptr;
    }

    /**
     * @brief Get value as object (returns nullptr if not an object)
     */
    std::shared_ptr<ScriptObject> getObject() const {
        if (std::holds_alternative<std::shared_ptr<ScriptObject>>(value)) {
            return std::get<std::shared_ptr<ScriptObject>>(value);
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
        return std::monostate{};
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
        return std::monostate{};
    }

    /**
     * @brief Check if value is an array
     */
    bool isArray() const {
        return std::holds_alternative<std::shared_ptr<ScriptArray>>(value);
    }

    /**
     * @brief Check if value is an object
     */
    bool isObject() const {
        return std::holds_alternative<std::shared_ptr<ScriptObject>>(value);
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
            value);
    }
};

}  // namespace RSM
