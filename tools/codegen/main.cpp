#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

#include "StaticCodeGenerator.h"
#include "common/Logger.h"

namespace fs = std::filesystem;

void printUsage(const char *programName) {
    LOG_INFO("SCXML Static Code Generator");
    LOG_INFO("Generates zero-overhead C++ state machine code from SCXML\n");
    LOG_INFO("Usage: {} [options] <input.scxml>", programName);
    LOG_INFO("\nOptions:");
    LOG_INFO("  -o, --output <dir>     Output directory (default: current directory)");
    LOG_INFO("  -h, --help             Show this help message");
    LOG_INFO("  -v, --verbose          Enable verbose logging");
    LOG_INFO("  --version              Show version information\n");
    LOG_INFO("Examples:");
    LOG_INFO("  {} thermostat.scxml", programName);
    LOG_INFO("  {} -o generated/ robot.scxml", programName);
    LOG_INFO("  {} --output=include/ state_machine.scxml", programName);
    LOG_INFO("\nOutput:");
    LOG_INFO("  Generates <StateMachineName>_sm.h in the output directory");
    LOG_INFO("  Inherit from generated base class to implement your logic");
}

void printVersion() {
    LOG_INFO("scxml-codegen version 1.0.0");
    LOG_INFO("Static SCXML-to-C++ Code Generator");
    LOG_INFO("Zero-overhead compile-time state machines");
}

int main(int argc, char *argv[]) {
    std::string inputFile;
    std::string outputDir = ".";
    bool verbose = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "--version") {
            printVersion();
            return 0;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                outputDir = argv[++i];
            } else {
                LOG_ERROR("Error: --output requires a directory path");
                return 1;
            }
        } else if (arg.starts_with("--output=")) {
            outputDir = arg.substr(9);
        } else if (arg.starts_with("-")) {
            LOG_ERROR("Error: Unknown option {}", arg);
            printUsage(argv[0]);
            return 1;
        } else {
            if (inputFile.empty()) {
                inputFile = arg;
            } else {
                LOG_ERROR("Error: Multiple input files specified");
                return 1;
            }
        }
    }

    // Validate arguments
    if (inputFile.empty()) {
        LOG_ERROR("Error: No input file specified");
        printUsage(argv[0]);
        return 1;
    }

    if (!fs::exists(inputFile)) {
        LOG_ERROR("Error: Input file '{}' does not exist", inputFile);
        return 1;
    }

    // Create output directory if it doesn't exist
    if (!fs::exists(outputDir)) {
        try {
            fs::create_directories(outputDir);
            if (verbose) {
                LOG_INFO("Created output directory: {}", outputDir);
            }
        } catch (const std::exception &e) {
            LOG_ERROR("Error: Cannot create output directory '{}': {}", outputDir, e.what());
            return 1;
        }
    }

    if (!fs::is_directory(outputDir)) {
        LOG_ERROR("Error: Output path '{}' is not a directory", outputDir);
        return 1;
    }

    if (verbose) {
        LOG_INFO("Verbose mode enabled");
    }

    try {
        LOG_INFO("Starting SCXML static code generation...");
        LOG_INFO("Input file: {}", inputFile);
        LOG_INFO("Output directory: {}", outputDir);

        // Generate code using StaticCodeGenerator
        RSM::Codegen::StaticCodeGenerator generator;
        bool success = generator.generate(inputFile, outputDir);

        if (!success) {
            LOG_ERROR("Error: Code generation failed");
            return 1;
        }

        LOG_INFO("Code generation completed successfully");

        // Get the expected output file name
        auto scxmlPath = fs::path(inputFile);
        std::string baseName = scxmlPath.stem().string();
        std::string outputFile = (fs::path(outputDir) / (baseName + "_sm.h")).string();

        if (fs::exists(outputFile)) {
            LOG_INFO("Generated: {}", outputFile);
            LOG_INFO("\nNext steps:");
            LOG_INFO("  1. Include the generated header: #include \"{}\"", fs::path(outputFile).filename().string());
            LOG_INFO("  2. Inherit from base class: class MyLogic : public {}Base<MyLogic> {{}}", baseName);
            LOG_INFO("  3. Implement required guard/action methods");
            LOG_INFO("  4. Call sm.initialize() to start the state machine");
        }

        return 0;

    } catch (const std::exception &e) {
        LOG_ERROR("Error: {}", e.what());
        return 1;
    }
}
