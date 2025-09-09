#pragma once

#include "runtime/IECMAScriptEngine.h"
#include <memory>
#include <mutex>

namespace SCXML {

/**
 * @brief Factory for creating ECMAScript engines
 *
 * Provides graceful engine switching between different JavaScript engines
 * like JavaScriptExpressionEvaluator.
 */
class ECMAScriptEngineFactory {
public:
    /**
     * @brief Available engine types
     */
    enum class EngineType {
        QUICKJS  ///< Use QuickJS JavaScript engine (default and only supported)
    };

    /**
     * @brief Create ECMAScript engine instance
     * @param type Engine type to create (defaults to QuickJS)
     * @return Unique pointer to engine instance
     */
    static std::unique_ptr<IECMAScriptEngine> create(EngineType type = EngineType::QUICKJS);

    /**
     * @brief Check if specific engine type is available
     * @param type Engine type to check
     * @return true if engine is available
     */
    static bool isEngineAvailable(EngineType type);

    /**
     * @brief Get default engine type for current system
     * @return Recommended engine type (always QuickJS)
     */
    static EngineType getDefaultEngineType();

    /**
     * @brief Get engine type name as string
     * @param type Engine type
     * @return String representation
     */
    static std::string getEngineTypeName(EngineType type);
};

}  // namespace SCXML