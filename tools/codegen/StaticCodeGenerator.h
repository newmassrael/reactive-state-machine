// SCXML to C++ static code generator
#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace RSM::Codegen {

// Forward declaration
class SCXMLModel;

/**
 * @brief SCXML to C++ static code generator
 *
 * Generates compile-time optimized C++ state machines from SCXML files.
 * Features:
 * - Type-safe State and Event enums
 * - Template-based state machine with zero overhead
 * - Strategy pattern support for user logic injection
 */
class StaticCodeGenerator {
public:
    StaticCodeGenerator() = default;
    ~StaticCodeGenerator() = default;

    /**
     * @brief Generate C++ code from SCXML file
     * @param scxmlPath Path to SCXML file
     * @param outputDir Output directory
     * @return Success status
     */
    bool generate(const std::string &scxmlPath, const std::string &outputDir);

    // Individual generation methods (public for testability)
    std::string generateStateEnum(const std::set<std::string> &states);
    std::string generateEventEnum(const std::set<std::string> &events);
    std::string generateProcessEvent(const std::string &className);
    std::string generateClass(const std::string &className, const std::string &initialState);

private:
    // Template method to generate enums (DRY principle)
    std::string generateEnum(const std::string &enumName, const std::set<std::string> &values);
    // Helper methods
    std::string capitalize(const std::string &str);
    std::string sanitizeName(const std::string &name);

    // Extract SCXML parsing results
    std::set<std::string> extractStates(const SCXMLModel &model);
    std::set<std::string> extractEvents(const SCXMLModel &model);

    // File writing
    bool writeToFile(const std::string &path, const std::string &content);
};

// Simplified model for code generation (extracted from RSM::SCXMLModel)
class SCXMLModel {
public:
    std::string name;
    std::string initial;
    std::vector<std::string> states;
    std::vector<std::pair<std::string, std::string>> transitions;  // (event, target)
};

}  // namespace RSM::Codegen