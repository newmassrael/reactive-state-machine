#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/JavaScriptEvaluatorAdapter.h"

#include "common/Logger.h"
#include <functional>
#include <mutex>

namespace SCXML {

std::unique_ptr<IECMAScriptEngine> ECMAScriptEngineFactory::create(EngineType type) {
    SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - Creating engine type: " + std::to_string(static_cast<int>(type)));

    switch (type) {
    case EngineType::AUTO:
        SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - AUTO: Using JavaScriptEvaluatorAdapter");
        return std::make_unique<JavaScriptEvaluatorAdapter>();

    case EngineType::JAVASCRIPT_EVALUATOR:
        SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - Creating JavaScriptEvaluatorAdapter");
        return std::make_unique<JavaScriptEvaluatorAdapter>();

    case EngineType::FULL_JAVASCRIPT:
        SCXML::Common::Logger::warning(
            "ECMAScriptEngineFactory::create - FullJavaScriptEngine removed, using JavaScriptEvaluatorAdapter");
        return std::make_unique<JavaScriptEvaluatorAdapter>();

    default:
        SCXML::Common::Logger::error("ECMAScriptEngineFactory::create - Unknown engine type, using JavaScriptEvaluator");
        return std::make_unique<JavaScriptEvaluatorAdapter>();
    }
}

bool ECMAScriptEngineFactory::isEngineAvailable(EngineType type) {
    switch (type) {
    case EngineType::JAVASCRIPT_EVALUATOR:
        return true;  // Always available

    case EngineType::AUTO:
        return true;  // AUTO always available (falls back to JavaScriptEvaluator)

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

    default:
        return "Unknown";
    }
}

}  // namespace SCXML