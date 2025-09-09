#include "runtime/ParserRuntimeBridge.h"
#include "common/Logger.h"
#include "core/DataNode.h"
#include "core/NodeFactory.h"
#include "parsing/XIncludeProcessor.h"
#include "runtime/DataModelEngine.h"
#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/GuardEvaluator.h"
#include "runtime/InitialStateResolver.h"
#include "runtime/StateMachineFactory.h"
#include "runtime/TransitionExecutor.h"
#include "runtime/impl/DataContextManager.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace std;

namespace SCXML {

// ========== Constructor/Destructor ==========

ParserRuntimeBridge::ParserRuntimeBridge(std::shared_ptr<Model::INodeFactory> nodeFactory) : nodeFactory_(nodeFactory) {
    // Use default factory if none provided
    if (!nodeFactory_) {
        nodeFactory_ = std::make_shared<Core::NodeFactory>();
    }

    // Initialize parser with factory
    auto xincludeProcessor = std::make_shared<Parsing::XIncludeProcessor>();
    parser_ = std::make_shared<Parsing::DocumentParser>(nodeFactory_, xincludeProcessor);
}

ParserRuntimeBridge::~ParserRuntimeBridge() {
    // Cleanup handled by smart pointers
}

// ========== Core Bridge Operations ==========

ParserRuntimeBridge::InitializationResult
ParserRuntimeBridge::createRuntimeFromFile(const std::string &filePath, const TransformationOptions &transformOptions,
                                           const RuntimeConfig &runtimeConfig) {
    InitializationResult result;
    clearMessages();

    try {
        // Parse SCXML file
        auto model = parser_->parseFile(filePath);
        if (!model) {
            addError("Failed to parse SCXML file: " + filePath);

            // Add parser errors
            if (parser_->hasErrors()) {
                for (const auto &error : parser_->getErrorMessages()) {
                    addError("Parser: " + error);
                }
            }

            result.errorMessage = getLastError();
            return result;
        }

        // Add parser warnings
        for (const auto &warning : parser_->getWarningMessages()) {
            addWarning("Parser: " + warning);
        }

        // Initialize from parsed model
        result = initializeFromModel(model, transformOptions, runtimeConfig);
        ++parsedModels_;

    } catch (const std::exception &e) {
        addError("Exception during file parsing: " + std::string(e.what()));
        result.errorMessage = getLastError();
    }

    return result;
}

ParserRuntimeBridge::InitializationResult
ParserRuntimeBridge::createRuntimeFromContent(const std::string &scxmlContent,
                                              const TransformationOptions &transformOptions,
                                              const RuntimeConfig &runtimeConfig) {
    InitializationResult result;
    clearMessages();

    try {
        // Parse SCXML content
        auto model = parser_->parseContent(scxmlContent);
        if (!model) {
            addError("Failed to parse SCXML content");

            // Add parser errors
            if (parser_->hasErrors()) {
                for (const auto &error : parser_->getErrorMessages()) {
                    addError("Parser: " + error);
                }
            }

            result.errorMessage = getLastError();
            return result;
        }

        // Add parser warnings
        for (const auto &warning : parser_->getWarningMessages()) {
            addWarning("Parser: " + warning);
        }

        // Initialize from parsed model
        result = initializeFromModel(model, transformOptions, runtimeConfig);
        ++parsedModels_;

    } catch (const std::exception &e) {
        addError("Exception during content parsing: " + std::string(e.what()));
        result.errorMessage = getLastError();
    }

    return result;
}

ParserRuntimeBridge::InitializationResult
ParserRuntimeBridge::createRuntimeFromModel(std::shared_ptr<Model::DocumentModel> model,
                                            const RuntimeConfig &runtimeConfig) {
    InitializationResult result;
    clearMessages();

    if (!model) {
        addError("Cannot initialize runtime with null model");
        result.errorMessage = getLastError();
        return result;
    }

    // Use default transformation options
    TransformationOptions transformOptions;

    return initializeFromModel(model, transformOptions, runtimeConfig);
}

// ========== Model Transformation ==========

std::shared_ptr<Model::DocumentModel>
ParserRuntimeBridge::transformModelForRuntime(std::shared_ptr<Model::DocumentModel> model,
                                              const TransformationOptions &options) {
    if (!model) {
        addError("Cannot transform null model");
        return nullptr;
    }

    try {
        // Create a copy for transformation (in a real implementation, we might deep copy)
        auto transformedModel = model;

        // Apply transformations
        if (!applyModelTransformations(transformedModel, options)) {
            addError("Failed to apply model transformations");
            return nullptr;
        }

        return transformedModel;

    } catch (const std::exception &e) {
        addError("Exception during model transformation: " + std::string(e.what()));
        return nullptr;
    }
}

std::vector<std::string> ParserRuntimeBridge::validateModelForRuntime(std::shared_ptr<Model::DocumentModel> model) {
    std::vector<std::string> errors;

    if (!model) {
        errors.push_back("Model is null");
        return errors;
    }

    // Validate basic model structure
    if (!model->getRootState()) {
        errors.push_back("Model has no root state");
    }

    if (model->getInitialState().empty()) {
        errors.push_back("Model has no initial state specified");
    }

    // Validate state relationships
    if (!model->validateStateRelationships()) {
        errors.push_back("Model has invalid state relationships");

        // Get specific missing state IDs
        auto missingStates = model->findMissingStateIds();
        for (const auto &missingState : missingStates) {
            errors.push_back("Referenced state not found: " + missingState);
        }
    }

    // Validate initial state exists
    auto initialState = model->findStateById(model->getInitialState());
    if (!initialState) {
        errors.push_back("Initial state not found: " + model->getInitialState());
    }

    // Additional runtime-specific validations
    try {
        // Check for circular dependencies
        // Check for unreachable states
        // Validate guard expressions
        // Validate action expressions

        // These would be more comprehensive in a full implementation

    } catch (const std::exception &e) {
        errors.push_back("Validation exception: " + std::string(e.what()));
    }

    return errors;
}

// ========== Runtime Integration ==========

bool ParserRuntimeBridge::initializeRuntimeContext(std::shared_ptr<Model::DocumentModel> model,
                                                   Runtime::RuntimeContext &context) {
    if (!model) {
        addError("Cannot initialize context with null model");
        return false;
    }

    try {
        // Setup ECMAScript engine if required by datamodel
        if (!setupECMAScriptEngineForContext(model, context)) {
            addError("Failed to setup ECMAScript engine for datamodel: " + model->getDatamodel());
            return false;
        }
        // Initialize data model variables with graceful error handling
        for (const auto &dataItem : model->getDataModelItems()) {
            if (dataItem) {
                // Cast to DataNode and initialize
                auto dataNode = std::dynamic_pointer_cast<Core::DataNode>(dataItem);
                if (dataNode && !dataNode->initialize(context)) {
                    // Instead of failing completely, log the error and continue
                    // This allows the state machine to work even if some data model items fail
                    std::string errorMsg = "Failed to initialize data model item: " + dataItem->getId();
                    addWarning(errorMsg + " (continuing with degraded functionality)");
                    SCXML::Common::Logger::warning(errorMsg + " - ECMAScript engine may not be available");

                    // Try to set a default value for the failed data item
                    try {
                        // This is a fallback approach - set a null/default value
                        context.setDataValue(dataItem->getId(), "null");
                        SCXML::Common::Logger::info("Set default value for failed data item: " + dataItem->getId());
                    } catch (...) {
                        // Even the fallback failed, but we continue
                        SCXML::Common::Logger::warning("Could not set fallback value for: " + dataItem->getId());
                    }
                }
            }
        }

        // Initialize system variables
        for (const auto &sysVar : model->getSystemVariables()) {
            if (sysVar) {
                // Cast to DataNode and initialize system variable
                auto dataNode = std::dynamic_pointer_cast<Core::DataNode>(sysVar);
                if (dataNode && !dataNode->initialize(context)) {
                    addError("Failed to initialize system variable: " + sysVar->getId());
                    return false;
                }
            }
        }

        // Set context properties
        for (const auto &[name, type] : model->getContextProperties()) {
            context.setProperty(name, type);
        }

        // Configure logging
        context.log("info", "Runtime context initialized for model: " + model->getName());

        return true;

    } catch (const std::exception &e) {
        addError("Failed to initialize runtime context: " + std::string(e.what()));
        return false;
    }
}

bool ParserRuntimeBridge::configureRuntimeComponents(std::shared_ptr<Processor> runtime,
                                                     std::shared_ptr<Model::DocumentModel> model) {
    if (!runtime || !model) {
        addError("Cannot configure runtime components with null runtime or model");
        return false;
    }

    try {
        // Set runtime name from model
        // Placeholder - Processor interface needs setName method

        // Placeholder - Processor interface needs setEventTracing and setMaxEventRate methods

        // Additional configuration based on model properties
        if (model->getBinding() == "early") {
            // Configure for early binding
            addWarning("Early binding mode detected - may affect performance");
        }

        return true;

    } catch (const std::exception &e) {
        addError("Failed to configure runtime components: " + std::string(e.what()));
        return false;
    }
}

// ========== Error Handling ==========

const std::string &ParserRuntimeBridge::getLastError() const {
    return lastError_;
}

const std::vector<std::string> &ParserRuntimeBridge::getWarnings() const {
    return warnings_;
}

bool ParserRuntimeBridge::hasErrors() const {
    return !lastError_.empty();
}

void ParserRuntimeBridge::clearMessages() {
    lastError_.clear();
    warnings_.clear();
}

// ========== Utility Functions ==========

std::vector<std::string> ParserRuntimeBridge::getSupportedFeatures() {
    return {
        "scxml-core",                  // Core SCXML functionality
        "scxml-datamodel-ecmascript",  // ECMAScript data model (partial)
        "scxml-datamodel-null",        // Null data model
        "scxml-transitions",           // Transition processing
        "scxml-states",                // State management
        "scxml-events-internal",       // Internal events
        "scxml-events-external",       // External events
        "scxml-guards",                // Guard conditions
        "scxml-actions-log",           // Log actions
        "scxml-actions-assign",        // Variable assignment
        "scxml-actions-send",          // Event sending
        "scxml-parallel",              // Parallel states (basic)
        "scxml-history-shallow",       // Shallow history
        "scxml-history-deep",          // Deep history (basic)
        "scxml-invoke-basic"           // Basic invoke functionality
    };
}

bool ParserRuntimeBridge::isFeatureSupported(const std::string &feature) {
    auto supportedFeatures = getSupportedFeatures();
    return std::find(supportedFeatures.begin(), supportedFeatures.end(), feature) != supportedFeatures.end();
}

void ParserRuntimeBridge::setNodeFactory(std::shared_ptr<Model::INodeFactory> factory) {
    if (factory) {
        nodeFactory_ = factory;

        // Recreate parser with new factory
        auto xincludeProcessor = std::make_shared<Parsing::XIncludeProcessor>();
        parser_ = std::make_shared<Parsing::DocumentParser>(nodeFactory_, xincludeProcessor);
    }
}

std::shared_ptr<Model::INodeFactory> ParserRuntimeBridge::getNodeFactory() const {
    return nodeFactory_;
}

// ========== Private Implementation ==========

ParserRuntimeBridge::InitializationResult
ParserRuntimeBridge::initializeFromModel(std::shared_ptr<Model::DocumentModel> model,
                                         const TransformationOptions &transformOptions,
                                         const RuntimeConfig &runtimeConfig) {
    InitializationResult result;

    // Validate model for runtime
    auto validationErrors = validateModelForRuntime(model);
    if (!validationErrors.empty()) {
        for (const auto &error : validationErrors) {
            addError("Model validation: " + error);
        }
        result.errorMessage = getLastError();
        return result;
    }

    // Transform model for runtime if needed
    auto transformedModel = model;
    if (transformOptions.enableOptimizations) {
        transformedModel = transformModelForRuntime(model, transformOptions);
        if (!transformedModel) {
            result.errorMessage = getLastError();
            return result;
        }
    }

    // Create runtime instance
    auto runtime = std::make_shared<SCXML::Processor>(runtimeConfig.name);

    // Initialize runtime with model
    if (!runtime->initialize(transformedModel)) {
        addError("Failed to initialize runtime with model");
        result.errorMessage = getLastError();
        return result;
    }

    // Configure runtime components
    if (!configureRuntimeComponents(runtime, transformedModel)) {
        result.errorMessage = getLastError();
        return result;
    }

    // Set success result
    result.success = true;
    result.model = transformedModel;
    result.runtime = runtime;
    result.warnings = warnings_;

    ++createdRuntimes_;

    return result;
}

bool ParserRuntimeBridge::applyModelTransformations(std::shared_ptr<Model::DocumentModel> model,
                                                    const TransformationOptions &options) {
    try {
        if (options.enableOptimizations) {
            if (!optimizeTransitions(model)) {
                return false;
            }
        }

        if (options.precompileGuards) {
            if (!precompileGuards(model)) {
                return false;
            }
        }

        // Additional transformations can be added here

        return true;

    } catch (const std::exception &e) {
        addError("Model transformation failed: " + std::string(e.what()));
        return false;
    }
}

bool ParserRuntimeBridge::optimizeTransitions(std::shared_ptr<Model::DocumentModel> model) {
    (void)model;  // Suppress unused parameter warning
    // Placeholder for transition optimization
    // In a full implementation, this would:
    // - Sort transitions by priority
    // - Optimize transition conflict detection
    // - Pre-calculate transition paths
    // - Cache frequently used transitions

    addWarning("Transition optimization is not yet implemented");
    return true;
}

bool ParserRuntimeBridge::precompileGuards(std::shared_ptr<Model::DocumentModel> model) {
    (void)model;  // Suppress unused parameter warning
    // Placeholder for guard precompilation
    // In a full implementation, this would:
    // - Parse guard expressions into AST
    // - Optimize expression evaluation
    // - Cache compiled expressions
    // - Validate expression syntax

    addWarning("Guard precompilation is not yet implemented");
    return true;
}

void ParserRuntimeBridge::addError(const std::string &message) {
    lastError_ = message;
    SCXML::Common::Logger::error("ParserRuntimeBridge Error: " + message);
}

void ParserRuntimeBridge::addWarning(const std::string &message) {
    warnings_.push_back(message);
    SCXML::Common::Logger::warning("ParserRuntimeBridge Warning: " + message);
}

bool ParserRuntimeBridge::setupECMAScriptEngineForContext(std::shared_ptr<Model::DocumentModel> model,
                                                          Runtime::RuntimeContext &context) {
    if (!model) {
        return false;
    }

    try {
        auto datamodelType = model->getDatamodel();

        // Check if ECMAScript datamodel is specified
        if (datamodelType == "ecmascript" || datamodelType == "javascript") {
            SCXML::Common::Logger::info("Setting up ECMAScript engine for datamodel: " + datamodelType);

            // NOTE: This method is now deprecated. The new architecture uses
            // StateMachineFactory::setupECMAScriptEngineForContext instead.
            // Fallback: Create new ECMAScript engine using factory
            {
                SCXML::Common::Logger::warning("No stored ECMAScript engine found for model: " + model->getName());

                // Fallback: Create new ECMAScript engine using factory
                auto engine =
                    ::SCXML::ECMAScriptEngineFactory::create(::SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS);
                if (!engine) {
                    addError("Failed to create ECMAScript engine for datamodel: " + datamodelType);
                    return false;
                }

                // Initialize engine
                if (!engine->initialize()) {
                    addError("Failed to initialize ECMAScript engine");
                    return false;
                }

                SCXML::Common::Logger::info("Created fallback ECMAScript engine: " + engine->getEngineName());

                // Create a DataModelEngine and connect the engine
                auto dataModelEngine =
                    std::make_unique<SCXML::DataModelEngine>(SCXML::DataModelEngine::DataModelType::ECMASCRIPT);
                dataModelEngine->setECMAScriptEngine(std::move(engine));  // Move the unique_ptr to shared_ptr

                // Set the DataModelEngine in the RuntimeContext
                auto &dataContextManager =
                    dynamic_cast<SCXML::Runtime::DataContextManager &>(context.getDataContextManager());
                dataContextManager.setDataModelEngine(std::move(dataModelEngine));

                SCXML::Common::Logger::info("Successfully connected fallback ECMAScript engine to DataContextManager");
                return true;
            }
        }

        // Non-ECMAScript datamodel, no engine setup needed
        return true;

    } catch (const std::exception &e) {
        addError("Exception setting up ECMAScript engine: " + std::string(e.what()));
        return false;
    }
}

// ========== Convenience Functions ==========

std::shared_ptr<Processor> createProcessor(const std::string &filePath, const std::string &runtimeName) {
    try {
        ParserRuntimeBridge bridge;

        ParserRuntimeBridge::RuntimeConfig config;
        config.name = runtimeName;

        auto result = bridge.createRuntimeFromFile(filePath, {}, config);

        if (result.success) {
            return result.runtime;
        } else {
            SCXML::Common::Logger::error("Failed to create runtime: " + result.errorMessage);
            return nullptr;
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("Exception creating runtime: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<Processor> createProcessorFromContent(const std::string &scxmlContent, const std::string &runtimeName) {
    try {
        ParserRuntimeBridge bridge;

        ParserRuntimeBridge::RuntimeConfig config;
        config.name = runtimeName;

        auto result = bridge.createRuntimeFromContent(scxmlContent, {}, config);

        if (result.success) {
            return result.runtime;
        } else {
            SCXML::Common::Logger::error("Failed to create runtime: " + result.errorMessage);
            return nullptr;
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("Exception creating runtime: " + std::string(e.what()));
        return nullptr;
    }
}

}  // namespace SCXML