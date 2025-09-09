#include "runtime/ECMAScriptEngineFactory.h"

#ifdef HAS_QUICKJS
#include "runtime/QuickJSEngine.h"
#endif

#include "common/Logger.h"
#include <stdexcept>

namespace SCXML {

std::unique_ptr<IECMAScriptEngine> ECMAScriptEngineFactory::create(EngineType type) {
    SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - Creating engine type: " +
                                 std::to_string(static_cast<int>(type)));

    switch (type) {
    case EngineType::QUICKJS:

#ifdef HAS_QUICKJS
        SCXML::Common::Logger::debug("ECMAScriptEngineFactory::create - Creating QuickJSEngine");
        return std::make_unique<QuickJSEngine>();
#else
        SCXML::Common::Logger::error(
            "ECMAScriptEngineFactory::create - QuickJS not available, but no fallback configured");
        throw std::runtime_error("QuickJS engine not available and no fallback configured");
#endif

    default:
        SCXML::Common::Logger::error("ECMAScriptEngineFactory::create - Unknown engine type");
        throw std::runtime_error("Unknown engine type specified");
    }
}

bool ECMAScriptEngineFactory::isEngineAvailable(EngineType type) {
    switch (type) {
    case EngineType::QUICKJS:

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
    return EngineType::QUICKJS;
}

std::string ECMAScriptEngineFactory::getEngineTypeName(EngineType type) {
    switch (type) {
    case EngineType::QUICKJS:
        return "QuickJS";
    default:
        return "Unknown";
    }
}

}  // namespace SCXML