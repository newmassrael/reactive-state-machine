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
    struct CreationOptions {
        std::string name = "state_machine";  // State machine name
        bool enableLogging = true;           // Enable state change logging
        bool enableEventTracing = false;     // Enable event tracing
        bool validateModel = true;           // Validate SCXML model
        bool enableOptimizations = true;     // Enable runtime optimizations
        size_t maxEventQueueSize = 1000;     // Maximum event queue size
        int maxEventRate = 0;                // Max events/sec (0=unlimited)

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
    static CreationResult createFromModel(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                          const CreationOptions &options);

    static CreationResult createFromModel(std::shared_ptr<::SCXML::Model::DocumentModel> model) {
        return createFromModel(model, getDefaultOptions());
    }

    /**
     * @brief Quick create and start state machine from file
     * @param filePath Path to SCXML file
     * @param machineName Optional machine name
     * @return Started runtime instance or nullptr on error
     */
    static std::shared_ptr<Processor> quickStart(const std::string &filePath,
                                                 const std::string &machineName = "quick_machine");

    /**
     * @brief Quick create and start state machine from content
     * @param scxmlContent SCXML document content
     * @param machineName Optional machine name
     * @return Started runtime instance or nullptr on error
     */
    static std::shared_ptr<Processor> quickStartFromContent(const std::string &scxmlContent,
                                                            const std::string &machineName = "quick_machine");

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

    /**
     * @brief Validate SCXML file
     * @param scxmlFilePath Path to SCXML file to validate
     * @return Vector of validation errors (empty if valid)
     */
    static std::vector<std::string> validateScxmlFile(const std::string &scxmlFilePath);
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
