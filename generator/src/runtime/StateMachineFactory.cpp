#include "runtime/StateMachineFactory.h"
#include "common/Logger.h"
#include "core/NodeFactory.h"
#include "model/DocumentModel.h"
#include "parsing/DocumentParser.h"
#include "parsing/XIncludeProcessor.h"
#include "runtime/ECMAScriptContextManager.h"
#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/IECMAScriptEngine.h"
#include "runtime/ParserRuntimeBridge.h"
#include "runtime/StateMachineFactory.h"
#include "runtime/impl/DataContextManager.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace SCXML {
namespace Runtime {

// ========== ECMAScript Engine Storage Bridge ==========
// Static storage for ECMAScript engines to bridge StateMachineFactory and RuntimeContext

// ========== Public Static Methods ==========

StateMachineFactory::CreationResult StateMachineFactory::createFromFile(const std::string &filePath,
                                                                        const CreationOptions &options) {
    try {
        // Create parser-runtime bridge
        ParserRuntimeBridge bridge;

        // Convert options to bridge options
        ParserRuntimeBridge::TransformationOptions transformOptions;
        transformOptions.enableOptimizations = options.enableOptimizations;
        transformOptions.validateTransitions = options.validateModel;

        ParserRuntimeBridge::RuntimeConfig runtimeConfig;
        runtimeConfig.name = options.name;
        runtimeConfig.maxEventQueueSize = options.maxEventQueueSize;
        runtimeConfig.enableEventTracing = options.enableEventTracing;
        runtimeConfig.enableStateLogging = options.enableLogging;
        runtimeConfig.maxEventRate = options.maxEventRate;

        // Create runtime from file
        auto bridgeResult = bridge.createRuntimeFromFile(filePath, transformOptions, runtimeConfig);

        // Apply automatic ECMAScript integration if model was created successfully
        if (bridgeResult.success && bridgeResult.model && options.autoDetectDatamodel) {
            auto datamodelType = bridgeResult.model->getDatamodel();
            if (datamodelType == "ecmascript" || datamodelType == "javascript") {
                if (!setupECMAScriptEngine(bridgeResult.model, options)) {
                    CreationResult result;
                    result.success = false;
                    result.errorMessage = "Failed to setup ECMAScript engine for datamodel: " + datamodelType;
                    return result;
                }
            }
        }

        // Convert to creation result
        return createFromBridgeResult(bridgeResult, options);

    } catch (const std::exception &e) {
        CreationResult result;
        result.success = false;
        result.errorMessage = "Exception creating state machine: " + std::string(e.what());
        return result;
    }
}

StateMachineFactory::CreationResult StateMachineFactory::createFromContent(const std::string &scxmlContent,
                                                                           const CreationOptions &options) {
    try {
        // Create parser-runtime bridge
        ParserRuntimeBridge bridge;

        // Convert options to bridge options
        ParserRuntimeBridge::TransformationOptions transformOptions;
        transformOptions.enableOptimizations = options.enableOptimizations;
        transformOptions.validateTransitions = options.validateModel;

        ParserRuntimeBridge::RuntimeConfig runtimeConfig;
        runtimeConfig.name = options.name;
        runtimeConfig.maxEventQueueSize = options.maxEventQueueSize;
        runtimeConfig.enableEventTracing = options.enableEventTracing;
        runtimeConfig.enableStateLogging = options.enableLogging;
        runtimeConfig.maxEventRate = options.maxEventRate;

        // Create runtime from content
        auto bridgeResult = bridge.createRuntimeFromContent(scxmlContent, transformOptions, runtimeConfig);

        // Apply automatic ECMAScript integration if model was created successfully
        if (bridgeResult.success && bridgeResult.model && options.autoDetectDatamodel) {
            auto datamodelType = bridgeResult.model->getDatamodel();
            if (datamodelType == "ecmascript" || datamodelType == "javascript") {
                if (!setupECMAScriptEngine(bridgeResult.model, options)) {
                    CreationResult result;
                    result.success = false;
                    result.errorMessage = "Failed to setup ECMAScript engine for datamodel: " + datamodelType;
                    return result;
                }
            }
        }

        // Convert to creation result
        return createFromBridgeResult(bridgeResult, options);

    } catch (const std::exception &e) {
        CreationResult result;
        result.success = false;
        result.errorMessage = "Exception creating state machine: " + std::string(e.what());
        return result;
    }
}

StateMachineFactory::CreationResult StateMachineFactory::create(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                                const CreationOptions &options) {
    SCXML::Common::Logger::info("*** StateMachineFactory::create ENTRY (Clean Architecture) ***");

    try {
        if (!model) {
            CreationResult result;
            result.success = false;
            result.errorMessage = "Cannot create state machine from null model";
            return result;
        }

        // Step 1: Create RuntimeContext
        auto context = std::make_shared<SCXML::Runtime::RuntimeContext>();
        context->setModel(model);

        // Step 2: Setup ECMAScript engine if required by datamodel
        auto datamodelType = model->getDatamodel();

        // W3C SCXML compliance: fallback to ECMAScript for null/empty datamodel
        if (datamodelType.empty()) {
            datamodelType = "ecmascript";
            SCXML::Common::Logger::info(
                "StateMachineFactory::create - using ECMAScript as default for empty datamodel");
        }

        SCXML::Common::Logger::info("StateMachineFactory::create - detected datamodel: '" + datamodelType + "'");

        if (datamodelType == "ecmascript" || datamodelType == "javascript") {
            if (!setupECMAScriptEngineForContext(model, context, options)) {
                CreationResult result;
                result.success = false;
                result.errorMessage = "Failed to setup ECMAScript engine for datamodel: " + datamodelType;
                return result;
            }
        }

        // Step 3: Create Processor with fully configured context
        auto processor = std::make_shared<SCXML::Processor>(options.name);

        // Step 4: Initialize processor with pre-configured context
        if (!processor->initializeWithContext(model, context)) {
            CreationResult result;
            result.success = false;
            result.errorMessage = "Failed to initialize processor with configured context";
            return result;
        }

        // Step 5: Return success result
        CreationResult result;
        result.success = true;
        result.runtime = processor;
        result.model = model;
        SCXML::Common::Logger::info(
            "StateMachineFactory::create - Successfully created processor with clean architecture");
        return result;

    } catch (const std::exception &e) {
        CreationResult result;
        result.success = false;
        result.errorMessage = "Exception creating state machine: " + std::string(e.what());
        return result;
    }
}

std::shared_ptr<Processor> StateMachineFactory::startFromFile(const std::string &filePath,
                                                              const std::string &machineName) {
    CreationOptions options;
    options.name = machineName;
    options.enableLogging = true;

    auto result = createFromFile(filePath, options);

    if (result.success && result.runtime) {
        // Start the runtime
        if (result.runtime->start()) {
            return result.runtime;
        } else {
            SCXML::Common::Logger::error("Failed to start state machine: " + machineName);
            return nullptr;
        }
    } else {
        SCXML::Common::Logger::error("Failed to create state machine: " + result.errorMessage);
        return nullptr;
    }
}

std::shared_ptr<Processor> StateMachineFactory::startFromContent(const std::string &scxmlContent,
                                                                 const std::string &machineName) {
    CreationOptions options;
    options.name = machineName;
    options.enableLogging = true;

    auto result = createFromContent(scxmlContent, options);

    if (result.success && result.runtime) {
        // Start the runtime
        if (result.runtime->start()) {
            return result.runtime;
        } else {
            SCXML::Common::Logger::error("Failed to start state machine: " + machineName);
            return nullptr;
        }
    } else {
        SCXML::Common::Logger::error("Failed to create state machine: " + result.errorMessage);
        return nullptr;
    }
}

std::vector<StateMachineFactory::CreationResult>
StateMachineFactory::createFromDirectory(const std::string &directoryPath, const CreationOptions &options) {
    std::vector<CreationResult> results;

    try {
        // Check if directory exists
        if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath)) {
            CreationResult errorResult;
            errorResult.success = false;
            errorResult.errorMessage = "Directory not found: " + directoryPath;
            results.push_back(errorResult);
            return results;
        }

        // Find all SCXML files
        for (const auto &entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                auto extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                if (extension == ".scxml" || extension == ".xml") {
                    // Create state machine for this file
                    CreationOptions fileOptions = options;
                    if (fileOptions.name == "state_machine") {
                        // Use filename as machine name
                        fileOptions.name = entry.path().stem().string();
                    }

                    auto result = createFromFile(entry.path().string(), fileOptions);
                    results.push_back(result);
                }
            }
        }

    } catch (const std::exception &e) {
        CreationResult errorResult;
        errorResult.success = false;
        errorResult.errorMessage = "Exception processing directory: " + std::string(e.what());
        results.push_back(errorResult);
    }

    return results;
}

StateMachineFactory::CreationOptions StateMachineFactory::getDefaultOptions() {
    CreationOptions options;
    options.name = "default_machine";
    options.enableLogging = true;
    options.enableEventTracing = false;
    options.validateModel = true;
    options.enableOptimizations = true;
    options.maxEventQueueSize = 1000;
    options.maxEventRate = 0;  // Unlimited
    return options;
}

StateMachineFactory::CreationOptions StateMachineFactory::getDebugOptions() {
    CreationOptions options = getDefaultOptions();
    options.name = "debug_machine";
    options.enableLogging = true;
    options.enableEventTracing = true;
    options.validateModel = true;
    options.enableOptimizations = false;  // Disable for debugging
    options.maxEventRate = 10;            // Slower for debugging
    return options;
}

StateMachineFactory::CreationOptions StateMachineFactory::getPerformanceOptions() {
    CreationOptions options = getDefaultOptions();
    options.name = "performance_machine";
    options.enableLogging = false;  // Disable for performance
    options.enableEventTracing = false;
    options.validateModel = false;  // Skip validation for performance
    options.enableOptimizations = true;
    options.maxEventQueueSize = 10000;  // Larger queue
    options.maxEventRate = 0;           // Unlimited
    return options;
}

// ========== Private Methods ==========

StateMachineFactory::CreationResult
StateMachineFactory::createFromBridgeResult(const ParserRuntimeBridge::InitializationResult &bridgeResult,
                                            const CreationOptions &options) {
    CreationResult result;
    result.success = bridgeResult.success;
    result.runtime = bridgeResult.runtime;
    result.model = bridgeResult.model;
    result.errorMessage = bridgeResult.errorMessage;
    result.warnings = bridgeResult.warnings;

    if (result.success && result.runtime) {
        // Configure runtime with options
        if (!configureRuntime(result.runtime, options)) {
            result.success = false;
            result.errorMessage = "Failed to configure runtime with options";
        }
    }

    return result;
}

bool StateMachineFactory::configureRuntime(std::shared_ptr<Processor> runtime, const CreationOptions &options) {
    if (!runtime) {
        return false;
    }

    try {
        // Placeholder - Processor interface needs setName, setEventTracing, setMaxEventRate methods
        (void)options.name;
        (void)options.enableEventTracing;
        (void)options.maxEventRate;

        // Setup callbacks if provided
        if (options.onStateChange) {
            // Note: In a full implementation, we would register the callback
            // with the runtime's event system
        }

        if (options.onError) {
            // Note: In a full implementation, we would register the error callback
        }

        if (options.onWarning) {
            // Note: In a full implementation, we would register the warning callback
        }

        return true;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("Failed to configure runtime: " + std::string(e.what()));
        return false;
    }
}

bool StateMachineFactory::setupECMAScriptEngine(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                const CreationOptions &options) {
    if (!model) {
        SCXML::Common::Logger::error("Cannot setup ECMAScript engine: model is null");
        return false;
    }

    try {
        // Determine which engine to use
        ::SCXML::ECMAScriptEngineFactory::EngineType engineType;
        switch (options.ecmaScriptEngine) {
        case ECMAScriptEngine::AUTO:
            engineType = ::SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS;
            break;
        case ECMAScriptEngine::QUICKJS:
            engineType = ::SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS;
            break;
        case ECMAScriptEngine::NONE:
            // No engine setup needed
            return true;
        default:
            engineType = ::SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS;
            break;
        }

        // Initialize shared ECMAScript context for entire SCXML engine
        auto &contextManager = ::SCXML::ECMAScriptContextManager::getInstance();

        // Initialize shared engine if not already done
        if (!contextManager.isInitialized()) {
            if (!contextManager.initializeEngine(static_cast<int>(engineType))) {
                SCXML::Common::Logger::error("StateMachineFactory - Failed to initialize shared ECMAScript context");
                return false;  // QuickJS is now required
            }
        }

        // Get shared engine instance
        auto engine = contextManager.getSharedEngine();
        if (!engine) {
            SCXML::Common::Logger::error("StateMachineFactory - Failed to get shared ECMAScript engine");
            return false;
        }

        // Configure engine options (optional, failure won't stop execution)
        configureECMAScriptEngine(engine.get(), options.scriptOptions);

        // Get engine name before moving
        std::string engineName = engine->getEngineName();

        // NOTE: This legacy method now creates engines but doesn't store them globally.
        // The new architecture uses setupECMAScriptEngineForContext instead.

        SCXML::Common::Logger::info("Successfully setup and connected ECMAScript engine: " + engineName);

        return true;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("Exception setting up ECMAScript engine: " + std::string(e.what()));
        return false;
    }
}

bool StateMachineFactory::configureECMAScriptEngine(IECMAScriptEngine *engine,
                                                    const CreationOptions::ScriptOptions &options) {
    if (!engine) {
        return false;
    }

    try {
        // Note: These configurations would be implemented if the IECMAScriptEngine interface
        // supported them. For now, we'll log the intended configuration.
        SCXML::Common::Logger::info("ECMAScript Engine Configuration:");
        SCXML::Common::Logger::info("- Memory Limit: " + std::to_string(options.memoryLimit) + " bytes");
        SCXML::Common::Logger::info("- Stack Size: " + std::to_string(options.stackSize) + " bytes");
        SCXML::Common::Logger::info("- Debugging: " + std::string(options.enableDebugging ? "enabled" : "disabled"));
        SCXML::Common::Logger::info("- Timeout: " + std::to_string(options.timeout.count()) + "ms");

        // In a complete implementation, we would:
        // engine->setMemoryLimit(options.memoryLimit);
        // engine->setStackSize(options.stackSize);
        // engine->setDebuggingEnabled(options.enableDebugging);
        // engine->setTimeout(options.timeout);

        return true;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("Failed to configure ECMAScript engine: " + std::string(e.what()));
        return false;
    }
}

bool StateMachineFactory::setupECMAScriptEngineForContext(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                                          std::shared_ptr<SCXML::Runtime::RuntimeContext> context,
                                                          const CreationOptions &options) {
    if (!model || !context) {
        SCXML::Common::Logger::error("Cannot setup ECMAScript engine: model or context is null");
        return false;
    }

    try {
        // Determine which engine to use
        ::SCXML::ECMAScriptEngineFactory::EngineType engineType;
        switch (options.ecmaScriptEngine) {
        case ECMAScriptEngine::AUTO:
            engineType = ::SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS;
            break;
        case ECMAScriptEngine::QUICKJS:
            engineType = ::SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS;
            break;
        case ECMAScriptEngine::NONE:
            SCXML::Common::Logger::info("ECMAScript engine disabled by configuration");
            return true;
        default:
            engineType = ::SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS;
            break;
        }

        // Get shared ECMAScript engine from context manager
        auto &contextManager = ::SCXML::ECMAScriptContextManager::getInstance();
        if (!contextManager.isInitialized()) {
            if (!contextManager.initializeEngine(static_cast<int>(engineType))) {
                SCXML::Common::Logger::error("Failed to initialize shared ECMAScript engine");
                return false;
            }
        }

        auto engine = contextManager.getSharedEngine();
        if (!engine) {
            SCXML::Common::Logger::error("Failed to get shared ECMAScript engine");
            return false;
        }

        // Configure engine options
        configureECMAScriptEngine(engine.get(), options.scriptOptions);

        SCXML::Common::Logger::info("ECMAScript Engine Configuration:");
        SCXML::Common::Logger::info("- Memory Limit: " + std::to_string(options.scriptOptions.memoryLimit) + " bytes");
        SCXML::Common::Logger::info("- Stack Size: " + std::to_string(options.scriptOptions.stackSize) + " bytes");
        SCXML::Common::Logger::info("- Debugging: " +
                                    std::string(options.scriptOptions.enableDebugging ? "enabled" : "disabled"));
        SCXML::Common::Logger::info("- Timeout: " + std::to_string(options.scriptOptions.timeout.count()) + "ms");

        // Get engine name for logging
        std::string engineName = engine->getEngineName();

        // Create DataModelEngine and connect ECMAScript engine directly
        // CRITICAL: Use shared_ptr (not std::move) to maintain engine sharing across components
        auto dataModelEngine =
            std::make_unique<SCXML::DataModelEngine>(SCXML::DataModelEngine::DataModelType::ECMASCRIPT);
        dataModelEngine->setECMAScriptEngine(engine);

        // Connect to RuntimeContext immediately - no global storage needed
        auto &dataContextManager = dynamic_cast<SCXML::Runtime::DataContextManager &>(context->getDataContextManager());
        dataContextManager.setDataModelEngine(std::move(dataModelEngine));

        SCXML::Common::Logger::info("Successfully setup and connected ECMAScript engine directly to RuntimeContext: " +
                                    engineName);
        return true;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("Exception setting up ECMAScript engine for context: " + std::string(e.what()));
        return false;
    }
}

// ========== SCXML Validation Methods ==========

std::vector<std::string> StateMachineFactory::validateScxmlFile(const std::string &scxmlFilePath) {
    std::vector<std::string> errors;

    try {
        // Check if file exists
        if (!std::filesystem::exists(scxmlFilePath)) {
            errors.push_back("SCXML file does not exist: " + scxmlFilePath);
            return errors;
        }

        // Create parser with dependencies
        auto nodeFactory = std::make_shared<SCXML::Core::NodeFactory>();
        auto xincludeProcessor = std::make_shared<SCXML::Parsing::XIncludeProcessor>();
        SCXML::Parsing::DocumentParser parser(nodeFactory, xincludeProcessor);

        // Parse the file
        auto model = parser.parseFile(scxmlFilePath);

        if (!model) {
            errors.push_back("Failed to parse SCXML file: " + scxmlFilePath);
        }

        // Add parser errors to validation errors
        if (parser.hasErrors()) {
            const auto &parserErrors = parser.getErrorMessages();
            errors.insert(errors.end(), parserErrors.begin(), parserErrors.end());
        }

        // Basic model validation if parsed successfully
        if (model) {
            if (model->getName().empty()) {
                errors.push_back("SCXML model missing name attribute");
            }

            if (model->getInitialState().empty()) {
                errors.push_back("SCXML model missing initial state");
            }
        }

    } catch (const std::exception &e) {
        errors.push_back("Validation failed: " + std::string(e.what()));
    }

    return errors;
}

// ========== ECMAScript Engine Storage Methods ==========

}  // namespace Runtime
}  // namespace SCXML