// Static code generator with SCXML parser integration
#include "StaticCodeGenerator.h"
#include "rsm/include/common/Logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

#include "rsm/include/actions/ScriptAction.h"
#include "rsm/include/factory/NodeFactory.h"
#include "rsm/include/model/SCXMLModel.h"
#include "rsm/include/parsing/SCXMLParser.h"

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

    // Extract all states
    auto allStates = rsmModel->getAllStates();
    if (allStates.empty()) {
        LOG_ERROR("StaticCodeGenerator: SCXML model '{}' has no states", model.name);
        return false;
    }

    static const std::regex funcRegex(R"((\w+)\(\))");

    for (const auto &state : allStates) {
        // Create State with entry/exit actions
        State stateInfo;
        stateInfo.name = state->getId();

        // Extract entry actions
        for (const auto &actionBlock : state->getEntryActionBlocks()) {
            for (const auto &actionNode : actionBlock) {
                if (auto scriptAction = std::dynamic_pointer_cast<RSM::ScriptAction>(actionNode)) {
                    const std::string &content = scriptAction->getContent();
                    auto extractedFuncs = extractFunctionNames(content, funcRegex);
                    stateInfo.entryActions.insert(stateInfo.entryActions.end(), extractedFuncs.begin(),
                                                  extractedFuncs.end());
                }
            }
        }

        // Extract exit actions
        for (const auto &actionBlock : state->getExitActionBlocks()) {
            for (const auto &actionNode : actionBlock) {
                if (auto scriptAction = std::dynamic_pointer_cast<RSM::ScriptAction>(actionNode)) {
                    const std::string &content = scriptAction->getContent();
                    auto extractedFuncs = extractFunctionNames(content, funcRegex);
                    stateInfo.exitActions.insert(stateInfo.exitActions.end(), extractedFuncs.begin(),
                                                 extractedFuncs.end());
                }
            }
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
    ss << "#include <memory>\n\n";

    // Namespace
    ss << "namespace RSM::Generated {\n\n";

    // Generate State enum
    ss << generateStateEnum(states);
    ss << "\n";

    // Generate Event enum
    ss << generateEventEnum(events);
    ss << "\n";

    // Generate Strategy interface (if there are guards or actions)
    if (!guards.empty() || !actions.empty()) {
        ss << generateStrategyInterface(model.name, guards, actions);
        ss << "\n";
    }

    // Generate class
    ss << generateClass(model);

    ss << "\n} // namespace RSM::Generated\n";

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

    ss << "    void processEvent(Event event) {\n";
    ss << "        switch (currentState_) {\n";

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
                    // Extract function name from guard expression (e.g., "isReady()" -> "isReady")
                    std::string guardFunc = trans.guard;
                    // Remove () and any negation or logical operators
                    size_t parenPos = guardFunc.find('(');
                    if (parenPos != std::string::npos) {
                        guardFunc = guardFunc.substr(0, parenPos);
                    }
                    // Remove leading ! if present
                    if (!guardFunc.empty() && guardFunc[0] == '!') {
                        guardFunc = guardFunc.substr(1);
                    }

                    ss << indent << "if (logic_->" << guardFunc << "()) {\n";
                    indent += "    ";  // Increase indentation inside guard
                }

                // 1. Generate onexit actions of source state
                auto sourceIt = stateMap.find(trans.sourceState);
                if (sourceIt != stateMap.end() && !sourceIt->second->exitActions.empty()) {
                    for (const auto &action : sourceIt->second->exitActions) {
                        ss << indent << "logic_->" << action << "();\n";
                    }
                }

                // 2. Generate transition action calls
                for (const auto &action : trans.actions) {
                    ss << indent << "logic_->" << action << "();\n";
                }

                // 3. Generate onentry actions of target state
                auto targetIt = stateMap.find(trans.targetState);
                if (targetIt != stateMap.end() && !targetIt->second->entryActions.empty()) {
                    for (const auto &action : targetIt->second->entryActions) {
                        ss << indent << "logic_->" << action << "();\n";
                    }
                }

                // 4. Generate state transition
                ss << indent << "currentState_ = State::" << capitalize(trans.targetState) << ";\n";

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
    ss << "    }\n";

    return ss.str();
}

std::string StaticCodeGenerator::generateClass(const SCXMLModel &model) {
    std::stringstream ss;

    ss << "template<typename LogicType = void>\n";
    ss << "class " << model.name << " {\n";
    ss << "private:\n";
    ss << "    State currentState_ = State::" << capitalize(model.initial) << ";\n";
    ss << "    std::unique_ptr<LogicType> logic_;\n\n";

    ss << "public:\n";
    ss << "    " << model.name << "() = default;\n\n";

    // processEvent method
    ss << generateProcessEvent(model);
    ss << "\n";

    // Logic injection
    ss << "    void setLogic(std::unique_ptr<LogicType> logic) {\n";
    ss << "        logic_ = std::move(logic);\n";
    ss << "    }\n\n";

    // Getter
    ss << "    State getCurrentState() const { return currentState_; }\n";

    ss << "};\n";

    return ss.str();
}

std::string StaticCodeGenerator::capitalize(const std::string &str) {
    if (str.empty()) {
        return str;
    }

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
    for (const auto &transition : model.transitions) {
        if (!transition.event.empty()) {
            events.insert(transition.event);
        }
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