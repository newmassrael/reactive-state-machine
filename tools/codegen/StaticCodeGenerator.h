// Minimal static code generator - TDD RED phase
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
 * TDD approach:
 * 1. Start with minimal features (State/Event enums only)
 * 2. Add features progressively
 * 3. Pass tests at each stage
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
    std::string generateClass(const std::string &className);

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

// Temporary model class (before connecting actual SCXMLParser)
class SCXMLModel {
public:
    std::string name;
    std::string initial;
    std::vector<std::string> states;
    std::vector<std::pair<std::string, std::string>> transitions;  // event, target
};

}  // namespace RSM::Codegen