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
        var.initialValue = dataItem->getExpr();
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
            if (!event.empty() && !targets.empty()) {
                // Use first target (SCXML supports multiple targets)
                Transition trans;
                trans.sourceState = state->getId();
                trans.event = event;
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

    // Step 5: Extract unique states and events
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
    ss << "#include \"static/StaticExecutionEngine.h\"\n\n";

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

    // Build map from state name to State for quick lookup
    std::map<std::string, const State *> stateMap;
    for (const auto &state : model.states) {
        stateMap[state.name] = &state;
    }

    // Generate case for each state (sorted for consistent output)
    std::set<std::string> stateNames;
    for (const auto &state : model.states) {
        stateNames.insert(state.name);
    }

    for (const auto &stateName : stateNames) {
        ss << "            case State::" << capitalize(stateName) << ":\n";

        // Generate if-else chain for events in this state
        auto it = transitionsByState.find(stateName);
        if (it != transitionsByState.end() && !it->second.empty()) {
            bool isFirst = true;
            for (const auto &trans : it->second) {
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

                    // Check if guard is a function call (contains "()") vs datamodel expression
                    // Function call: isReady() -> derived().isReady()
                    // Datamodel expr: Var1 == 1 -> Var1 == 1 (direct use)
                    bool isFunctionCall = (guardExpr.find("()") != std::string::npos);

                    if (isFunctionCall) {
                        // Extract function name from guard expression (e.g., "isReady()" -> "isReady")
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
                        // Datamodel expression - use directly
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
        }
    }

    return result;
}

std::string StaticCodeGenerator::generateClass(const SCXMLModel &model) {
    std::stringstream ss;

    ss << "#include \"static/StaticExecutionEngine.h\"\n\n";

    // Determine if we need non-static (stateful) policy
    bool hasDataModel = !model.dataModel.empty();

    // Generate State Policy class
    ss << "// State policy for " << model.name << "\n";
    ss << "struct " << model.name << "Policy {\n";
    ss << "    using State = ::RSM::Generated::" << model.name << "::State;\n";
    ss << "    using Event = ::RSM::Generated::" << model.name << "::Event;\n\n";

    // Generate datamodel member variables (for stateful policies)
    if (hasDataModel) {
        ss << "    // Datamodel variables\n";
        for (const auto &var : model.dataModel) {
            ss << "    int " << var.name << " = " << var.initialValue << ";\n";
        }
        ss << "\n";
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
    ss << "    }\n";

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