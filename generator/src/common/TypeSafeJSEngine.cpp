#include "common/TypeSafeJSEngine.h"
#include "common/Logger.h"
#include "model/Event.h"
#include "runtime/DataModelEngine.h"

// Include JavaScript engine headers conditionally
#ifdef HAVE_QUICKJS
#include <quickjs.h>
#endif

namespace SCXML {
namespace Common {

// DataValueConverter implementation
JSValue TypeSafeJSEngine::DataValueConverter::convertToJS(JSContext *ctx, const DataValue &value) {
#ifdef HAVE_QUICKJS
    // Implementation would depend on DataValue structure
    // This is a placeholder showing the type-safe approach
    switch (value.getType()) {
    case DataValue::Type::STRING:
        return JS_NewString(ctx, value.getString().c_str());
    case DataValue::Type::NUMBER:
        return JS_NewFloat64(ctx, value.getNumber());
    case DataValue::Type::BOOLEAN:
        return JS_NewBool(ctx, value.getBoolean());
    default:
        return JS_NULL;
    }
#else
    // Fallback implementation
    Logger::warning("JavaScript engine not available, returning null JSValue");
    return JSValue{};  // Placeholder null value
#endif
}

DataValue TypeSafeJSEngine::DataValueConverter::convertFromJS(JSContext *ctx, JSValue jsValue) {
#ifdef HAVE_QUICKJS
    if (JS_IsString(jsValue)) {
        const char *str = JS_ToCString(ctx, jsValue);
        DataValue result(std::string(str));
        JS_FreeCString(ctx, str);
        return result;
    } else if (JS_IsNumber(jsValue)) {
        double num;
        JS_ToFloat64(ctx, &num, jsValue);
        return DataValue(num);
    } else if (JS_IsBool(jsValue)) {
        return DataValue(JS_ToBool(ctx, jsValue));
    }
#endif

    // Default fallback
    return DataValue();  // Default constructed DataValue
}

// Static interrupt handler
int TypeSafeJSEngine::staticInterruptHandler(JSRuntime *runtime, void *opaque) {
    auto &handlerMap = getInterruptHandlerMap();
    auto it = handlerMap.find(runtime);

    if (it != handlerMap.end() && it->second) {
        return it->second->shouldInterrupt(runtime) ? 1 : 0;
    }

    return 0;  // Continue execution
}

// JSValueGuard implementation
JSValueGuard::~JSValueGuard() {
#ifdef HAVE_QUICKJS
    if (context_ && !JS_IsNull(value_) && !JS_IsUndefined(value_)) {
        JS_FreeValue(context_, value_);
    }
#endif
}

}  // namespace Common
}  // namespace SCXML