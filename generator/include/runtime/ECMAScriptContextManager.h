#pragma once

#include "runtime/IECMAScriptEngine.h"
#include <memory>
#include <mutex>

namespace SCXML {

/**
 * @brief Singleton manager for ECMAScript engine context
 * 
 * Ensures that all SCXML components (DataModel, GuardEvaluator, ActionExecutor) 
 * share the same JavaScript context, maintaining variable consistency across
 * the entire SCXML engine lifecycle.
 * 
 * This solves the critical architectural problem where different components
 * were using separate QuickJS contexts, causing variable isolation.
 */
class ECMAScriptContextManager {
public:
    /**
     * @brief Get the singleton instance
     */
    static ECMAScriptContextManager& getInstance();

    /**
     * @brief Get the shared ECMAScript engine instance
     * 
     * All SCXML components should use this method to access the JavaScript engine
     * instead of creating their own instances.
     * 
     * @return Shared ECMAScript engine instance
     */
    std::shared_ptr<IECMAScriptEngine> getSharedEngine();

    /**
     * @brief Initialize the shared engine with specific type
     * 
     * Should be called once during SCXML engine initialization.
     * 
     * @param engineType Type of ECMAScript engine to create
     * @return true if initialization successful
     */
    bool initializeEngine(int engineType = 0); // 0 = QuickJS

    /**
     * @brief Reset the shared engine context
     * 
     * Clears all variables and reinitializes the JavaScript context.
     * Should be called when starting a new SCXML session.
     */
    void resetContext();

    /**
     * @brief Shutdown and cleanup the shared engine
     * 
     * Should be called during SCXML engine shutdown.
     */
    void shutdown();

    /**
     * @brief Check if engine is initialized
     */
    bool isInitialized() const;

private:
    ECMAScriptContextManager() = default;
    ~ECMAScriptContextManager();
    
    // Prevent copying
    ECMAScriptContextManager(const ECMAScriptContextManager&) = delete;
    ECMAScriptContextManager& operator=(const ECMAScriptContextManager&) = delete;

    std::shared_ptr<IECMAScriptEngine> sharedEngine_;
    mutable std::mutex engineMutex_;
    bool initialized_ = false;
};

} // namespace SCXML