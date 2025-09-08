#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/JavaScriptEvaluatorAdapter.h"

#ifdef HAS_QUICKJS
#include "runtime/QuickJSEngine.h"
#endif

#include "common/Logger.h"
#include <functional>
#include <mutex>

namespace SCXML {

std::unique_ptr<IECMAScriptEngine> ECMAScriptEngineFactory::create(EngineType type) {
    SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - Creating engine type: " + std::to_string(static_cast<int>(type)));

    switch (type) {
    case EngineType::AUTO:
#ifdef HAS_QUICKJS
        SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - AUTO: Using QuickJSEngine");
        return std::make_unique<QuickJSEngine>();
#else
        SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - AUTO: Using JavaScriptEvaluatorAdapter (QuickJS not available)");
        return std::make_unique<JavaScriptEvaluatorAdapter>();
#endif

    case EngineType::JAVASCRIPT_EVALUATOR:
        SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - Creating JavaScriptEvaluatorAdapter");
        return std::make_unique<JavaScriptEvaluatorAdapter>();

    case EngineType::QUICKJS:
    case EngineType::FULL_JAVASCRIPT:
#ifdef HAS_QUICKJS
        SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - Creating QuickJSEngine");
        return std::make_unique<QuickJSEngine>();
#else
        SCXML::Common::Logger::warning("ECMAScriptEngineFactory::create - QuickJS not available, using fallback");
        return std::make_unique<JavaScriptEvaluatorAdapter>();
#endif

    default:
        SCXML::Common::Logger::error("ECMAScriptEngineFactory::create - Unknown engine type, using fallback");
        return std::make_unique<JavaScriptEvaluatorAdapter>();
    }
}

bool ECMAScriptEngineFactory::isEngineAvailable(EngineType type) {
    switch (type) {
    case EngineType::JAVASCRIPT_EVALUATOR:
        return true;  // Always available

    case EngineType::AUTO:
        return true;  // AUTO always available (falls back if needed)

    case EngineType::QUICKJS:
    case EngineType::FULL_JAVASCRIPT:
#ifdef HAS_QUICKJS
        return true;
#else
        return false;
#endif

    default:
        return false;
    }
}

ECMAScriptEngineFactory::EngineType ECMAScriptEngineFactory::getDefaultEngineType() {
    return EngineType::AUTO;
}

std::string ECMAScriptEngineFactory::getEngineTypeName(EngineType type) {
    switch (type) {
    case EngineType::AUTO:
        return "AUTO";
    case EngineType::JAVASCRIPT_EVALUATOR:
        return "JavaScriptEvaluator";
    case EngineType::QUICKJS:
        return "QuickJS";
    case EngineType::FULL_JAVASCRIPT:
        return "FullJavaScript";
    default:
        return "Unknown";
    }
}

}  // namespace SCXML