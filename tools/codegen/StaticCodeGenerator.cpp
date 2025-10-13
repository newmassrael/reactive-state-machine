// Static code generator with SCXML parser integration
#include "StaticCodeGenerator.h"
#include "common/Logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <sstream>

#include "actions/AssignAction.h"
#include "actions/ForeachAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "factory/NodeFactory.h"
#include "model/SCXMLModel.h"
#include "parsing/SCXMLParser.h"

namespace fs = std::filesystem;

namespace RSM::Codegen {

bool StaticCodeGenerator::generate(const std::string &scxmlPath, const std::string &outputDir) {
    // Step 1: Validate input
    if (scxmlPath.empty()) {
        LOG_ERROR("StaticCodeGenerator: SCXML path is empty");
        return false;
    }

    if (!fs::exists(scxmlPath)) {
        LOG_ERROR("StaticCodeGenerator: SCXML file does not exist: {}", scxmlPath);
        return false;
    }

    // Step 2: Parse SCXML file using actual parser
    auto nodeFactory = std::make_shared<RSM::NodeFactory>();
    RSM::SCXMLParser parser(nodeFactory);

    LOG_DEBUG("StaticCodeGenerator: Parsing SCXML file: {}", scxmlPath);
    auto rsmModel = parser.parseFile(scxmlPath);
    if (!rsmModel) {
        LOG_ERROR("StaticCodeGenerator: Failed to parse SCXML file: {}", scxmlPath);
        return false;
    }

    // Step 3: Validate parsed model
    if (rsmModel->getName().empty()) {
        LOG_ERROR("StaticCodeGenerator: SCXML model has no name");
        return false;
    }

    // Step 4: Convert RSM::SCXMLModel to simplified format for code generation
    SCXMLModel model;
    model.name = rsmModel->getName();
    model.initial = rsmModel->getInitialState();

    if (model.initial.empty()) {
        LOG_ERROR("StaticCodeGenerator: SCXML model '{}' has no initial state", model.name);
        return false;
    }

    // Extract datamodel variables
    for (const auto &dataItem : rsmModel->getDataModelItems()) {
        DataModelVariable var;
        var.name = dataItem->getId();
        // Try expr attribute first, fallback to content (text between tags)
        var.initialValue = dataItem->getExpr();
        if (var.initialValue.empty()) {
            var.initialValue = dataItem->getContent();
            // Trim whitespace from content (important for inline array/object literals)
            auto start = var.initialValue.find_first_not_of(" \t\n\r");
            auto end = var.initialValue.find_last_not_of(" \t\n\r");
            if (start != std::string::npos && end != std::string::npos) {
                var.initialValue = var.initialValue.substr(start, end - start + 1);
            }
        }
        model.dataModel.push_back(var);
    }

    // Extract all states
    auto allStates = rsmModel->getAllStates();
    if (allStates.empty()) {
        LOG_ERROR("StaticCodeGenerator: SCXML model '{}' has no states", model.name);
        return false;
    }

    static const std::regex funcRegex(R"((\w+)\(\))");

    std::set<std::string> processedStates;

    for (const auto &state : allStates) {
        std::string stateId = state->getId();

        // Skip if already processed (getAllStates may return duplicates)
        if (processedStates.find(stateId) != processedStates.end()) {
            continue;
        }
        processedStates.insert(stateId);

        // Create State with entry/exit actions
        State stateInfo;
        stateInfo.name = stateId;
        stateInfo.isFinal = state->isFinalState();

        // Extract entry actions
        for (const auto &actionBlock : state->getEntryActionBlocks()) {
            auto actions = processActions(actionBlock);
            stateInfo.entryActions.insert(stateInfo.entryActions.end(), actions.begin(), actions.end());
        }

        // Extract exit actions
        for (const auto &actionBlock : state->getExitActionBlocks()) {
            auto actions = processActions(actionBlock);
            stateInfo.exitActions.insert(stateInfo.exitActions.end(), actions.begin(), actions.end());
        }

        model.states.push_back(stateInfo);

        // Extract transitions from each state
        auto transitions = state->getTransitions();
        for (const auto &transition : transitions) {
            auto event = transition->getEvent();
            auto targets = transition->getTargets();

            // Accept transitions with targets (event-based or eventless)
            if (!targets.empty()) {
                // Use first target (SCXML supports multiple targets)
                Transition trans;
                trans.sourceState = state->getId();
                trans.event = event;  // May be empty for eventless transitions
                trans.targetState = targets[0];
                trans.guard = transition->getGuard();  // Extract guard condition

                // Extract actions from transition
                for (const auto &actionNode : transition->getActionNodes()) {
                    if (auto scriptAction = std::dynamic_pointer_cast<RSM::ScriptAction>(actionNode)) {
                        const std::string &content = scriptAction->getContent();
                        auto extractedFuncs = extractFunctionNames(content, funcRegex);
                        trans.actions.insert(trans.actions.end(), extractedFuncs.begin(), extractedFuncs.end());
                    }
                }

                model.transitions.push_back(trans);
            }
        }
    }

    // Step 5: Feature detection for hybrid generation
    std::function<void(const std::vector<Action> &)> detectFeatures;
    detectFeatures = [&](const std::vector<Action> &actions) {
        for (const auto &action : actions) {
            if (action.type == Action::FOREACH) {
                model.hasForEach = true;
            } else if (action.type == Action::IF) {
                for (const auto &branch : action.branches) {
                    detectFeatures(branch.actions);
                }
            } else if (action.type == Action::FOREACH) {
                detectFeatures(action.iterationActions);
            }
        }
    };

    // Detect features in all states
    for (const auto &state : model.states) {
        detectFeatures(state.entryActions);
        detectFeatures(state.exitActions);
    }

    // Detect complex datamodel (arrays, typeof)
    for (const auto &var : model.dataModel) {
        if (var.initialValue.find('[') != std::string::npos || var.initialValue.find('{') != std::string::npos) {
            model.hasComplexDatamodel = true;
        }
    }

    // Detect typeof in guards
    for (const auto &trans : model.transitions) {
        if (trans.guard.find("typeof") != std::string::npos) {
            model.hasComplexDatamodel = true;
        }
    }

    LOG_INFO("StaticCodeGenerator: Feature detection - forEach: {}, complexDatamodel: {}, needsJSEngine: {}",
             model.hasForEach, model.hasComplexDatamodel, model.needsJSEngine());

    // Step 6: Extract unique states and events
    auto states = extractStates(model);
    auto events = extractEvents(model);

    // Validate we have states (events can be empty for stateless machines)
    if (states.empty()) {
        LOG_ERROR("StaticCodeGenerator: No states extracted from model '{}'", model.name);
        return false;
    }

    // Step 5b: Extract guards and actions
    auto guards = extractGuardsInternal(rsmModel);
    auto actions = extractActionsInternal(rsmModel);

    LOG_INFO("StaticCodeGenerator: Generating code for '{}' with {} states, {} events, {} guards, {} actions",
             model.name, states.size(), events.size(), guards.size(), actions.size());

    // Step 6: Generate code
    std::stringstream ss;

    // Header guard
    ss << "#pragma once\n";
    ss << "#include <cstdint>\n";
    ss << "#include <memory>\n";
    ss << "#include <stdexcept>\n";
    ss << "#include \"static/StaticExecutionEngine.h\"\n";

    // Add JSEngine and Logger includes if needed for hybrid generation (OUTSIDE namespace)
    if (model.needsJSEngine()) {
        ss << "#include <optional>\n";
        ss << "#include \"common/Logger.h\"\n";
        ss << "#include \"scripting/JSEngine.h\"\n";
    }
    ss << "\n";

    // Namespace - each test gets its own nested namespace to avoid conflicts
    ss << "namespace RSM::Generated::" << model.name << " {\n\n";

    // Generate State enum
    ss << generateStateEnum(states);
    ss << "\n";

    // Generate Event enum
    ss << generateEventEnum(events);
    ss << "\n";

    // Generate base class template
    ss << generateClass(model);

    ss << "\n} // namespace RSM::Generated::" << model.name << "\n";

    // Step 7: Validate output directory and write to file
    if (outputDir.empty()) {
        LOG_ERROR("StaticCodeGenerator: Output directory is empty");
        return false;
    }

    if (!fs::exists(outputDir)) {
        LOG_ERROR("StaticCodeGenerator: Output directory does not exist: {}", outputDir);
        return false;
    }

    if (!fs::is_directory(outputDir)) {
        LOG_ERROR("StaticCodeGenerator: Output path is not a directory: {}", outputDir);
        return false;
    }

    std::string outputPath = outputDir + "/" + model.name + "_sm.h";
    LOG_INFO("StaticCodeGenerator: Writing generated code to: {}", outputPath);
    return writeToFile(outputPath, ss.str());
}

std::string StaticCodeGenerator::generateEnum(const std::string &enumName, const std::set<std::string> &values) {
    std::stringstream ss;
    ss << "enum class " << enumName << " : uint8_t {\n";

    size_t idx = 0;
    for (const auto &value : values) {
        ss << "    " << capitalize(value);
        if (idx < values.size() - 1) {
            ss << ",";
        }
        ss << "\n";
        idx++;
    }

    ss << "};\n";
    return ss.str();
}

std::string StaticCodeGenerator::generateStateEnum(const std::set<std::string> &states) {
    return generateEnum("State", states);
}

std::string StaticCodeGenerator::generateEventEnum(const std::set<std::string> &events) {
    return generateEnum("Event", events);
}

std::string StaticCodeGenerator::generateStrategyInterface(const std::string &className,
                                                           const std::set<std::string> &guards,
                                                           const std::set<std::string> &actions) {
    std::stringstream ss;

    ss << "class I" << className << "Logic {\n";
    ss << "public:\n";
    ss << "    virtual ~I" << className << "Logic() = default;\n";

    // Generate Guard methods
    if (!guards.empty()) {
        ss << "\n";
        ss << "    // Guards\n";
        for (const auto &guard : guards) {
            ss << "    virtual bool " << guard << "() = 0;\n";
        }
    }

    // Generate Action methods
    if (!actions.empty()) {
        ss << "\n";
        ss << "    // Actions\n";
        for (const auto &action : actions) {
            ss << "    virtual void " << action << "() = 0;\n";
        }
    }

    ss << "};\n";

    return ss.str();
}

std::string StaticCodeGenerator::generateProcessEvent(const SCXMLModel &model) {
    std::stringstream ss;

    ss << "        (void)engine;\n";
    ss << "        bool transitionTaken = false;\n";
    ss << "        switch (currentState) {\n";

    // Group transitions by source state
    std::map<std::string, std::vector<Transition>> transitionsByState;
    for (const auto &trans : model.transitions) {
        transitionsByState[trans.sourceState].push_back(trans);
    }

    // Generate case for each state (sorted for consistent output)
    std::set<std::string> stateNames;
    for (const auto &state : model.states) {
        stateNames.insert(state.name);
    }

    for (const auto &stateName : stateNames) {
        ss << "            case State::" << capitalize(stateName) << ":\n";

        auto it = transitionsByState.find(stateName);
        if (it != transitionsByState.end() && !it->second.empty()) {
            // Separate event-based and eventless transitions
            std::vector<Transition> eventTransitions;
            std::vector<Transition> eventlessTransitions;

            for (const auto &trans : it->second) {
                if (trans.event.empty()) {
                    eventlessTransitions.push_back(trans);
                } else {
                    eventTransitions.push_back(trans);
                }
            }

            // Generate event-based transitions first
            if (!eventTransitions.empty()) {
                bool isFirst = true;
                for (const auto &trans : eventTransitions) {
                    if (isFirst) {
                        ss << "                if (event == Event::" << capitalize(trans.event) << ") {\n";
                        isFirst = false;
                    } else {
                        ss << "                } else if (event == Event::" << capitalize(trans.event) << ") {\n";
                    }

                    // Determine indentation level based on guard presence
                    std::string indent = "                    ";
                    bool hasGuard = !trans.guard.empty();

                    // Generate guard condition if present
                    if (hasGuard) {
                        std::string guardExpr = trans.guard;

                        // Check if guard needs JSEngine (typeof, complex ECMAScript)
                        bool needsJSEngine = (guardExpr.find("typeof") != std::string::npos);
                        bool isFunctionCall = (guardExpr.find("()") != std::string::npos);

                        if (needsJSEngine) {
                            // Complex guard → JSEngine evaluation (escape for safety)
                            ss << indent << "ensureJSEngine();  // Lazy initialization\n";
                            ss << indent << "auto& jsEngine = ::RSM::JSEngine::instance();\n";
                            ss << indent << "auto guardResult = jsEngine.evaluateExpression(sessionId_.value(), \""
                               << escapeStringLiteral(guardExpr) << "\").get();\n";
                            ss << indent << "if (!::RSM::JSEngine::isSuccess(guardResult)) {\n";
                            ss << indent
                               << "    LOG_ERROR(\"Guard evaluation failed: " << escapeStringLiteral(guardExpr)
                               << "\");\n";
                            ss << indent << "    throw std::runtime_error(\"Guard evaluation failed\");\n";
                            ss << indent << "}\n";
                            ss << indent << "if (::RSM::JSEngine::resultToBool(guardResult)) {\n";
                        } else if (isFunctionCall) {
                            // Extract function name from guard expression
                            std::string guardFunc = guardExpr;
                            size_t parenPos = guardFunc.find('(');
                            if (parenPos != std::string::npos) {
                                guardFunc = guardFunc.substr(0, parenPos);
                            }
                            // Remove leading ! if present
                            if (!guardFunc.empty() && guardFunc[0] == '!') {
                                guardFunc = guardFunc.substr(1);
                            }
                            ss << indent << "if (derived()." << guardFunc << "()) {\n";
                        } else {
                            // Simple datamodel expression - use directly
                            ss << indent << "if (" << guardExpr << ") {\n";
                        }

                        indent += "    ";  // Increase indentation inside guard
                    }

                    // Generate transition action calls
                    for (const auto &action : trans.actions) {
                        ss << indent << "derived()." << action << "();\n";
                    }

                    // Generate state transition
                    ss << indent << "currentState = State::" << capitalize(trans.targetState) << ";\n";
                    ss << indent << "transitionTaken = true;\n";

                    // Close guard if block
                    if (hasGuard) {
                        ss << "                    }\n";
                    }
                }
                ss << "                }\n";
            }

            // Generate eventless transitions (checked regardless of event)
            if (!eventlessTransitions.empty()) {
                bool firstTransition = true;

                for (size_t i = 0; i < eventlessTransitions.size(); ++i) {
                    const auto &trans = eventlessTransitions[i];
                    std::string indent = "                ";
                    bool hasGuard = !trans.guard.empty();
                    bool isLastTransition = (i == eventlessTransitions.size() - 1);

                    // Generate guard condition if present
                    if (hasGuard) {
                        std::string guardExpr = trans.guard;

                        // Check if guard needs JSEngine
                        bool needsJSEngine = (guardExpr.find("typeof") != std::string::npos);
                        bool isFunctionCall = (guardExpr.find("()") != std::string::npos);

                        // Add else if this is not the first guarded transition
                        if (!firstTransition) {
                            ss << "                } else {\n";
                        }

                        if (needsJSEngine) {
                            // Complex guard → JSEngine evaluation (escape for safety)
                            if (firstTransition) {
                                ss << indent << "{\n";  // Open scope for JSEngine variables
                            }
                            ss << indent << "    ensureJSEngine();  // Lazy initialization\n";
                            ss << indent << "    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                            ss << indent << "    auto guardResult = jsEngine.evaluateExpression(sessionId_.value(), \""
                               << escapeStringLiteral(guardExpr) << "\").get();\n";
                            ss << indent << "    if (!::RSM::JSEngine::isSuccess(guardResult)) {\n";
                            ss << indent
                               << "        LOG_ERROR(\"Guard evaluation failed: " << escapeStringLiteral(guardExpr)
                               << "\");\n";
                            ss << indent << "        throw std::runtime_error(\"Guard evaluation failed\");\n";
                            ss << indent << "    }\n";
                            ss << indent << "    if (::RSM::JSEngine::resultToBool(guardResult)) {\n";
                            indent += "        ";  // Indent for code inside the if block
                        } else if (isFunctionCall) {
                            if (firstTransition) {
                                ss << indent;
                            }
                            std::string guardFunc = guardExpr;
                            size_t parenPos = guardFunc.find('(');
                            if (parenPos != std::string::npos) {
                                guardFunc = guardFunc.substr(0, parenPos);
                            }
                            if (!guardFunc.empty() && guardFunc[0] == '!') {
                                guardFunc = guardFunc.substr(1);
                            }
                            ss << "if (derived()." << guardFunc << "()) {\n";
                            indent += "    ";
                        } else {
                            if (firstTransition) {
                                ss << indent;
                            }
                            ss << "if (" << guardExpr << ") {\n";
                            indent += "    ";
                        }

                        firstTransition = false;
                    } else {
                        // No guard - this is a fallback transition
                        if (!firstTransition) {
                            // Previous transitions had guards, so this is the else clause
                            ss << "                } else {\n";
                            indent += "    ";
                        } else {
                            // No previous guards, just execute unconditionally
                            ss << indent;
                        }
                    }

                    // Generate transition action calls
                    for (const auto &action : trans.actions) {
                        ss << indent << "derived()." << action << "();\n";
                    }

                    // Generate state transition
                    ss << indent << "currentState = State::" << capitalize(trans.targetState) << ";\n";
                    ss << indent << "transitionTaken = true;\n";

                    // Close blocks only at the end of all transitions
                    if (isLastTransition) {
                        // Check if first transition had JSEngine guard
                        bool firstHasJSEngine = false;
                        if (!eventlessTransitions.empty() && !eventlessTransitions[0].guard.empty()) {
                            firstHasJSEngine = (eventlessTransitions[0].guard.find("typeof") != std::string::npos);
                        }

                        if (firstHasJSEngine) {
                            // Close the inner if/else chain (align with the 'if' statement)
                            ss << "                }\n";
                            // Close the outer scope block
                            ss << "                }\n";
                        } else if (!firstTransition) {
                            // No JSEngine, just close the if/else chain
                            ss << "                }\n";
                        }
                    }
                }
            }
        }

        ss << "                break;\n";
    }

    ss << "        }\n";
    ss << "        return transitionTaken;\n";

    return ss.str();
}

std::vector<Action>
StaticCodeGenerator::processActions(const std::vector<std::shared_ptr<RSM::IActionNode>> &actionNodes) {
    std::vector<Action> result;
    static const std::regex funcRegex(R"((\w+)\(\))");

    for (const auto &actionNode : actionNodes) {
        std::string actionType = actionNode->getActionType();

        if (actionType == "raise") {
            if (auto raiseAction = std::dynamic_pointer_cast<RSM::RaiseAction>(actionNode)) {
                result.push_back(Action(Action::RAISE, raiseAction->getEvent()));
            }
        } else if (actionType == "script") {
            if (auto scriptAction = std::dynamic_pointer_cast<RSM::ScriptAction>(actionNode)) {
                const std::string &content = scriptAction->getContent();
                auto extractedFuncs = extractFunctionNames(content, funcRegex);
                for (const auto &func : extractedFuncs) {
                    result.push_back(Action(Action::SCRIPT, func));
                }
            }
        } else if (actionType == "assign") {
            if (auto assignAction = std::dynamic_pointer_cast<RSM::AssignAction>(actionNode)) {
                result.push_back(Action(Action::ASSIGN, assignAction->getLocation(), assignAction->getExpr()));
            }
        } else if (actionType == "log") {
            if (auto logAction = std::dynamic_pointer_cast<RSM::LogAction>(actionNode)) {
                result.push_back(Action(Action::LOG, logAction->getExpr()));
            }
        } else if (actionType == "if") {
            if (auto ifAction = std::dynamic_pointer_cast<RSM::IfAction>(actionNode)) {
                Action ifActionResult(Action::IF);

                // Process each branch
                for (const auto &branch : ifAction->getBranches()) {
                    ConditionalBranch condBranch(branch.condition, branch.isElseBranch);
                    condBranch.actions = processActions(branch.actions);
                    ifActionResult.branches.push_back(condBranch);
                }

                result.push_back(ifActionResult);
            }
        } else if (actionType == "foreach") {
            if (auto foreachAction = std::dynamic_pointer_cast<RSM::ForeachAction>(actionNode)) {
                Action foreachResult(Action::FOREACH, foreachAction->getArray(), foreachAction->getItem(),
                                     foreachAction->getIndex());
                // Process iteration actions
                foreachResult.iterationActions = processActions(foreachAction->getIterationActions());
                result.push_back(foreachResult);
            }
        }
    }

    return result;
}

std::string StaticCodeGenerator::generateClass(const SCXMLModel &model) {
    std::stringstream ss;

    // Determine if we need non-static (stateful) policy
    bool hasDataModel = !model.dataModel.empty() || model.needsJSEngine();

    // Generate State Policy class
    ss << "// State policy for " << model.name << "\n";
    ss << "struct " << model.name << "Policy {\n";
    ss << "    using State = ::RSM::Generated::" << model.name << "::State;\n";
    ss << "    using Event = ::RSM::Generated::" << model.name << "::Event;\n\n";

    // Generate datamodel member variables (for stateful policies)
    if (hasDataModel) {
        ss << "    // Datamodel variables\n";
        for (const auto &var : model.dataModel) {
            // Detect variable type from initial value
            if (var.initialValue.find('[') != std::string::npos) {
                ss << "    // Array variable (handled by JSEngine): " << var.name << " = " << var.initialValue << "\n";
            } else if (var.initialValue.empty()) {
                ss << "    // Dynamic variable (handled by JSEngine): " << var.name << "\n";
            } else {
                ss << "    int " << var.name << " = " << var.initialValue << ";\n";
            }
        }

        // Add JSEngine session ID for hybrid generation (lazy-initialized)
        if (model.needsJSEngine()) {
            ss << "\n    // JSEngine session for dynamic features (lazy-initialized)\n";
            ss << "    mutable ::std::optional<::std::string> sessionId_;\n";
        }
        ss << "\n";
    }

    // JSEngine lazy initialization and cleanup (RAII + Lazy Init pattern)
    if (model.needsJSEngine()) {
        // Default constructor (required when copy/move are deleted)
        ss << "    // Default constructor (lazy initialization, no immediate resource allocation)\n";
        ss << "    " << model.name << "Policy() = default;\n\n";

        // Destructor: Clean up JSEngine session if initialized (RAII pattern)
        ss << "    // Destructor: Clean up JSEngine session if initialized (RAII pattern)\n";
        ss << "    ~" << model.name << "Policy() {\n";
        ss << "        if (sessionId_.has_value()) {\n";
        ss << "            auto& jsEngine = ::RSM::JSEngine::instance();\n";
        ss << "            jsEngine.destroySession(sessionId_.value());\n";
        ss << "        }\n";
        ss << "    }\n\n";

        // Prevent copying/moving to avoid session ownership issues
        ss << "    // Prevent copy/move to maintain session ownership\n";
        ss << "    " << model.name << "Policy(const " << model.name << "Policy&) = delete;\n";
        ss << "    " << model.name << "Policy& operator=(const " << model.name << "Policy&) = delete;\n";
        ss << "    " << model.name << "Policy(" << model.name << "Policy&&) = delete;\n";
        ss << "    " << model.name << "Policy& operator=(" << model.name << "Policy&&) = delete;\n\n";
    }

    // Initial state
    ss << "    static State initialState() {\n";
    ss << "        return State::" << capitalize(model.initial) << ";\n";
    ss << "    }\n\n";

    // Is final state
    ss << "    static bool isFinalState(State state) {\n";
    ss << "        switch (state) {\n";
    std::set<std::string> finalStates;
    for (const auto &state : model.states) {
        if (state.isFinal && finalStates.find(state.name) == finalStates.end()) {
            finalStates.insert(state.name);
            ss << "            case State::" << capitalize(state.name) << ":\n";
            ss << "                return true;\n";
        }
    }
    ss << "            default:\n";
    ss << "                return false;\n";
    ss << "        }\n";
    ss << "    }\n\n";

    // Execute entry actions
    ss << "    template<typename Engine>\n";
    if (hasDataModel) {
        ss << "    void executeEntryActions(State state, Engine& engine) {\n";  // non-static for stateful
    } else {
        ss << "    static void executeEntryActions(State state, Engine& engine) {\n";  // static for stateless
    }
    ss << "        (void)engine;\n";
    ss << "        switch (state) {\n";
    for (const auto &state : model.states) {
        if (!state.entryActions.empty()) {
            ss << "            case State::" << capitalize(state.name) << ":\n";
            for (const auto &action : state.entryActions) {
                generateActionCode(ss, action, "engine");
            }
            ss << "                break;\n";
        }
    }
    ss << "            default:\n";
    ss << "                break;\n";
    ss << "        }\n";
    ss << "    }\n\n";

    // Execute exit actions
    ss << "    template<typename Engine>\n";
    if (hasDataModel) {
        ss << "    void executeExitActions(State state, Engine& engine) {\n";  // non-static for stateful
    } else {
        ss << "    static void executeExitActions(State state, Engine& engine) {\n";  // static for stateless
    }
    ss << "        (void)engine;\n";
    ss << "        switch (state) {\n";
    for (const auto &state : model.states) {
        if (!state.exitActions.empty()) {
            ss << "            case State::" << capitalize(state.name) << ":\n";
            for (const auto &action : state.exitActions) {
                generateActionCode(ss, action, "engine");
            }
            ss << "                break;\n";
        }
    }
    ss << "            default:\n";
    ss << "                break;\n";
    ss << "        }\n";
    ss << "    }\n\n";

    // Process transition
    ss << "    template<typename Engine>\n";
    if (hasDataModel) {
        ss << "    bool processTransition(State& currentState, Event event, Engine& engine) {\n";  // non-static for
                                                                                                   // stateful
    } else {
        ss << "    static bool processTransition(State& currentState, Event event, Engine& engine) {\n";  // static for
                                                                                                          // stateless
    }
    ss << generateProcessEvent(model);
    ss << "    }\n\n";

    // Generate private helper methods for JSEngine operations
    if (model.needsJSEngine()) {
        ss << "private:\n";
        ss << "    // Helper: Ensure JSEngine is initialized (lazy initialization)\n";
        ss << "    void ensureJSEngine() const {\n";
        ss << "        if (!sessionId_.has_value()) {\n";
        ss << "            auto& jsEngine = ::RSM::JSEngine::instance();\n";
        ss << "            sessionId_ = jsEngine.generateSessionIdString(\"" << model.name << "_\");\n";
        ss << "            jsEngine.createSession(sessionId_.value());\n";

        // Initialize datamodel variables in JSEngine with error checking
        for (const auto &var : model.dataModel) {
            if (var.initialValue.find('[') != std::string::npos) {
                // Array initialization with error handling
                ss << "            auto initResult_" << var.name
                   << " = jsEngine.executeScript(sessionId_.value(), \"var " << var.name << " = " << var.initialValue
                   << ";\").get();\n";
                ss << "            if (!::RSM::JSEngine::isSuccess(initResult_" << var.name << ")) {\n";
                ss << "                LOG_ERROR(\"Failed to initialize datamodel variable: " << var.name << "\");\n";
                ss << "                throw std::runtime_error(\"Datamodel initialization failed\");\n";
                ss << "            }\n";
            } else if (!var.initialValue.empty()) {
                // Simple variable initialization - skip for now
            }
        }

        ss << "        }\n";
        ss << "    }\n\n";

        ss << "    // Helper: Execute foreach loop with JSEngine\n";
        ss << "    void executeForeachLoop(const ::std::string& arrayName, const ::std::string& itemVar, const "
              "::std::string& indexVar) {\n";
        ss << "        ensureJSEngine();  // Lazy initialization\n";
        ss << "        auto& jsEngine = ::RSM::JSEngine::instance();\n";
        ss << "        auto arrayExpr = ::std::string(arrayName);\n";
        ss << "        auto arrayResult = jsEngine.evaluateExpression(sessionId_.value(), arrayExpr).get();\n";
        ss << "        if (!::RSM::JSEngine::isSuccess(arrayResult)) {\n";
        ss << "            LOG_ERROR(\"Failed to evaluate array expression: {}\", arrayExpr);\n";
        ss << "            throw std::runtime_error(\"Foreach array evaluation failed\");\n";
        ss << "        }\n";
        ss << "        auto arrayValues = ::RSM::JSEngine::resultToStringArray(arrayResult, sessionId_.value(), "
              "arrayExpr);\n";
        ss << "        for (size_t i = 0; i < arrayValues.size(); ++i) {\n";
        ss << "            auto setResult = jsEngine.setVariable(sessionId_.value(), itemVar, "
              "::ScriptValue(arrayValues[i])).get();\n";
        ss << "            if (!::RSM::JSEngine::isSuccess(setResult)) {\n";
        ss << "                LOG_ERROR(\"Failed to set foreach item variable: {}\", itemVar);\n";
        ss << "                throw std::runtime_error(\"Foreach setVariable failed\");\n";
        ss << "            }\n";
        ss << "            if (!indexVar.empty()) {\n";
        ss << "                auto indexResult = jsEngine.setVariable(sessionId_.value(), indexVar, "
              "::ScriptValue(static_cast<int64_t>(i))).get();\n";
        ss << "                if (!::RSM::JSEngine::isSuccess(indexResult)) {\n";
        ss << "                    LOG_ERROR(\"Failed to set foreach index variable: {}\", indexVar);\n";
        ss << "                    throw std::runtime_error(\"Foreach setVariable failed\");\n";
        ss << "                }\n";
        ss << "            }\n";
        ss << "        }\n";
        ss << "    }\n";
    }

    ss << "};\n\n";

    // Generate user-facing class using StaticExecutionEngine
    ss << "// User-facing state machine class\n";
    ss << "class " << model.name << " : public ::RSM::Static::StaticExecutionEngine<" << model.name << "Policy> {\n";
    ss << "public:\n";
    ss << "    " << model.name << "() = default;\n";
    ss << "};\n";

    return ss.str();
}

void StaticCodeGenerator::generateActionCode(std::stringstream &ss, const Action &action,
                                             const std::string &engineVar) {
    switch (action.type) {
    case Action::RAISE:
        ss << "                " << engineVar << ".raise(Event::" << capitalize(action.param1) << ");\n";
        break;
    case Action::SCRIPT:
        ss << "                " << action.param1 << "();\n";
        break;
    case Action::ASSIGN:
        ss << "                " << action.param1 << " = " << action.param2 << ";\n";
        break;
    case Action::LOG:
        ss << "                // TODO: log " << action.param1 << "\n";
        break;
    case Action::IF:
        // Generate if/elseif/else branches
        for (size_t i = 0; i < action.branches.size(); ++i) {
            const auto &branch = action.branches[i];

            if (i == 0) {
                // First branch is if
                ss << "                if (" << branch.condition << ") {\n";
            } else if (branch.isElseBranch) {
                // Else branch
                ss << "                } else {\n";
            } else {
                // Elseif branches
                ss << "                } else if (" << branch.condition << ") {\n";
            }

            // Generate actions in this branch
            for (const auto &branchAction : branch.actions) {
                generateActionCode(ss, branchAction, engineVar);
            }
        }

        if (!action.branches.empty()) {
            ss << "                }\n";
        }
        break;
    case Action::FOREACH:
        // Hybrid generation: foreach → JSEngine
        ss << "                // Foreach loop (hybrid: delegated to JSEngine)\n";

        if (action.iterationActions.empty()) {
            // Simple case: no iteration actions, use helper method (DRY)
            ss << "                executeForeachLoop(\"" << action.param1 << "\", \"" << action.param2 << "\", \""
               << action.param3 << "\");\n";
        } else {
            // Complex case: has iteration actions, generate inline
            ss << "                {\n";
            ss << "                    // Execute foreach: array=" << action.param1 << ", item=" << action.param2
               << ", index=" << action.param3 << "\n";
            ss << "                    ensureJSEngine();  // Lazy initialization\n";
            ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
            ss << "                    auto arrayExpr = ::std::string(\"" << action.param1 << "\");\n";
            ss << "                    auto arrayResult = jsEngine.evaluateExpression(sessionId_.value(), "
                  "arrayExpr).get();\n";
            ss << "                    if (!::RSM::JSEngine::isSuccess(arrayResult)) {\n";
            ss << "                        LOG_ERROR(\"Failed to evaluate array expression: {}\", arrayExpr);\n";
            ss << "                        throw std::runtime_error(\"Foreach array evaluation failed\");\n";
            ss << "                    }\n";
            ss << "                    auto arrayValues = ::RSM::JSEngine::resultToStringArray(arrayResult, "
                  "sessionId_.value(), arrayExpr);\n";
            ss << "                    for (size_t i = 0; i < arrayValues.size(); ++i) {\n";
            ss << "                        // Safe: Use setVariable to prevent code injection\n";
            ss << "                        auto setResult = jsEngine.setVariable(sessionId_.value(), \""
               << action.param2 << "\", ::ScriptValue(arrayValues[i])).get();\n";
            ss << "                        if (!::RSM::JSEngine::isSuccess(setResult)) {\n";
            ss << "                            LOG_ERROR(\"Failed to set foreach item variable: " << action.param2
               << "\");\n";
            ss << "                            throw std::runtime_error(\"Foreach setVariable failed\");\n";
            ss << "                        }\n";
            if (!action.param3.empty()) {
                ss << "                        auto indexResult = jsEngine.setVariable(sessionId_.value(), \""
                   << action.param3 << "\", ::ScriptValue(static_cast<int64_t>(i))).get();\n";
                ss << "                        if (!::RSM::JSEngine::isSuccess(indexResult)) {\n";
                ss << "                            LOG_ERROR(\"Failed to set foreach index variable: " << action.param3
                   << "\");\n";
                ss << "                            throw std::runtime_error(\"Foreach setVariable failed\");\n";
                ss << "                        }\n";
            }
            // Generate iteration actions
            for (const auto &iterAction : action.iterationActions) {
                generateActionCode(ss, iterAction, engineVar);
            }
            ss << "                    }\n";
            ss << "                }\n";
        }
        break;
    default:
        break;
    }
}

std::string StaticCodeGenerator::capitalizePublic(const std::string &str) {
    StaticCodeGenerator gen;
    return gen.capitalize(str);
}

std::string StaticCodeGenerator::capitalize(const std::string &str) {
    if (str.empty()) {
        return str;
    }

    // Handle wildcard event
    if (str == "*") {
        return "Wildcard";
    }

    // Handle other special characters in event names
    std::string result = str;
    result[0] = std::toupper(static_cast<unsigned char>(result[0]));
    return result;
}

std::set<std::string> StaticCodeGenerator::extractStates(const SCXMLModel &model) {
    std::set<std::string> stateNames;
    for (const auto &state : model.states) {
        stateNames.insert(state.name);
    }
    return stateNames;
}

std::set<std::string> StaticCodeGenerator::extractEvents(const SCXMLModel &model) {
    std::set<std::string> events;

    // Extract events from transitions
    for (const auto &transition : model.transitions) {
        if (!transition.event.empty()) {
            events.insert(transition.event);
        }
    }

    // Helper lambda to recursively extract events from actions
    std::function<void(const std::vector<Action> &)> extractFromActions;
    extractFromActions = [&](const std::vector<Action> &actions) {
        for (const auto &action : actions) {
            if (action.type == Action::RAISE && !action.param1.empty()) {
                events.insert(action.param1);
            } else if (action.type == Action::IF) {
                // Recursively process if/elseif/else branches
                for (const auto &branch : action.branches) {
                    extractFromActions(branch.actions);
                }
            }
        }
    };

    // Extract events from entry/exit actions
    for (const auto &state : model.states) {
        extractFromActions(state.entryActions);
        extractFromActions(state.exitActions);
    }

    return events;
}

std::set<std::string> StaticCodeGenerator::extractFunctionNames(const std::string &text, const std::regex &pattern) {
    std::set<std::string> functions;
    std::smatch match;
    std::string searchStr = text;

    while (std::regex_search(searchStr, match, pattern)) {
        functions.insert(match[1].str());  // Capture group 1 contains the function name
        searchStr = match.suffix();
    }

    return functions;
}

std::set<std::string> StaticCodeGenerator::extractGuardsInternal(const std::shared_ptr<RSM::SCXMLModel> &rsmModel) {
    std::set<std::string> guards;

    if (!rsmModel) {
        LOG_WARN("StaticCodeGenerator::extractGuardsInternal: rsmModel is null");
        return guards;
    }

    // Regex for extracting function names from guard expressions (e.g., "isReady()" or "!isReady()")
    static const std::regex funcRegex(R"((\w+)\(\))");

    // Extract guards from transitions using API
    auto allStates = rsmModel->getAllStates();
    for (const auto &state : allStates) {
        auto transitions = state->getTransitions();
        for (const auto &transition : transitions) {
            auto guardExpr = transition->getGuard();
            if (!guardExpr.empty()) {
                auto extracted = extractFunctionNames(guardExpr, funcRegex);
                guards.insert(extracted.begin(), extracted.end());
            }
        }
    }

    return guards;
}

std::set<std::string> StaticCodeGenerator::extractGuards(const std::string &scxmlPath) {
    // Parse SCXML file
    auto nodeFactory = std::make_shared<RSM::NodeFactory>();
    RSM::SCXMLParser parser(nodeFactory);
    auto rsmModel = parser.parseFile(scxmlPath);

    return extractGuardsInternal(rsmModel);
}

std::set<std::string> StaticCodeGenerator::extractActionsInternal(const std::shared_ptr<RSM::SCXMLModel> &rsmModel) {
    std::set<std::string> actions;

    if (!rsmModel) {
        LOG_WARN("StaticCodeGenerator::extractActionsInternal: rsmModel is null");
        return actions;
    }

    // Regex for extracting function names from script content
    static const std::regex funcRegex(R"((\w+)\(\))");

    auto allStates = rsmModel->getAllStates();
    for (const auto &state : allStates) {
        // Extract from entry action blocks
        for (const auto &actionBlock : state->getEntryActionBlocks()) {
            for (const auto &actionNode : actionBlock) {
                // Check if this is a ScriptAction
                if (auto scriptAction = std::dynamic_pointer_cast<RSM::ScriptAction>(actionNode)) {
                    const std::string &content = scriptAction->getContent();
                    auto extracted = extractFunctionNames(content, funcRegex);
                    actions.insert(extracted.begin(), extracted.end());
                }
            }
        }

        // Extract from exit action blocks
        for (const auto &actionBlock : state->getExitActionBlocks()) {
            for (const auto &actionNode : actionBlock) {
                if (auto scriptAction = std::dynamic_pointer_cast<RSM::ScriptAction>(actionNode)) {
                    const std::string &content = scriptAction->getContent();
                    auto extracted = extractFunctionNames(content, funcRegex);
                    actions.insert(extracted.begin(), extracted.end());
                }
            }
        }

        // Extract from transition actions
        for (const auto &transition : state->getTransitions()) {
            for (const auto &actionNode : transition->getActionNodes()) {
                if (auto scriptAction = std::dynamic_pointer_cast<RSM::ScriptAction>(actionNode)) {
                    const std::string &content = scriptAction->getContent();
                    auto extracted = extractFunctionNames(content, funcRegex);
                    actions.insert(extracted.begin(), extracted.end());
                }
            }
        }
    }

    return actions;
}

std::set<std::string> StaticCodeGenerator::extractActions(const std::string &scxmlPath) {
    // Parse SCXML file
    auto nodeFactory = std::make_shared<RSM::NodeFactory>();
    RSM::SCXMLParser parser(nodeFactory);
    auto rsmModel = parser.parseFile(scxmlPath);

    return extractActionsInternal(rsmModel);
}

std::string StaticCodeGenerator::escapeStringLiteral(const std::string &str) {
    std::string result;
    result.reserve(str.size() * 1.2);  // Reserve extra space for escapes

    for (char c : str) {
        switch (c) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += c;
            break;
        }
    }

    return result;
}

bool StaticCodeGenerator::writeToFile(const std::string &path, const std::string &content) {
    // Create directory
    fs::path filePath(path);
    fs::create_directories(filePath.parent_path());

    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("StaticCodeGenerator: Failed to open file for writing: {}", path);
        return false;
    }

    file << content;
    file.close();

    LOG_DEBUG("StaticCodeGenerator: Successfully wrote {} bytes to {}", content.size(), path);
    return true;
}

}  // namespace RSM::Codegen