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

    // Static invoke info for member generation
    struct StaticInvokeInfo {
        std::string invokeId;           // Unique invoke ID
        std::string childName;          // Child class name (e.g., test239sub1)
        std::string stateName;          // Parent state containing this invoke
        bool autoforward = false;       // W3C SCXML 6.4.1: Autoforward flag
        std::string finalizeContent;    // W3C SCXML 6.5: Finalize handler script
        bool childNeedsParent = false;  // W3C SCXML 6.2: Child uses #_parent
    };

    std::string generateProcessEvent(const SCXMLModel &model, const std::set<std::string> &events,
                                     const std::vector<StaticInvokeInfo> &staticInvokes = {});

    std::string generateClass(const SCXMLModel &model, const std::vector<StaticInvokeInfo> &staticInvokes = {});
    void generateActionCode(std::stringstream &ss, const Action &action, const std::string &engineVar,
                            const std::set<std::string> &events, const SCXMLModel &model);
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

    // W3C SCXML 3.3: Generate parent state mapping for hierarchical entry
    std::string generateGetParentMethod(const SCXMLModel &model);

    // File writing
    bool writeToFile(const std::string &path, const std::string &content);

    // W3C SCXML 6.4: Generate Interpreter wrapper for dynamic invoke fallback
    // ARCHITECTURE.md: No hybrid approach - entire SCXML uses Interpreter when dynamic invoke detected
    bool generateInterpreterWrapper(std::stringstream &ss, const SCXMLModel &model,
                                    std::shared_ptr<RSM::SCXMLModel> rsmModel, const std::string &scxmlPath,
                                    const std::string &outputDir);
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
        RAISE,    // <raise event="name"/>
        SCRIPT,   // <script>code</script>
        ASSIGN,   // <assign location="var" expr="value"/>
        LOG,      // <log expr="message"/>
        SEND,     // <send event="name"/>
        IF,       // <if cond="expr">...</if>
        FOREACH,  // <foreach array="arr" item="item" index="idx">...</foreach>
        CANCEL    // <cancel sendid="id"/> or <cancel sendidexpr="expr"/> (W3C SCXML 6.3)
    };

    Type type;
    std::string param1;  // event name for RAISE, array for FOREACH, script content for SCRIPT, condition for IF, sendid
                         // for CANCEL, etc.
    std::string param2;  // additional parameter (e.g., assign location, item var for FOREACH, sendidexpr for CANCEL)
    std::string param3;  // third parameter (e.g., index var for FOREACH, targetExpr for SEND)
    std::string param4;  // fourth parameter (e.g., eventExpr for SEND)
    std::string param5;  // fifth parameter (e.g., delay for SEND)
    std::string param6;  // sixth parameter (e.g., delayexpr for SEND)
    std::vector<ConditionalBranch> branches;                      // For IF action: if/elseif/else branches
    std::vector<Action> iterationActions;                         // For FOREACH: actions to execute in loop
    std::vector<std::pair<std::string, std::string>> sendParams;  // For SEND: param name->expr pairs (W3C 5.10)
    std::string sendContent;                                      // For SEND: <content> literal (W3C 5.10, test179)
    std::string sendContentExpr;  // For SEND: <content expr="..."> dynamic content (W3C 5.10)
    std::string sendId;           // For SEND: id attribute for event tracking/cancellation (W3C 6.2.5, test208)
    std::string sendIdLocation;   // For SEND: idlocation variable to store sendid (W3C 6.2.4, test183)
    std::string sendType;         // For SEND: type attribute for event processor (W3C 6.2.4, test193)

    Action(Type t, const std::string &p1 = "", const std::string &p2 = "", const std::string &p3 = "",
           const std::string &p4 = "", const std::string &p5 = "", const std::string &p6 = "")
        : type(t), param1(p1), param2(p2), param3(p3), param4(p4), param5(p5), param6(p6) {}
};

// W3C SCXML 6.4: Invoke information for JIT code generation
struct InvokeInfo {
    std::string invokeId;         // Invoke ID (generated or specified)
    std::string type;             // Invoke type (e.g., "scxml", "http")
    std::string src;              // SCXML file path or URL
    std::string srcExpr;          // Dynamic src expression
    bool autoforward = false;     // W3C SCXML 6.4.1: Autoforward flag
    std::string finalizeContent;  // W3C SCXML 6.5: Finalize handler XML content
    std::string namelist;         // Variable names to pass to child
    std::string content;          // Inline SCXML content
    std::string contentExpr;      // Dynamic content expression
    // param: name->expr/location mapping for data passing
    std::vector<std::tuple<std::string, std::string, std::string>> params;
};

struct State {
    std::string name;
    std::string parentState;  // W3C SCXML 3.3: Parent state (empty for root states)
    bool isFinal = false;
    bool isParallel = false;                // W3C SCXML 3.4: Parallel state flag
    std::vector<std::string> childRegions;  // W3C SCXML 3.4: Child region state IDs for parallel states
    std::vector<Action> entryActions;       // Actions from <onentry>
    std::vector<Action> exitActions;        // Actions from <onexit>
    std::vector<InvokeInfo> invokes;        // W3C SCXML 6.4: Invoke elements in this state
};

struct DataModelVariable {
    std::string name;
    std::string initialValue;  // Initial expression (e.g., "0", "'hello'")
    std::string type;          // Variable type hint (optional)
    std::string stateName;     // W3C SCXML 5.3: State name for late binding (empty = root datamodel)
};

class SCXMLModel {
public:
    std::string name;
    std::string initial;
    std::string bindingMode;  // W3C SCXML 5.3: "early" or "late" (default "early")
    std::vector<State> states;
    std::vector<Transition> transitions;
    std::vector<DataModelVariable> dataModel;  // Data model variables

    // Feature detection flags for hybrid code generation
    bool hasForEach = false;            // Uses <foreach> action
    bool hasComplexDatamodel = false;   // Uses arrays, typeof, dynamic variables
    bool hasComplexECMAScript = false;  // Needs JSEngine for evaluation
    bool hasSend = false;               // Uses <send> action
    bool hasSendWithDelay = false;      // Uses <send delay> or <send delayexpr> (W3C SCXML 6.2)
    bool hasSendParams = false;         // Uses <send> with <param> elements (event data)
    bool hasSendToParent = false;       // Uses <send target="#_parent"> (W3C SCXML 6.2)

    // Helper: Determine if JSEngine is needed
    bool needsJSEngine() const {
        return hasForEach || hasComplexDatamodel || hasComplexECMAScript;
    }

    // Helper: Determine if EventScheduler is needed (W3C SCXML 6.2)
    bool needsEventScheduler() const {
        return hasSendWithDelay;
    }

    // Helper: Check if any state has invoke elements
    bool hasInvoke() const {
        for (const auto &state : states) {
            if (!state.invokes.empty()) {
                return true;
            }
        }
        return false;
    }

    // Helper: Determine if stateful Policy is needed (ARCHITECTURE.md)
    bool needsStatefulPolicy() const {
        return needsJSEngine() || needsEventScheduler() || hasSendParams || !dataModel.empty() || hasInvoke();
    }
};

}  // namespace RSM::Codegen