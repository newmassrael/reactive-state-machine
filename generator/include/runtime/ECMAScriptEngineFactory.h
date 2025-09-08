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
        AUTO,                  ///< Automatic selection (QuickJS if available, fallback otherwise)
        JAVASCRIPT_EVALUATOR,  ///< Use JavaScriptExpressionEvaluator (legacy fallback)
        QUICKJS,              ///< Use QuickJS JavaScript engine (full ECMAScript support)
        FULL_JAVASCRIPT       ///< Legacy alias for QuickJS
    };

    /**
     * @brief Create ECMAScript engine instance
     * @param type Engine type to create
     * @return Unique pointer to engine instance
     */
    static std::unique_ptr<IECMAScriptEngine> create(EngineType type = EngineType::AUTO);

    /**
     * @brief Check if specific engine type is available
     * @param type Engine type to check
     * @return true if engine is available
     */
    static bool isEngineAvailable(EngineType type);

    /**
     * @brief Get default engine type for current system
     * @return Recommended engine type
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