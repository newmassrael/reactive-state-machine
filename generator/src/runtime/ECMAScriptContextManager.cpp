#include "runtime/ECMAScriptContextManager.h"
#include "runtime/ECMAScriptEngineFactory.h"
#include "common/Logger.h"
#include <stdexcept>
#include <thread>
#include <chrono>

namespace SCXML {

ECMAScriptContextManager& ECMAScriptContextManager::getInstance() {
    static ECMAScriptContextManager instance;
    return instance;
}

std::shared_ptr<IECMAScriptEngine> ECMAScriptContextManager::getSharedEngine() {
    std::lock_guard<std::mutex> lock(engineMutex_);
    
    if (!initialized_ || !sharedEngine_) {
        SCXML::Common::Logger::error("ECMAScriptContextManager::getSharedEngine - Engine not initialized. Call initializeEngine() first.");
        return nullptr;
    }
    
    return sharedEngine_;
}

bool ECMAScriptContextManager::initializeEngine(int engineType) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    
    if (initialized_) {
        SCXML::Common::Logger::warning("ECMAScriptContextManager::initializeEngine - Engine already initialized. Returning existing engine.");
        return true;
    }
    
    try {
        // Create the shared engine instance
        auto engine = ECMAScriptEngineFactory::create(
            static_cast<ECMAScriptEngineFactory::EngineType>(engineType)
        );
        
        if (!engine) {
            SCXML::Common::Logger::error("ECMAScriptContextManager::initializeEngine - Failed to create engine");
            return false;
        }
        
        // Initialize the engine
        if (!engine->initialize()) {
            SCXML::Common::Logger::error("ECMAScriptContextManager::initializeEngine - Failed to initialize engine");
            return false;
        }
        
        // Store as shared_ptr for safe sharing
        sharedEngine_ = std::move(engine);
        initialized_ = true;
        
        SCXML::Common::Logger::info("ECMAScriptContextManager::initializeEngine - Successfully initialized shared ECMAScript engine (Type: " + 
            ECMAScriptEngineFactory::getEngineTypeName(static_cast<ECMAScriptEngineFactory::EngineType>(engineType)) + ")");
        
        return true;
        
    } catch (const std::exception& e) {
        SCXML::Common::Logger::error("ECMAScriptContextManager::initializeEngine - Exception: " + std::string(e.what()));
        return false;
    }
}

void ECMAScriptContextManager::resetContext() {
    std::lock_guard<std::mutex> lock(engineMutex_);
    
    if (!initialized_ || !sharedEngine_) {
        SCXML::Common::Logger::warning("ECMAScriptContextManager::resetContext - Engine not initialized");
        return;
    }
    
    SCXML::Common::Logger::info("ECMAScriptContextManager::resetContext - Resetting JavaScript context for new session");
    
    // Complete shutdown with aggressive cleanup
    sharedEngine_->shutdown();
    
    // Allow time for complete cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Destroy and recreate engine instance to ensure complete cleanup
    sharedEngine_.reset();
    
    // Allow time before recreating
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    // Recreate fresh engine
    if (!initializeEngine(0)) {
        SCXML::Common::Logger::error("ECMAScriptContextManager::resetContext - Failed to reinitialize engine after reset");
        initialized_ = false;
    }
}

void ECMAScriptContextManager::shutdown() {
    std::lock_guard<std::mutex> lock(engineMutex_);
    
    if (initialized_ && sharedEngine_) {
        SCXML::Common::Logger::info("ECMAScriptContextManager::shutdown - Shutting down shared ECMAScript engine");
        
        // Force immediate cleanup of any pending JSValues
        // This is critical to prevent assertion during program exit
        initialized_ = false;  // Mark as not initialized first
        
        // Call shutdown and immediately reset to minimize timing issues
        sharedEngine_->shutdown();
        sharedEngine_.reset();
        
        SCXML::Common::Logger::info("ECMAScriptContextManager::shutdown - Shutdown complete");
    }
}

bool ECMAScriptContextManager::isInitialized() const {
    std::lock_guard<std::mutex> lock(engineMutex_);
    return initialized_ && sharedEngine_ != nullptr;
}

ECMAScriptContextManager::~ECMAScriptContextManager() {
    // Call shutdown explicitly - we've now fixed the underlying QuickJS cleanup issues
    shutdown();
}

} // namespace SCXML