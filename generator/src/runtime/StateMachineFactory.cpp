#include "runtime/StateMachineFactory.h"
#include "common/Logger.h"
#include "core/NodeFactory.h"
#include "parsing/DocumentParser.h"
#include "parsing/XIncludeProcessor.h"
#include "runtime/ParserRuntimeBridge.h"
#include "runtime/StateMachineFactory.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace SCXML {
namespace Runtime {

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

        // Convert to creation result
        return createFromBridgeResult(bridgeResult, options);

    } catch (const std::exception &e) {
        CreationResult result;
        result.success = false;
        result.errorMessage = "Exception creating state machine: " + std::string(e.what());
        return result;
    }
}

StateMachineFactory::CreationResult
StateMachineFactory::createFromModel(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                     const CreationOptions &options) {
    try {
        // Create parser-runtime bridge
        ParserRuntimeBridge bridge;

        ParserRuntimeBridge::RuntimeConfig runtimeConfig;
        runtimeConfig.name = options.name;
        runtimeConfig.maxEventQueueSize = options.maxEventQueueSize;
        runtimeConfig.enableEventTracing = options.enableEventTracing;
        runtimeConfig.enableStateLogging = options.enableLogging;
        runtimeConfig.maxEventRate = options.maxEventRate;

        // Create runtime from model
        auto bridgeResult = bridge.createRuntimeFromModel(model, runtimeConfig);

        // Convert to creation result
        return createFromBridgeResult(bridgeResult, options);

    } catch (const std::exception &e) {
        CreationResult result;
        result.success = false;
        result.errorMessage = "Exception creating state machine: " + std::string(e.what());
        return result;
    }
}

std::shared_ptr<Processor> StateMachineFactory::quickStart(const std::string &filePath,
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

std::shared_ptr<Processor> StateMachineFactory::quickStartFromContent(const std::string &scxmlContent,
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

}  // namespace Runtime
}  // namespace SCXML