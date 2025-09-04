#ifndef TYPESAFEJSENGINE_H
#define TYPESAFEJSENGINE_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declarations to avoid circular dependencies
class DataValue;
class DataModelEngine;
struct JSContext;
struct JSValue;
struct JSRuntime;

namespace SCXML {
namespace Common {

/**
 * @brief Type-safe wrapper for JavaScript engine operations
 */
class TypeSafeJSEngine {
public:
    /**
     * @brief Type-safe callback for JavaScript functions
     */
    template <typename ReturnType, typename... Args> using JSCallback = std::function<ReturnType(Args...)>;

    /**
     * @brief Type-safe data converter interface
     */
    class IDataConverter {
    public:
        virtual ~IDataConverter() = default;
        virtual JSValue convertToJS(JSContext *ctx, const DataValue &value) = 0;
        virtual DataValue convertFromJS(JSContext *ctx, JSValue jsValue) = 0;
    };

    /**
     * @brief Type-safe JavaScript function wrapper
     */
    class JSFunctionWrapper {
    public:
        JSFunctionWrapper(const std::string &name, JSContext *context) : functionName_(name), context_(context) {}

        const std::string &getName() const {
            return functionName_;
        }

        JSContext *getContext() const {
            return context_;
        }

        template <typename T> void setUserData(std::shared_ptr<T> data) {
            userData_ = std::static_pointer_cast<void>(data);
            userDataType_ = typeid(T).name();
        }

        template <typename T> std::shared_ptr<T> getUserData() const {
            if (userDataType_ == typeid(T).name()) {
                return std::static_pointer_cast<T>(userData_);
            }
            return nullptr;
        }

    private:
        std::string functionName_;
        JSContext *context_;
        std::shared_ptr<void> userData_;
        std::string userDataType_;
    };

    /**
     * @brief Type-safe interrupt handler
     */
    class InterruptHandler {
    public:
        virtual ~InterruptHandler() = default;
        virtual bool shouldInterrupt(JSRuntime *runtime) = 0;
    };

    /**
     * @brief Type-safe data value converter
     */
    class DataValueConverter : public IDataConverter {
    public:
        DataValueConverter(std::shared_ptr<DataModelEngine> engine) : dataEngine_(engine) {}

        JSValue convertToJS(JSContext *ctx, const DataValue &value) override;
        DataValue convertFromJS(JSContext *ctx, JSValue jsValue) override;

    private:
        std::weak_ptr<DataModelEngine> dataEngine_;
    };

    /**
     * @brief Create type-safe function wrapper
     */
    static std::unique_ptr<JSFunctionWrapper> createFunctionWrapper(const std::string &name, JSContext *context) {
        return std::make_unique<JSFunctionWrapper>(name, context);
    }

    /**
     * @brief Create type-safe data converter
     */
    static std::unique_ptr<IDataConverter> createDataConverter(std::shared_ptr<DataModelEngine> engine) {
        return std::make_unique<DataValueConverter>(engine);
    }

    /**
     * @brief Register type-safe interrupt handler
     */
    static void registerInterruptHandler(JSRuntime *runtime, std::shared_ptr<InterruptHandler> handler) {
        auto &handlerMap = getInterruptHandlerMap();
        handlerMap[runtime] = handler;
    }

    /**
     * @brief Static interrupt handler for C API
     */
    static int staticInterruptHandler(JSRuntime *runtime, void *opaque);

private:
    using InterruptHandlerMap = std::unordered_map<JSRuntime *, std::shared_ptr<InterruptHandler>>;

    static InterruptHandlerMap &getInterruptHandlerMap() {
        static InterruptHandlerMap handlerMap;
        return handlerMap;
    }
};

/**
 * @brief RAII wrapper for JavaScript values
 */
class JSValueGuard {
public:
    JSValueGuard(JSContext *ctx, JSValue val) : context_(ctx), value_(val) {}

    ~JSValueGuard();

    JSValue get() const {
        return value_;
    }

    JSValue release() {
        JSValue val = value_;
        value_ = JSValue{};  // Null value
        return val;
    }

private:
    JSContext *context_;
    JSValue value_;
};

}  // namespace Common

}  // namespace SCXML

#endif  // TYPESAFEJSENGINE_H