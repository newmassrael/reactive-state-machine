#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "model/INodeFactory.h"
#include "model/DocumentModel.h"
#include "parsing/DocumentParser.h"
#include "runtime/Processor.h"
#include "runtime/RuntimeContext.h"

namespace SCXML {

// Forward declarations - included headers provide definitions

/**
 * @brief Parser-Runtime Bridge - Connects SCXML parsing with runtime execution
 *
 * This class serves as the bridge between the SCXML parsing layer and the runtime
 * execution layer. It handles:
 * - SCXML document parsing and validation
 * - Model transformation for runtime optimization
 * - Runtime initialization with parsed model
 * - Error handling and reporting across both layers
 */
class ParserRuntimeBridge {
public:
    /**
     * @brief Bridge initialization result
     */
    struct InitializationResult {
        bool success = false;
        ::std::string errorMessage;
        ::std::vector<::std::string> warnings;
        ::std::shared_ptr<Model::DocumentModel> model;
        ::std::shared_ptr<Processor> runtime;
    };

    /**
     * @brief Model transformation options
     */
    struct TransformationOptions {
        bool enableOptimizations = true;  // Enable runtime optimizations
        bool validateTransitions = true;  // Validate transition consistency
        bool resolveIncludes = true;      // Process XInclude directives
        bool precompileGuards = false;    // Precompile guard expressions
        bool enableDebugging = false;     // Add debugging information
    };

    /**
     * @brief Runtime configuration options
     */
    struct RuntimeConfig {
        size_t maxEventQueueSize = 1000;       // Maximum event queue size
        bool enableEventTracing = false;       // Enable event tracing
        bool enableStateLogging = true;        // Enable state change logging
        int maxEventRate = 0;                  // Max events per second (0 = unlimited)
        ::std::string name = "scxml_runtime";  // Runtime instance name
    };

public:
    /**
     * @brief Constructor
     * @param nodeFactory Factory for creating SCXML node instances
     */
    explicit ParserRuntimeBridge(::std::shared_ptr<Model::INodeFactory> nodeFactory = nullptr);

    /**
     * @brief Destructor
     */
    ~ParserRuntimeBridge();

    // ========== Core Bridge Operations ==========

    /**
     * @brief Parse SCXML file and create runtime
     * @param filePath Path to SCXML file
     * @param transformOptions Model transformation options
     * @param runtimeConfig Runtime configuration
     * @return Initialization result with model and runtime
     */
    InitializationResult createRuntimeFromFile(const ::std::string &filePath,
                                               const TransformationOptions &transformOptions,
                                               const RuntimeConfig &runtimeConfig);

    /**
     * @brief Parse SCXML content and create runtime
     * @param scxmlContent SCXML document content
     * @param transformOptions Model transformation options
     * @param runtimeConfig Runtime configuration
     * @return Initialization result with model and runtime
     */
    InitializationResult createRuntimeFromContent(const ::std::string &scxmlContent,
                                                  const TransformationOptions &transformOptions,
                                                  const RuntimeConfig &runtimeConfig);

    /**
     * @brief Initialize runtime with pre-parsed model
     * @param model Pre-parsed SCXML model
     * @param runtimeConfig Runtime configuration
     * @return Initialization result with runtime
     */
    InitializationResult createRuntimeFromModel(::std::shared_ptr<Model::DocumentModel> model,
                                                const RuntimeConfig &runtimeConfig);

    // ========== Model Transformation ==========

    /**
     * @brief Transform parsed model for runtime optimization
     * @param model Source SCXML model
     * @param options Transformation options
     * @return Transformed model optimized for runtime
     */
    ::std::shared_ptr<Model::DocumentModel> transformModelForRuntime(::std::shared_ptr<Model::DocumentModel> model,
                                                                     const TransformationOptions &options);

    /**
     * @brief Validate SCXML model for runtime compatibility
     * @param model SCXML model to validate
     * @return Vector of validation error messages (empty if valid)
     */
    ::std::vector<::std::string> validateModelForRuntime(::std::shared_ptr<Model::DocumentModel> model);

    // ========== Runtime Integration ==========

    /**
     * @brief Initialize runtime context from SCXML model
     * @param model SCXML model
     * @param context Runtime context to initialize
     * @return true if initialization successful
     */
    bool initializeRuntimeContext(::std::shared_ptr<Model::DocumentModel> model, Runtime::RuntimeContext &context);

    /**
     * @brief Configure runtime components from model
     * @param runtime Runtime instance to configure
     * @param model SCXML model
     * @return true if configuration successful
     */
    bool configureRuntimeComponents(::std::shared_ptr<Processor> runtime,
                                    ::std::shared_ptr<Model::DocumentModel> model);

    // ========== Error Handling ==========

    /**
     * @brief Get last error message
     * @return Last error message
     */
    const ::std::string &getLastError() const;

    /**
     * @brief Get all warning messages from last operation
     * @return Vector of warning messages
     */
    const ::std::vector<::std::string> &getWarnings() const;

    /**
     * @brief Check if bridge has errors
     * @return true if there are unhandled errors
     */
    bool hasErrors() const;

    /**
     * @brief Clear all error and warning messages
     */
    void clearMessages();

    // ========== Utility Functions ==========

    /**
     * @brief Get supported SCXML features
     * @return Vector of supported feature names
     */
    static ::std::vector<::std::string> getSupportedFeatures();

    /**
     * @brief Check if SCXML feature is supported
     * @param feature Feature name to check
     * @return true if feature is supported
     */
    static bool isFeatureSupported(const ::std::string &feature);

    /**
     * @brief Set custom parser factory
     * @param factory Custom node factory
     */
    void setNodeFactory(::std::shared_ptr<Model::INodeFactory> factory);

    /**
     * @brief Get current parser factory
     * @return Current node factory
     */
    ::std::shared_ptr<Model::INodeFactory> getNodeFactory() const;

private:
    // ========== Private Implementation ==========

    /**
     * @brief Internal initialization from parser result
     * @param model Parsed SCXML model
     * @param transformOptions Transformation options
     * @param runtimeConfig Runtime configuration
     * @return Initialization result
     */
    InitializationResult initializeFromModel(::std::shared_ptr<Model::DocumentModel> model,
                                             const TransformationOptions &transformOptions,
                                             const RuntimeConfig &runtimeConfig);

    /**
     * @brief Apply model transformations
     * @param model Model to transform
     * @param options Transformation options
     * @return true if successful
     */
    bool applyModelTransformations(::std::shared_ptr<Model::DocumentModel> model, const TransformationOptions &options);

    /**
     * @brief Optimize state transitions for runtime
     * @param model Model to optimize
     * @return true if successful
     */
    bool optimizeTransitions(::std::shared_ptr<Model::DocumentModel> model);

    /**
     * @brief Precompile guard expressions
     * @param model Model containing guards
     * @return true if successful
     */
    bool precompileGuards(::std::shared_ptr<Model::DocumentModel> model);

    /**
     * @brief Add error message
     * @param message Error message
     */
    void addError(const ::std::string &message);

    /**
     * @brief Add warning message
     * @param message Warning message
     */
    void addWarning(const ::std::string &message);

    /**
     * @brief Setup ECMAScript engine for RuntimeContext based on model datamodel
     * @param model Document model containing datamodel information
     * @param context Runtime context to setup engine for
     * @return true if setup successful or not needed
     */
    bool setupECMAScriptEngineForContext(::std::shared_ptr<Model::DocumentModel> model,
                                         Runtime::RuntimeContext &context);

    // ========== Member Variables ==========

    ::std::shared_ptr<Model::INodeFactory> nodeFactory_;  // Node factory for parsing
    ::std::shared_ptr<Parsing::DocumentParser> parser_;  // SCXML parser instance

    ::std::string lastError_;                // Last error message
    ::std::vector<::std::string> warnings_;  // Warning messages

    // Statistics
    size_t parsedModels_ = 0;     // Number of parsed models
    size_t createdRuntimes_ = 0;  // Number of created runtimes
};

// ========== Convenience Functions ==========

/**
 * @brief Quick runtime creation from SCXML file
 * @param filePath Path to SCXML file
 * @param runtimeName Optional runtime name
 * @return Runtime instance or nullptr on error
 */
::std::shared_ptr<Processor> createProcessor(const ::std::string &filePath,
                                             const ::std::string &runtimeName = "default");

/**
 * @brief Quick runtime creation from SCXML content
 * @param scxmlContent SCXML document content
 * @param runtimeName Optional runtime name
 * @return Runtime instance or nullptr on error
 */
::std::shared_ptr<Processor> createProcessorFromContent(const ::std::string &scxmlContent,
                                                        const ::std::string &runtimeName = "default");

}  // namespace SCXML