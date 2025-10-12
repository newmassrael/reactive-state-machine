// SCXML to C++ static code generator
#pragma once

#include <memory>
#include <regex>
#include <set>
#include <string>
#include <vector>

// Forward declarations from RSM namespace
namespace RSM {
class SCXMLModel;
}

namespace RSM::Codegen {

// Forward declarations
class SCXMLModel;
struct Action;

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
    std::string generateStrategyInterface(const std::string &className, const std::set<std::string> &guards,
                                          const std::set<std::string> &actions);
    std::string generateProcessEvent(const SCXMLModel &model);
    std::string generateClass(const SCXMLModel &model);
    void generateActionCode(std::stringstream &ss, const Action &action, const std::string &engineVar);
    static std::string capitalizePublic(const std::string &str);

    // Extract Guard and Action functions from SCXML
    std::set<std::string> extractGuards(const std::string &scxmlPath);
    std::set<std::string> extractActions(const std::string &scxmlPath);

private:
    // Template method to generate enums (DRY principle)
    std::string generateEnum(const std::string &enumName, const std::set<std::string> &values);
    // Helper methods
    std::string capitalize(const std::string &str);

    // Extract SCXML parsing results
    std::set<std::string> extractStates(const SCXMLModel &model);
    std::set<std::string> extractEvents(const SCXMLModel &model);

    // Internal overloads for extraction (avoid duplicate parsing)
    std::set<std::string> extractGuardsInternal(const std::shared_ptr<RSM::SCXMLModel> &rsmModel);
    std::set<std::string> extractActionsInternal(const std::shared_ptr<RSM::SCXMLModel> &rsmModel);

    // Helper for regex-based function name extraction
    std::set<std::string> extractFunctionNames(const std::string &text, const std::regex &pattern);

    // File writing
    bool writeToFile(const std::string &path, const std::string &content);
};

// Simplified model for code generation (extracted from RSM::SCXMLModel)
struct Transition {
    std::string sourceState;
    std::string event;
    std::string targetState;
    std::string guard;                 // Guard condition expression (e.g., "isReady()")
    std::vector<std::string> actions;  // Action function names from transition scripts
};

// Executable content action representation
struct Action {
    enum Type {
        RAISE,   // <raise event="name"/>
        SCRIPT,  // <script>code</script>
        ASSIGN,  // <assign location="var" expr="value"/>
        LOG,     // <log expr="message"/>
        SEND,    // <send event="name"/>
        IF,      // <if cond="expr">...</if>
        FOREACH  // <foreach>...</foreach>
    };

    Type type;
    std::string param1;  // event name for RAISE, script content for SCRIPT, etc.
    std::string param2;  // additional parameter (e.g., assign location)

    Action(Type t, const std::string &p1 = "", const std::string &p2 = "") : type(t), param1(p1), param2(p2) {}
};

struct State {
    std::string name;
    bool isFinal = false;
    std::vector<Action> entryActions;  // Actions from <onentry>
    std::vector<Action> exitActions;   // Actions from <onexit>
};

class SCXMLModel {
public:
    std::string name;
    std::string initial;
    std::vector<State> states;  // Changed from vector<string> to vector<State>
    std::vector<Transition> transitions;
};

}  // namespace RSM::Codegen