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
class IActionNode;
}  // namespace RSM

namespace RSM::Codegen {

// Forward declarations
class SCXMLModel;
struct Action;
struct Transition;

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
    std::string generateProcessEvent(const SCXMLModel &model, const std::set<std::string> &events);
    std::string generateClass(const SCXMLModel &model);
    void generateActionCode(std::stringstream &ss, const Action &action, const std::string &engineVar,
                            const std::set<std::string> &events);
    static std::string capitalizePublic(const std::string &str);

    // Extract Guard and Action functions from SCXML
    std::set<std::string> extractGuards(const std::string &scxmlPath);
    std::set<std::string> extractActions(const std::string &scxmlPath);

private:
    // Template method to generate enums (DRY principle)
    std::string generateEnum(const std::string &enumName, const std::set<std::string> &values);
    // Helper methods
    std::string capitalize(const std::string &str);

    // W3C SCXML 3.5.1: Group transitions by event while preserving document order
    // This ensures the first occurrence of each event determines its position in output
    static std::vector<std::pair<std::string, std::vector<Transition>>>
    groupTransitionsByEventPreservingOrder(const std::vector<Transition> &transitions);

    // Extract SCXML parsing results
    // Convert Action to JavaScript code (for foreach iteration content)
    std::string actionToJavaScript(const std::vector<Action> &actions);

    std::set<std::string> extractStates(const SCXMLModel &model);
    std::set<std::string> extractEvents(const SCXMLModel &model);

    // Internal overloads for extraction (avoid duplicate parsing)
    std::set<std::string> extractGuardsInternal(const std::shared_ptr<RSM::SCXMLModel> &rsmModel);
    std::set<std::string> extractActionsInternal(const std::shared_ptr<RSM::SCXMLModel> &rsmModel);

    // Helper for regex-based function name extraction
    std::set<std::string> extractFunctionNames(const std::string &text, const std::regex &pattern);

    // Helper to process actions recursively (for if/elseif/else)
    std::vector<Action> processActions(const std::vector<std::shared_ptr<RSM::IActionNode>> &actionNodes);

    // Security: Escape string for safe code generation
    std::string escapeStringLiteral(const std::string &str);

    // File writing
    bool writeToFile(const std::string &path, const std::string &content);
};

// Simplified model for code generation (extracted from RSM::SCXMLModel)
struct Transition {
    std::string sourceState;
    std::string event;
    std::string targetState;
    std::string guard;                      // Guard condition expression (e.g., "isReady()")
    std::vector<std::string> actions;       // Action function names from transition scripts
    std::vector<Action> transitionActions;  // W3C SCXML: Transition actions (e.g., assign for internal transitions)
};

;

// Forward declaration for nested actions
struct Action;

// Conditional branch for if/elseif/else
struct ConditionalBranch {
    std::string condition;        // Boolean expression (empty for else)
    std::vector<Action> actions;  // Actions to execute in this branch
    bool isElseBranch = false;    // true for else branch

    ConditionalBranch() = default;

    ConditionalBranch(const std::string &cond, bool isElse = false) : condition(cond), isElseBranch(isElse) {}
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
        FOREACH  // <foreach array="arr" item="item" index="idx">...</foreach>
    };

    Type type;
    std::string param1;  // event name for RAISE, array for FOREACH, script content for SCRIPT, condition for IF, etc.
    std::string param2;  // additional parameter (e.g., assign location, item var for FOREACH)
    std::string param3;  // third parameter (e.g., index var for FOREACH)
    std::vector<ConditionalBranch> branches;  // For IF action: if/elseif/else branches
    std::vector<Action> iterationActions;     // For FOREACH: actions to execute in loop

    Action(Type t, const std::string &p1 = "", const std::string &p2 = "", const std::string &p3 = "")
        : type(t), param1(p1), param2(p2), param3(p3) {}
};

struct State {
    std::string name;
    bool isFinal = false;
    bool isParallel = false;                // W3C SCXML 3.4: Parallel state flag
    std::vector<std::string> childRegions;  // W3C SCXML 3.4: Child region state IDs for parallel states
    std::vector<Action> entryActions;       // Actions from <onentry>
    std::vector<Action> exitActions;        // Actions from <onexit>
};

struct DataModelVariable {
    std::string name;
    std::string initialValue;  // Initial expression (e.g., "0", "'hello'")
    std::string type;          // Variable type hint (optional)
};

class SCXMLModel {
public:
    std::string name;
    std::string initial;
    std::vector<State> states;
    std::vector<Transition> transitions;
    std::vector<DataModelVariable> dataModel;  // Data model variables

    // Feature detection flags for hybrid code generation
    bool hasForEach = false;            // Uses <foreach> action
    bool hasComplexDatamodel = false;   // Uses arrays, typeof, dynamic variables
    bool hasComplexECMAScript = false;  // Needs JSEngine for evaluation

    // Helper: Determine if JSEngine is needed
    bool needsJSEngine() const {
        return hasForEach || hasComplexDatamodel || hasComplexECMAScript;
    }
};

}  // namespace RSM::Codegen