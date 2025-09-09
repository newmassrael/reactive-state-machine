#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "model/DocumentModel.h"
#include "runtime/ParserRuntimeBridge.h"
#include "runtime/Processor.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {

// Forward declaration
class IECMAScriptEngine;

namespace Model {
class DocumentModel;
}
namespace Runtime {

/**
 * @brief State Machine Factory - High-level interface for creating SCXML state machines
 *
 * This factory class provides a simplified interface for creating and configuring
 * SCXML state machines. It encapsulates the complexity of the parser-runtime bridge
 * and provides convenient methods for common use cases.
 */
class StateMachineFactory {
public:
    /**
     * @brief State machine creation options
     */
    /**
     * @brief ECMAScript engine configuration options
     */
    enum class ECMAScriptEngine {
        AUTO,     // Auto-detect and use best available (QuickJS preferred)
        QUICKJS,  // Force QuickJS engine
        NONE      // Disable ECMAScript support (null datamodel only)
    };

    struct CreationOptions {
        std::string name = "state_machine";  // State machine name
        bool enableLogging = true;           // Enable state change logging
        bool enableEventTracing = false;     // Enable event tracing
        bool validateModel = true;           // Validate SCXML model
        bool enableOptimizations = true;     // Enable runtime optimizations
        size_t maxEventQueueSize = 1000;     // Maximum event queue size
        int maxEventRate = 0;                // Max events/sec (0=unlimited)

        // ECMAScript integration options
        ECMAScriptEngine ecmaScriptEngine = ECMAScriptEngine::AUTO;  // ECMAScript engine selection
        bool autoDetectDatamodel = true;    // Automatically detect datamodel type and setup engine
        
        // Advanced ECMAScript options
        struct ScriptOptions {
            size_t memoryLimit = 16 * 1024 * 1024;  // Memory limit in bytes (16MB default)
            size_t stackSize = 256 * 1024;          // Stack size in bytes (256KB default)
            bool enableDebugging = false;           // Enable JavaScript debugging features
            std::chrono::milliseconds timeout = std::chrono::milliseconds(5000);  // Script timeout
        } scriptOptions;

        // Callback functions
        std::function<void(const std::string &)> onStateChange;  // State change callback
        std::function<void(const std::string &)> onError;        // Error callback
        std::function<void(const std::string &)> onWarning;      // Warning callback
    };

    /**
     * @brief State machine creation result
     */
    struct CreationResult {
        bool success = false;
        std::shared_ptr<Processor> runtime;
        std::shared_ptr<::SCXML::Model::DocumentModel> model;
        std::string errorMessage;
        std::vector<std::string> warnings;

        // Convenience methods
        bool isValid() const {
            return success && runtime != nullptr;
        }

        std::string getName() const {
            return runtime ? runtime->getName() : "";
        }
    };

public:
    /**
     * @brief Create state machine from SCXML file
     * @param filePath Path to SCXML file
     * @param options Creation options
     * @return Creation result with runtime and model
     */
    static CreationResult createFromFile(const std::string &filePath, const CreationOptions &options);

    static CreationResult createFromFile(const std::string &filePath) {
        return createFromFile(filePath, getDefaultOptions());
    }

    /**
     * @brief Create state machine from SCXML content
     * @param scxmlContent SCXML document content
     * @param options Creation options
     * @return Creation result with runtime and model
     */
    static CreationResult createFromContent(const std::string &scxmlContent, const CreationOptions &options);

    static CreationResult createFromContent(const std::string &scxmlContent) {
        return createFromContent(scxmlContent, getDefaultOptions());
    }

    /**
     * @brief Create state machine from pre-parsed model
     * @param model Pre-parsed SCXML model
     * @param options Creation options
     * @return Creation result with runtime
     */
    static CreationResult create(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                const CreationOptions &options);

    static CreationResult create(std::shared_ptr<::SCXML::Model::DocumentModel> model) {
        return create(model, getDefaultOptions());
    }

    /**
     * @brief Create and start state machine from file
     * @param filePath Path to SCXML file
     * @param machineName Optional machine name
     * @return Started runtime instance or nullptr on error
     */
    static std::shared_ptr<Processor> startFromFile(const std::string &filePath,
                                                    const std::string &machineName = "state_machine");

    /**
     * @brief Create and start state machine from content
     * @param scxmlContent SCXML document content
     * @param machineName Optional machine name
     * @return Started runtime instance or nullptr on error
     */
    static std::shared_ptr<Processor> startFromContent(const std::string &scxmlContent,
                                                       const std::string &machineName = "state_machine");

    /**
     * @brief Create multiple state machines from directory
     * @param directoryPath Directory containing SCXML files
     * @param options Creation options (applied to all)
     * @return Vector of creation results
     */
    static std::vector<CreationResult> createFromDirectory(const std::string &directoryPath,
                                                           const CreationOptions &options);

    static std::vector<CreationResult> createFromDirectory(const std::string &directoryPath) {
        return createFromDirectory(directoryPath, getDefaultOptions());
    }

    /**
     * @brief Get default creation options with common settings
     * @return Default creation options
     */
    static CreationOptions getDefaultOptions();

    /**
     * @brief Get debugging-enabled creation options
     * @return Debug creation options
     */
    static CreationOptions getDebugOptions();

    /**
     * @brief Get performance-optimized creation options
     * @return Performance creation options
     */
    static CreationOptions getPerformanceOptions();

private:
    /**
     * @brief Internal creation from bridge result
     * @param bridgeResult Result from parser-runtime bridge
     * @param options Creation options
     * @return Creation result
     */
    static CreationResult createFromBridgeResult(const ParserRuntimeBridge::InitializationResult &bridgeResult,
                                                 const CreationOptions &options);

    /**
     * @brief Configure runtime with options
     * @param runtime Runtime to configure
     * @param options Configuration options
     * @return true if successful
     */
    static bool configureRuntime(std::shared_ptr<Processor> runtime, const CreationOptions &options);

public:
    /**
     * @brief Validate SCXML file
     * @param scxmlFilePath Path to SCXML file to validate
     * @return Vector of validation errors (empty if valid)
     */
    static std::vector<std::string> validateScxmlFile(const std::string &scxmlFilePath);

    /**
     * @brief Setup ECMAScript engine for automatic datamodel integration
     * @param model Document model containing datamodel information
     * @param options Creation options containing ECMAScript configuration
     * @return true if successful setup or no setup needed
     */
    static bool setupECMAScriptEngine(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                      const CreationOptions &options);

    
    /**
     * @brief Setup ECMAScript engine and connect directly to RuntimeContext
     * @param model Document model to determine engine requirements
     * @param context RuntimeContext to configure with engine
     * @param options Creation options including engine configuration
     * @return true if successfully setup and connected
     */
    static bool setupECMAScriptEngineForContext(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                               std::shared_ptr<SCXML::Runtime::RuntimeContext> context,
                                               const CreationOptions &options);

    /**
     * @brief Configure ECMAScript engine with specified options
     * @param engine ECMAScript engine to configure
     * @param options Engine-specific configuration options
     * @return true if successfully configured
     */
    static bool configureECMAScriptEngine(IECMAScriptEngine *engine,
                                          const CreationOptions::ScriptOptions &options);


};

// ========== Convenience Macros ==========

/**
 * @brief Quick macro for creating and starting a state machine
 * Usage: auto sm = CREATE_STATE_MACHINE("path/to/file.scxml", "MyMachine");
 */
#define CREATE_STATE_MACHINE(filePath, name) SCXML::Runtime::StateMachineFactory::quickStart(filePath, name)

/**
 * @brief Quick macro for creating state machine from content
 * Usage: auto sm = CREATE_STATE_MACHINE_FROM_CONTENT(xmlContent, "MyMachine");
 */
#define CREATE_STATE_MACHINE_FROM_CONTENT(content, name)                                                               \
    SCXML::Runtime::StateMachineFactory::quickStartFromContent(content, name)

}  // namespace Runtime
}  // namespace SCXML
