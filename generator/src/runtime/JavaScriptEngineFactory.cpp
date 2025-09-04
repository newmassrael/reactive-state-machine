#include "runtime/IJavaScriptEngine.h"

#include "common/Logger.h"
#include "common/ErrorCategories.h"

namespace SCXML {

std::unique_ptr<IJavaScriptEngine> JavaScriptEngineFactory::create(EngineType type) {
    switch (type) {
    case EngineType::V8:
        SCXML::Common::Logger::error("JavaScriptEngineFactory: V8 engine not available");
        // 미래 확장을 위한 자리
        // return std::make_unique<V8Engine>();
        return nullptr;

    case EngineType::AUTO:
        SCXML::Common::Logger::error("JavaScriptEngineFactory: No JavaScript engines available");
        return nullptr;

    default:
        SCXML::Common::Logger::error("JavaScriptEngineFactory: Unknown engine type");
        return nullptr;
    }
}

std::vector<JavaScriptEngineFactory::EngineType> JavaScriptEngineFactory::getAvailableEngines() {
    std::vector<JavaScriptEngineFactory::EngineType> engines;

// V8 엔진이 빌드되어 있다면 추가
#ifdef USE_V8_ENGINE
    engines.push_back(EngineType::V8);
#endif

    engines.push_back(JavaScriptEngineFactory::EngineType::AUTO);

    return engines;
}

std::string JavaScriptEngineFactory::engineTypeToString(JavaScriptEngineFactory::EngineType type) {
    switch (type) {
    case JavaScriptEngineFactory::EngineType::V8:
        return "v8";
    case JavaScriptEngineFactory::EngineType::AUTO:
        return "auto";
    default:
        return "unknown";
    }
}

JavaScriptEngineFactory::EngineType JavaScriptEngineFactory::stringToEngineType(const std::string &str) {
    if (str == "v8") {
        return JavaScriptEngineFactory::EngineType::V8;
    }
    if (str == "auto") {
        return JavaScriptEngineFactory::EngineType::AUTO;
    }

    SCXML::Common::Logger::warning("JavaScriptEngineFactory: Unknown engine type '" + str + "', using auto");
    return JavaScriptEngineFactory::EngineType::AUTO;
}

}  // namespace SCXML