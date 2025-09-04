#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/IStateMachine.h"
#include "model/DocumentModel.h"

namespace SCXML {

namespace Model {
class DocumentModel;
}
namespace CodeGen {

/**
 * @brief SCXML Code Generator - Generates C++ code from SCXML models
 *
 * This class generates high-performance C++ code from parsed SCXML models.
 * The generated code implements the IStateMachine interface and provides
 * compile-time optimized state machine execution.
 */
class CodeGenerator {
public:
    /**
     * @brief Code generation options
     */
    struct GenerationOptions {
        std::string className = "StateMachine";        // Generated class name
        std::string namespaceName = "";                // Target namespace
        std::string outputDirectory = "./generated";   // Output directory
        bool generateHeader = true;                    // Generate .h file
        bool generateImplementation = true;            // Generate .cpp file
        bool generateInterface = true;                 // Generate abstract interface
        bool enableOptimizations = true;               // Enable performance optimizations
        bool enableReactiveGuards = true;              // Enable reactive guard system
        bool enableDependencyInjection = true;         // Enable dependency injection
        bool enableTypeHelpers = true;                 // Generate type helper functions
        bool enableStatisticsTracking = true;          // Enable performance statistics
        bool generateComments = true;                  // Add code comments
        bool generateDebugInfo = false;                // Add debug information
        std::string headerGuard = "";                  // Custom header guard (auto if empty)
        std::vector<std::string> additionalIncludes;   // Additional #include files
        std::vector<std::string> forwardDeclarations;  // Forward declarations

        // ========== Generation Options ==========
        // Note: All code generation uses standard patterns - no legacy options needed
    };

    /**
     * @brief Generated code result
     */
    struct GenerationResult {
        bool success = false;
        std::string headerCode;                   // Generated header code
        std::string implementationCode;           // Generated implementation code
        std::string interfaceCode;                // Generated interface code
        std::string errorMessage;                 // Error message if failed
        std::vector<std::string> warnings;        // Warning messages
        std::vector<std::string> generatedFiles;  // List of generated file paths

        // Statistics
        size_t linesOfCode = 0;          // Total lines generated
        size_t numberOfStates = 0;       // Number of states processed
        size_t numberOfTransitions = 0;  // Number of transitions processed
        size_t numberOfActions = 0;      // Number of actions processed
    };

public:
    /**
     * @brief Constructor
     */
    CodeGenerator();

    /**
     * @brief Destructor
     */
    ~CodeGenerator();

    // ========== Core Generation Methods ==========

    /**
     * @brief Generate code from SCXML model
     * @param model SCXML model to generate from
     * @param options Generation options
     * @return Generation result
     */
    GenerationResult generate(std::shared_ptr<::SCXML::Model::DocumentModel> model, const GenerationOptions &options);

    /**
     * @brief Generate code from SCXML file
     * @param scxmlFilePath Path to SCXML file
     * @param options Generation options
     * @return Generation result
     */
    GenerationResult generateFromFile(const std::string &scxmlFilePath, const GenerationOptions &options);

    /**
     * @brief Generate code from SCXML content
     * @param scxmlContent SCXML content string
     * @param options Generation options
     * @return Generation result
     */
    GenerationResult generateFromContent(const std::string &scxmlContent, const GenerationOptions &options);

    // ========== Output Methods ==========

    /**
     * @brief Write generated code to files
     * @param result Generation result
     * @param options Generation options
     * @return true if files written successfully
     */
    bool writeToFiles(const GenerationResult &result, const GenerationOptions &options);

    /**
     * @brief Get generated header file path
     * @param options Generation options
     * @return Header file path
     */
    std::string getHeaderFilePath(const GenerationOptions &options) const;

    /**
     * @brief Get generated implementation file path
     * @param options Generation options
     * @return Implementation file path
     */
    std::string getImplementationFilePath(const GenerationOptions &options) const;

    /**
     * @brief Get generated interface file path
     * @param options Generation options
     * @return Interface file path
     */
    std::string getInterfaceFilePath(const GenerationOptions &options) const;

    // ========== Template Methods ==========

    /**
     * @brief Set custom code template
     * @param templateName Template name (header, implementation, interface)
     * @param templateContent Template content with placeholders
     */
    void setTemplate(const std::string &templateName, const std::string &templateContent);

    /**
     * @brief Get available template names
     * @return Vector of template names
     */
    std::vector<std::string> getAvailableTemplates() const;

    /**
     * @brief Reset templates to default
     */
    void resetTemplates();

    // ========== Validation ==========

    /**
     * @brief Validate model for code generation
     * @param model SCXML model to validate
     * @return Vector of validation errors (empty if valid)
     */
    std::vector<std::string> validateModel(std::shared_ptr<::SCXML::Model::DocumentModel> model) const;

    /**
     * @brief Check if model is suitable for code generation
     * @param model SCXML model to check
     * @return true if model can be used for code generation
     */
    bool isModelValid(std::shared_ptr<::SCXML::Model::DocumentModel> model) const;

private:
    // ========== Code Generation Implementation ==========

    /**
     * @brief Generate header code
     * @param model SCXML model
     * @param options Generation options
     * @return Generated header code
     */
    std::string generateHeader(std::shared_ptr<::SCXML::Model::DocumentModel> model, const GenerationOptions &options);

    /**
     * @brief Generate implementation code
     * @param model SCXML model
     * @param options Generation options
     * @return Generated implementation code
     */
    std::string generateImplementation(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                       const GenerationOptions &options);

    /**
     * @brief Generate interface code
     * @param model SCXML model
     * @param options Generation options
     * @return Generated interface code
     */
    std::string generateInterface(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                  const GenerationOptions &options);

    // ========== Additional Generation Methods ==========

    /**
     * @brief Generate simple header for testing
     * @param model SCXML model
     * @param options Generation options
     * @return Generated simple header code
     */
    std::string generateSimpleHeader(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                     const GenerationOptions &options);

    /**
     * @brief Generate simple implementation for testing
     * @param model SCXML model
     * @param options Generation options
     * @return Generated simple implementation code
     */
    std::string generateSimpleImplementation(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                             const GenerationOptions &options);

    // ========== Component Generators ==========

    /**
     * @brief Generate state enumeration
     * @param model SCXML model
     * @param options Generation options
     * @return State enum code
     */
    std::string generateStateEnum(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                  const GenerationOptions &options);

    /**
     * @brief Generate event enumeration
     * @param model SCXML model
     * @param options Generation options
     * @return Event enum code
     */
    std::string generateEventEnum(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                  const GenerationOptions &options);

    /**
     * @brief Generate state transition methods
     * @param model SCXML model
     * @param options Generation options
     * @return Transition methods code
     */
    std::string generateTransitionMethods(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                          const GenerationOptions &options);

    /**
     * @brief Generate guard condition methods
     * @param model SCXML model
     * @param options Generation options
     * @return Guard methods code
     */
    std::string generateGuardMethods(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                     const GenerationOptions &options);

    /**
     * @brief Generate action methods
     * @param model SCXML model
     * @param options Generation options
     * @return Action methods code
     */
    std::string generateActionMethods(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                      const GenerationOptions &options);

    /**
     * @brief Generate reactive guard system
     * @param model SCXML model
     * @param options Generation options
     * @return Reactive guard code
     */
    std::string generateReactiveGuards(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                       const GenerationOptions &options);

    /**
     * @brief Generate dependency injection infrastructure
     * @param model SCXML model
     * @param options Generation options
     * @return DI infrastructure code
     */
    std::string generateDependencyInjection(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                            const GenerationOptions &options);

    // ========== Utility Methods ==========

    /**
     * @brief Convert identifier to C++ compatible name
     * @param identifier Original identifier
     * @return C++ compatible identifier
     */
    std::string toCppIdentifier(const std::string &identifier) const;

    /**
     * @brief Generate header guard
     * @param className Class name
     * @param namespaceName Namespace name
     * @return Header guard string
     */
    std::string generateHeaderGuard(const std::string &className, const std::string &namespaceName) const;

    /**
     * @brief Add indentation to code
     * @param code Code to indent
     * @param level Indentation level
     * @return Indented code
     */
    std::string addIndentation(const std::string &code, int level = 1) const;

    /**
     * @brief Process template with placeholders
     * @param templateStr Template string
     * @param placeholders Placeholder values
     * @return Processed template
     */
    std::string processTemplate(const std::string &templateStr,
                                const std::unordered_map<std::string, std::string> &placeholders) const;

private:
    std::unordered_map<std::string, std::string> templates_;  // Code templates
    std::vector<std::string> errorMessages_;                  // Error messages
    std::vector<std::string> warningMessages_;                // Warning messages
};

// ========== Convenience Functions ==========

/**
 * @brief Quick code generation from SCXML file
 * @param scxmlFilePath Path to SCXML file
 * @param className Generated class name
 * @param outputDir Output directory
 * @return true if generation successful
 */
bool generateStateMachineCode(const std::string &scxmlFilePath, const std::string &className = "StateMachine",
                              const std::string &outputDir = "./generated");

/**
 * @brief Generate code with default options
 * @param model SCXML model
 * @param className Generated class name
 * @return Generation result
 */
CodeGenerator::GenerationResult quickGenerate(std::shared_ptr<::SCXML::Model::DocumentModel> model,
                                              const std::string &className = "StateMachine");

}  // namespace CodeGen
}  // namespace SCXML

// ========== Compatibility Support ==========
// Provide backward compatibility for existing code
using CodeGenerator = SCXML::CodeGen::CodeGenerator;