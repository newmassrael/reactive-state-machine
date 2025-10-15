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
#include "actions/SendAction.h"
#include "common/SendHelper.h"
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

    // Step 3: Validate parsed model and extract name
    std::string modelName = rsmModel->getName();
    if (modelName.empty()) {
        // Fallback: Use filename without extension as model name
        fs::path scxmlFilePath(scxmlPath);
        modelName = scxmlFilePath.stem().string();
        LOG_WARN("StaticCodeGenerator: SCXML model has no name attribute, using filename: {}", modelName);
    }

    // Step 4: Convert RSM::SCXMLModel to simplified format for code generation
    SCXMLModel model;
    model.name = modelName;
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

        // W3C SCXML 3.4: Detect parallel states using existing API
        if (state->getType() == RSM::Type::PARALLEL) {
            stateInfo.isParallel = true;
            // Collect child region state IDs
            for (const auto &child : state->getChildren()) {
                stateInfo.childRegions.push_back(child->getId());
                LOG_DEBUG("StaticCodeGenerator: Parallel state '{}' has child region '{}'", stateId, child->getId());
            }
            LOG_DEBUG("StaticCodeGenerator: Detected parallel state '{}' with {} regions", stateId,
                      stateInfo.childRegions.size());
        }

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

            // W3C SCXML: Accept transitions with targets OR with actions (internal transitions)
            bool hasActions = !transition->getActionNodes().empty();
            if (!targets.empty() || hasActions) {
                Transition trans;
                trans.sourceState = state->getId();
                trans.event = event;                                    // May be empty for eventless transitions
                trans.targetState = targets.empty() ? "" : targets[0];  // Empty for internal transitions
                trans.guard = transition->getGuard();                   // Extract guard condition

                // Extract actions from transition (including AssignAction for internal transitions)
                for (const auto &actionNode : transition->getActionNodes()) {
                    if (auto scriptAction = std::dynamic_pointer_cast<RSM::ScriptAction>(actionNode)) {
                        const std::string &content = scriptAction->getContent();
                        auto extractedFuncs = extractFunctionNames(content, funcRegex);
                        trans.actions.insert(trans.actions.end(), extractedFuncs.begin(), extractedFuncs.end());
                    } else if (auto assignAction = std::dynamic_pointer_cast<RSM::AssignAction>(actionNode)) {
                        // Extract assign actions (for internal transitions like p0's event1)
                        Action act(Action::ASSIGN, assignAction->getLocation(), assignAction->getExpr());
                        trans.transitionActions.push_back(act);
                    }
                }

                model.transitions.push_back(trans);
            }
        }
    }

    // Step 5: Feature detection for hybrid code generation
    std::function<void(const std::vector<Action> &)> detectFeatures;
    detectFeatures = [&](const std::vector<Action> &actions) {
        for (const auto &action : actions) {
            if (action.type == Action::FOREACH) {
                model.hasForEach = true;
                // If foreach has iteration actions (assign, if, etc.), entire datamodel must be JSEngine
                if (!action.iterationActions.empty()) {
                    model.hasComplexDatamodel = true;
                }
                // Recursively check iteration actions
                detectFeatures(action.iterationActions);
            } else if (action.type == Action::SEND) {
                model.hasSend = true;
            } else if (action.type == Action::ASSIGN) {
                // W3C SCXML: <assign> with expr attribute requires JSEngine for evaluation
                // Test 174: <assign location="Var1" expr="'http://...'" />
                if (!action.param2.empty()) {
                    model.hasComplexDatamodel = true;
                }
            } else if (action.type == Action::IF) {
                for (const auto &branch : action.branches) {
                    detectFeatures(branch.actions);
                }
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
    ss << "#include <string>\n";
    ss << "#include \"static/StaticExecutionEngine.h\"\n";

    // Add SendHelper include if needed
    if (model.hasSend) {
        ss << "#include \"common/SendHelper.h\"\n";
    }

    // Add JSEngine and Logger includes if needed for hybrid code generation (OUTSIDE namespace)
    if (model.needsJSEngine()) {
        ss << "#include <optional>\n";
        ss << "#include \"common/Logger.h\"\n";
        ss << "#include \"scripting/JSEngine.h\"\n";
        ss << "#include \"common/ForeachValidator.h\"\n";
        ss << "#include \"common/ForeachHelper.h\"\n";
        ss << "#include \"common/GuardHelper.h\"\n";
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

std::string StaticCodeGenerator::generateProcessEvent(const SCXMLModel &model, const std::set<std::string> &events) {
    std::stringstream ss;

    ss << "        (void)engine;\n";
    ss << "        (void)event;\n";
    ss << "        bool transitionTaken = false;\n";
    ss << "        switch (currentState) {\n";

    // Group transitions by source state
    std::map<std::string, std::vector<Transition>> transitionsByState;
    for (const auto &trans : model.transitions) {
        transitionsByState[trans.sourceState].push_back(trans);
    }

    // Generate case for each state (sorted for consistent output)
    std::set<std::string> stateNames;
    std::map<std::string, const State *> stateByName;
    for (const auto &state : model.states) {
        stateNames.insert(state.name);
        stateByName[state.name] = &state;
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
                // W3C SCXML 3.5.1: Group transitions by event while preserving document order
                auto transitionsByEvent = groupTransitionsByEventPreservingOrder(eventTransitions);

                // Generate event-based transition checks
                bool firstEvent = true;
                const std::string baseIndent = "                ";

                for (const auto &[eventName, transitions] : transitionsByEvent) {
                    // Start event check block
                    if (firstEvent) {
                        ss << baseIndent << "if (event == Event::" << capitalize(eventName) << ") {\n";
                        firstEvent = false;
                    } else {
                        ss << baseIndent << "} else if (event == Event::" << capitalize(eventName) << ") {\n";
                    }

                    const std::string eventIndent = baseIndent + "    ";

                    // Generate guard-based transition chain for this event
                    bool firstGuard = true;
                    for (const auto &trans : transitions) {
                        bool hasGuard = !trans.guard.empty();
                        std::string guardIndent = eventIndent;

                        if (hasGuard) {
                            // Guarded transition
                            std::string guardExpr = trans.guard;
                            bool needsJSEngine =
                                model.needsJSEngine() || (guardExpr.find("typeof") != std::string::npos);
                            bool isFunctionCall = (guardExpr.find("()") != std::string::npos);

                            if (firstGuard) {
                                // First guard uses if
                                if (needsJSEngine) {
                                    ss << eventIndent << "this->ensureJSEngine();\n";
                                    ss << eventIndent << "auto& jsEngine = ::RSM::JSEngine::instance();\n";
                                    ss << eventIndent
                                       << "if (::RSM::GuardHelper::evaluateGuard(jsEngine, sessionId_.value(), \""
                                       << escapeStringLiteral(guardExpr) << "\")) {\n";
                                } else if (isFunctionCall) {
                                    std::string guardFunc = guardExpr;
                                    size_t parenPos = guardFunc.find('(');
                                    if (parenPos != std::string::npos) {
                                        guardFunc = guardFunc.substr(0, parenPos);
                                    }
                                    if (!guardFunc.empty() && guardFunc[0] == '!') {
                                        guardFunc = guardFunc.substr(1);
                                    }
                                    ss << eventIndent << "if (derived()." << guardFunc << "()) {\n";
                                } else {
                                    ss << eventIndent << "if (" << guardExpr << ") {\n";
                                }
                                firstGuard = false;
                            } else {
                                // Subsequent guards use else if
                                if (needsJSEngine) {
                                    ss << eventIndent << "} else {\n";
                                    guardIndent = eventIndent + "    ";
                                    ss << guardIndent << "this->ensureJSEngine();\n";
                                    ss << guardIndent << "auto& jsEngine = ::RSM::JSEngine::instance();\n";
                                    ss << guardIndent
                                       << "if (::RSM::GuardHelper::evaluateGuard(jsEngine, sessionId_.value(), \""
                                       << escapeStringLiteral(guardExpr) << "\")) {\n";
                                } else if (isFunctionCall) {
                                    std::string guardFunc = guardExpr;
                                    size_t parenPos = guardFunc.find('(');
                                    if (parenPos != std::string::npos) {
                                        guardFunc = guardFunc.substr(0, parenPos);
                                    }
                                    if (!guardFunc.empty() && guardFunc[0] == '!') {
                                        guardFunc = guardFunc.substr(1);
                                    }
                                    ss << eventIndent << "} else if (derived()." << guardFunc << "()) {\n";
                                } else {
                                    ss << eventIndent << "} else if (" << guardExpr << ") {\n";
                                }
                            }
                            guardIndent += "    ";
                        } else {
                            // Unguarded transition (fallback else clause)
                            if (!firstGuard) {
                                ss << eventIndent << "} else {\n";
                                guardIndent += "    ";
                            }
                        }

                        // Generate transition action calls
                        for (const auto &action : trans.actions) {
                            ss << guardIndent << "derived()." << action << "();\n";
                        }

                        // W3C SCXML: Execute transition actions (e.g., assign for internal transitions)
                        for (const auto &action : trans.transitionActions) {
                            generateActionCode(ss, action, "engine", events, model);
                        }

                        // W3C SCXML: Only change state if targetState exists (not an internal transition)
                        if (!trans.targetState.empty()) {
                            ss << guardIndent << "currentState = State::" << capitalize(trans.targetState) << ";\n";
                        }
                        ss << guardIndent << "transitionTaken = true;\n";
                    }

                    // Close guard chain if we had any guards
                    if (!firstGuard) {
                        ss << eventIndent << "}\n";
                    }
                }

                // Close event chain
                if (!firstEvent) {
                    ss << baseIndent << "}\n";
                }
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

                        // Interpreter Engine: ALL guards use JSEngine evaluation
                        // JIT Engine: Only typeof guards use JSEngine, others use C++ direct evaluation
                        bool needsJSEngine = model.needsJSEngine() || (guardExpr.find("typeof") != std::string::npos);
                        bool isFunctionCall = (guardExpr.find("()") != std::string::npos);

                        // Add else if this is not the first guarded transition
                        if (!firstTransition) {
                            ss << "                } else {\n";
                        }

                        if (needsJSEngine) {
                            // Interpreter Engine or complex guard → JSEngine evaluation using GuardHelper
                            if (firstTransition) {
                                ss << indent << "{\n";  // Open scope for JSEngine variables
                            }
                            ss << indent << "    this->ensureJSEngine();\n";
                            ss << indent << "    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                            ss << indent << "    if (::RSM::GuardHelper::evaluateGuard(jsEngine, sessionId_.value(), \""
                               << escapeStringLiteral(guardExpr) << "\")) {\n";
                            indent += "        ";  // Indent for code inside the if block
                        } else if (isFunctionCall) {
                            // JIT Engine: Extract function name from guard expression
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
                            // JIT Engine: Simple datamodel expression - use C++ direct evaluation
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

                    // W3C SCXML: Execute transition actions (e.g., assign for internal transitions)
                    for (const auto &action : trans.transitionActions) {
                        generateActionCode(ss, action, "engine", events, model);
                    }

                    // W3C SCXML: Only change state if targetState exists (not an internal transition)
                    if (!trans.targetState.empty()) {
                        ss << indent << "currentState = State::" << capitalize(trans.targetState) << ";\n";
                    }
                    ss << indent << "transitionTaken = true;\n";

                    // Close blocks only at the end of all transitions
                    if (isLastTransition) {
                        // Check if first transition had JSEngine guard
                        bool firstHasJSEngine = false;
                        if (!eventlessTransitions.empty() && !eventlessTransitions[0].guard.empty()) {
                            std::string firstGuard = eventlessTransitions[0].guard;
                            firstHasJSEngine =
                                model.needsJSEngine() || (firstGuard.find("typeof") != std::string::npos);
                        }

                        if (firstHasJSEngine) {
                            // Close the inner if/else chain (align with the 'if' statement)
                            ss << "                    }\n";
                            // Close the outer scope block
                            ss << "                }\n";
                        } else if (!firstTransition) {
                            // No JSEngine, just close the if/else chain
                            ss << "                }\n";
                        }
                    }
                }
            }

            // W3C SCXML 3.4: Propagate events to parallel region children
            auto stateIt = stateByName.find(stateName);
            if (stateIt != stateByName.end() && stateIt->second->isParallel && !stateIt->second->childRegions.empty()) {
                ss << "\n";
                ss << "                // W3C SCXML 3.4: Check transitions in parallel region children\n";
                for (size_t i = 0; i < stateIt->second->childRegions.size(); ++i) {
                    const auto &regionName = stateIt->second->childRegions[i];
                    ss << "                if (parallel_" << stateName << "_region" << i
                       << "State_ == State::" << capitalize(regionName) << ") {\n";

                    // Check transitions for this region state
                    auto regionIt = transitionsByState.find(regionName);
                    if (regionIt != transitionsByState.end() && !regionIt->second.empty()) {
                        // W3C SCXML 3.5.1: Group transitions by event while preserving document order
                        auto transitionsByEvent = groupTransitionsByEventPreservingOrder(regionIt->second);

                        // Generate event-based transition checks
                        bool firstEvent = true;
                        const std::string baseIndent = "                    ";

                        for (const auto &[eventName, transitions] : transitionsByEvent) {
                            // Start event check block
                            if (firstEvent) {
                                ss << baseIndent << "if (event == Event::" << capitalize(eventName) << ") {\n";
                                firstEvent = false;
                            } else {
                                ss << baseIndent << "} else if (event == Event::" << capitalize(eventName) << ") {\n";
                            }

                            const std::string eventIndent = baseIndent + "    ";

                            // Generate guard-based transition chain for this event
                            bool firstGuard = true;
                            for (const auto &trans : transitions) {
                                bool hasGuard = !trans.guard.empty();
                                std::string guardIndent = eventIndent;

                                if (hasGuard) {
                                    // Guarded transition
                                    if (firstGuard) {
                                        ss << eventIndent << "if (" << trans.guard << ") {\n";
                                        firstGuard = false;
                                    } else {
                                        ss << eventIndent << "} else if (" << trans.guard << ") {\n";
                                    }
                                    guardIndent += "    ";
                                } else {
                                    // Unguarded transition (fallback else clause)
                                    if (!firstGuard) {
                                        ss << eventIndent << "} else {\n";
                                        guardIndent += "    ";
                                    }
                                    // If firstGuard is true and no guard, execute unconditionally
                                }

                                // Execute transition actions
                                for (const auto &action : trans.transitionActions) {
                                    std::stringstream actionSS;
                                    generateActionCode(actionSS, action, "engine", events, model);
                                    std::string actionCode = actionSS.str();
                                    // Add proper indentation to action code
                                    std::istringstream iss(actionCode);
                                    std::string line;
                                    while (std::getline(iss, line)) {
                                        if (!line.empty()) {
                                            ss << guardIndent << line << "\n";
                                        }
                                    }
                                }

                                // Update region state if target exists
                                if (!trans.targetState.empty()) {
                                    ss << guardIndent << "parallel_" << stateName << "_region" << i
                                       << "State_ = State::" << capitalize(trans.targetState) << ";\n";
                                }

                                ss << guardIndent << "transitionTaken = true;\n";
                            }

                            // Close guard chain if we had any guards
                            if (!firstGuard) {
                                ss << eventIndent << "}\n";
                            }
                        }

                        // Close event chain
                        if (!firstEvent) {
                            ss << baseIndent << "}\n";
                        }
                    }

                    ss << "                }\n";
                }
            }

            ss << "                break;\n";
        } else {
            // States without transitions (e.g., final states) must explicitly break
            ss << "                break;\n";
        }
    }
    ss << "            default:\n";
    ss << "                break;\n";
    ss << "        }\n";
    ss << "        return transitionTaken;\n";
    ss << "    }\n";

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
        } else if (actionType == "send") {
            if (auto sendAction = std::dynamic_pointer_cast<RSM::SendAction>(actionNode)) {
                // Store event, target, targetExpr, and eventExpr for send action
                std::string event = sendAction->getEvent();
                std::string target = sendAction->getTarget();
                std::string targetExpr = sendAction->getTargetExpr();
                std::string eventExpr = sendAction->getEventExpr();
                result.push_back(Action(Action::SEND, event, target, targetExpr, eventExpr));
            }
        }
    }

    return result;
}

std::string StaticCodeGenerator::generateClass(const SCXMLModel &model) {
    std::stringstream ss;
    std::set<std::string> events = extractEvents(model);
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
                ss << "    // Runtime-evaluated variable (handled by JSEngine): " << var.name << "\n";
            } else if (var.initialValue.size() >= 2 && var.initialValue.front() == '\'' &&
                       var.initialValue.back() == '\'') {
                // JavaScript string literal: 'value' → C++ string: "value"
                std::string strValue = var.initialValue.substr(1, var.initialValue.size() - 2);
                // Escape special characters for safe C++ generation
                std::string escapedValue = escapeStringLiteral(strValue);
                ss << "    std::string " << var.name << " = \"" << escapedValue << "\";\n";
            } else {
                ss << "    int " << var.name << " = " << var.initialValue << ";\n";
            }
        }

        // Add JSEngine session ID for hybrid code generation (lazy-initialized)
        if (model.needsJSEngine()) {
            ss << "\n    // JSEngine session for dynamic features (lazy-initialized)\n";
            ss << "    mutable ::std::optional<::std::string> sessionId_;\n";
        }
        ss << "\n";
    }

    // W3C SCXML 3.4: Generate parallel region state variables
    std::map<std::string, std::vector<std::string>> parallelStateRegions;
    bool hasParallelStates = false;
    for (const auto &state : model.states) {
        if (state.isParallel && !state.childRegions.empty()) {
            parallelStateRegions[state.name] = state.childRegions;
            hasParallelStates = true;
        }
    }

    if (hasParallelStates) {
        ss << "    // Parallel region state variables (8 bytes per region)\n";
        for (const auto &[parallelState, regions] : parallelStateRegions) {
            for (size_t i = 0; i < regions.size(); ++i) {
                const auto &regionName = regions[i];
                ss << "    State parallel_" << parallelState << "_region" << i
                   << "State_ = State::" << capitalize(regionName) << ";\n";
            }
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
        bool hasEntryActions = !state.entryActions.empty();
        bool needsParallelInit = state.isParallel && !state.childRegions.empty();

        if (hasEntryActions || needsParallelInit) {
            ss << "            case State::" << capitalize(state.name) << ":\n";

            // W3C SCXML 3.4: Initialize parallel region states first (before entry actions)
            if (needsParallelInit) {
                ss << "                // W3C SCXML 3.4: Initialize parallel region states\n";
                for (size_t i = 0; i < state.childRegions.size(); ++i) {
                    const auto &regionName = state.childRegions[i];
                    ss << "                parallel_" << state.name << "_region" << i
                       << "State_ = State::" << capitalize(regionName) << ";\n";
                }
            }

            // Then execute entry actions
            for (const auto &action : state.entryActions) {
                generateActionCode(ss, action, "engine", events, model);
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
                generateActionCode(ss, action, "engine", events, model);
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
    ss << generateProcessEvent(model, events);

    // Generate private helper methods for JSEngine operations
    if (model.needsJSEngine()) {
        ss << "private:\n";
        ss << "    // Helper: Ensure JSEngine is initialized (lazy initialization)\n";
        ss << "    void ensureJSEngine() const {\n";
        ss << "        if (!sessionId_.has_value()) {\n";
        ss << "            auto& jsEngine = ::RSM::JSEngine::instance();\n";
        ss << "            sessionId_ = jsEngine.generateSessionIdString(\"" << model.name << "_\");\n";
        ss << "            jsEngine.createSession(sessionId_.value());\n";

        // Initialize ALL datamodel variables in JSEngine (Interpreter Engine architecture)
        for (const auto &var : model.dataModel) {
            // Generate initialization expression (use "undefined" if no initial value)
            std::string initExpr = var.initialValue.empty() ? "undefined" : var.initialValue;

            // Evaluate expression and set variable using evaluateExpression + setVariable pattern
            ss << "            auto initExpr_" << var.name << " = jsEngine.evaluateExpression(sessionId_.value(), \""
               << escapeStringLiteral(initExpr) << "\").get();\n";
            ss << "            if (!::RSM::JSEngine::isSuccess(initExpr_" << var.name << ")) {\n";
            ss << "                LOG_ERROR(\"Failed to evaluate expression for variable: " << var.name << "\");\n";
            ss << "                throw std::runtime_error(\"Datamodel initialization failed\");\n";
            ss << "            }\n";
            ss << "            jsEngine.setVariable(sessionId_.value(), \"" << var.name << "\", initExpr_" << var.name
               << ".getInternalValue());\n";
        }

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

void StaticCodeGenerator::generateActionCode(std::stringstream &ss, const Action &action, const std::string &engineVar,
                                             const std::set<std::string> &events, const SCXMLModel &model) {
    switch (action.type) {
    case Action::RAISE:
        ss << "                " << engineVar << ".raise(Event::" << capitalize(action.param1) << ");\n";
        break;
    case Action::SCRIPT:
        ss << "                " << action.param1 << "();\n";
        break;
    case Action::ASSIGN: {
        // W3C SCXML: <assign> with expr attribute
        // When JSEngine is needed, use evaluateExpression + setVariable pattern
        // When JSEngine is not needed, use direct C++ assignment
        if (model.needsJSEngine()) {
            // JSEngine-based assignment: evaluate expression and set variable
            ss << "                {\n";  // Extra scope to avoid "jump to case label" error
            ss << "                    this->ensureJSEngine();\n";
            ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
            ss << "                    {\n";
            ss << "                        auto exprResult = jsEngine.evaluateExpression(sessionId_.value(), \""
               << escapeStringLiteral(action.param2) << "\").get();\n";
            ss << "                        if (!::RSM::JSEngine::isSuccess(exprResult)) {\n";
            ss << "                            LOG_ERROR(\"Failed to evaluate expression for assign: "
               << escapeStringLiteral(action.param2) << "\");\n";
            ss << "                            throw std::runtime_error(\"Assign expression evaluation failed\");\n";
            ss << "                        }\n";
            ss << "                        jsEngine.setVariable(sessionId_.value(), \"" << action.param1
               << "\", exprResult.getInternalValue());\n";
            ss << "                    }\n";
            ss << "                }\n";
        } else {
            // Direct C++ assignment for simple static variables
            std::string expr = action.param2;
            // Convert JavaScript string literal 'value' to C++ string literal "value"
            if (expr.size() >= 2 && expr.front() == '\'' && expr.back() == '\'') {
                // Extract string content and escape special characters
                std::string strValue = expr.substr(1, expr.size() - 2);
                std::string escapedValue = escapeStringLiteral(strValue);
                expr = "\"" + escapedValue + "\"";
            }
            ss << "                " << action.param1 << " = " << expr << ";\n";
        }
    } break;
    case Action::LOG:
        ss << "                // TODO: log " << action.param1 << "\n";
        break;
    case Action::SEND:
        // W3C SCXML 6.2: send with target validation using shared SendHelper
        // param1: event, param2: target, param3: targetExpr, param4: eventExpr
        {
            std::string event = action.param1;
            std::string target = action.param2;
            std::string targetExpr = action.param3;
            std::string eventExpr = action.param4;

            // W3C SCXML 6.2: Handle targetexpr (dynamic target evaluation) - Test 173
            if (!targetExpr.empty()) {
                ss << "                // W3C SCXML 6.2 (test 173): Validate dynamic target from targetexpr\n";
                ss << "                {\n";
                if (model.needsJSEngine()) {
                    // JSEngine-based: retrieve variable value from JSEngine
                    ss << "                    this->ensureJSEngine();\n";
                    ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                    ss << "                    auto targetResult = jsEngine.getVariable(sessionId_.value(), \""
                       << targetExpr << "\").get();\n";
                    ss << "                    if (!::RSM::JSEngine::isSuccess(targetResult)) {\n";
                    ss << "                        LOG_ERROR(\"Failed to get variable for targetexpr: "
                       << escapeStringLiteral(targetExpr) << "\");\n";
                    ss << "                        return;\n";
                    ss << "                    }\n";
                    ss << "                    std::string dynamicTarget = "
                          "::RSM::JSEngine::resultToString(targetResult);\n";
                } else {
                    // Static: direct variable reference
                    ss << "                    std::string dynamicTarget = " << targetExpr << ";\n";
                }
                ss << "                    if (::RSM::SendHelper::isInvalidTarget(dynamicTarget)) {\n";
                ss << "                        // W3C SCXML 6.2: Invalid target stops execution\n";
                ss << "                        return;\n";
                ss << "                    }\n";
                ss << "                    // Target is valid (including #_internal for internal events)\n";
                ss << "                }\n";
            } else if (!target.empty()) {
                // Static target validation (only when targetExpr is not present)
                ss << "                // W3C SCXML 6.2 (tests 159, 194): Validate send target using SendHelper\n";
                ss << "                if (::RSM::SendHelper::isInvalidTarget(\"" << target << "\")) {\n";
                ss << "                    // W3C SCXML 5.10: Invalid target stops subsequent executable content\n";
                ss << "                    return;  // Stop execution of subsequent actions in this "
                      "entry/exit/transition\n";
                ss << "                }\n";
            }

            // W3C SCXML: Handle eventexpr (dynamic event name evaluation)
            if (!eventExpr.empty()) {
                ss << "                // W3C SCXML 6.2 (test172): Evaluate eventexpr and raise as event\n";
                ss << "                {\n";
                if (model.needsJSEngine()) {
                    // JSEngine-based: retrieve variable value from JSEngine
                    ss << "                    this->ensureJSEngine();\n";
                    ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                    ss << "                    auto eventResult = jsEngine.getVariable(sessionId_.value(), \""
                       << eventExpr << "\").get();\n";
                    ss << "                    if (!::RSM::JSEngine::isSuccess(eventResult)) {\n";
                    ss << "                        LOG_ERROR(\"Failed to get variable for eventexpr: "
                       << escapeStringLiteral(eventExpr) << "\");\n";
                    ss << "                        return;\n";
                    ss << "                    }\n";
                    ss << "                    std::string eventName = ::RSM::JSEngine::resultToString(eventResult);\n";
                } else {
                    // Static: direct variable reference
                    ss << "                    std::string eventName = " << eventExpr << ";\n";
                }
                ss << "                    // Convert event name string to Event enum\n";

                // Generate if-else chain to map string to Event enum
                bool firstEvent = true;
                for (const auto &eventName : events) {
                    std::string eventEnum = capitalize(eventName);
                    if (firstEvent) {
                        ss << "                    if (eventName == \"" << eventName << "\") {\n";
                        ss << "                        " << engineVar << ".raise(Event::" << eventEnum << ");\n";
                        firstEvent = false;
                    } else {
                        ss << "                    } else if (eventName == \"" << eventName << "\") {\n";
                        ss << "                        " << engineVar << ".raise(Event::" << eventEnum << ");\n";
                    }
                }
                if (!firstEvent) {
                    ss << "                    }\n";
                }
                ss << "                }\n";
            } else if (!event.empty()) {
                // Static event: only raise if event exists in Event enum
                if (events.find(event) != events.end()) {
                    ss << "                " << engineVar << ".raise(Event::" << capitalize(event) << ");\n";
                } else {
                    // Event not in enum, likely unreachable - just comment
                    ss << "                // Event '" << event << "' not defined in Event enum (unreachable)\n";
                }
            }
        }
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
                generateActionCode(ss, branchAction, engineVar, events, model);
            }
        }

        if (!action.branches.empty()) {
            ss << "                }\n";
        }
        break;
    case Action::FOREACH:
        // JIT generation: foreach → JSEngine with error handling
        {
            bool hasErrorExecution = (events.find("error.execution") != events.end());
            ss << "                // Foreach loop (JIT: delegated to JSEngine)\n";

            if (hasErrorExecution) {
                ss << "                try {\n";
            }

            // W3C SCXML 4.6: Validate array and item attributes using common validation function
            ss << "                    ::RSM::Validation::validateForeachAttributes(\"" << action.param1 << "\", \""
               << action.param2 << "\");\n";

            if (action.iterationActions.empty()) {
                // Simple case: no iteration actions, use ForeachHelper directly
                ss << "                    {\n";
                ss << "                        this->ensureJSEngine();\n";
                ss << "                        auto& jsEngine = ::RSM::JSEngine::instance();\n";
                ss << "                        ::RSM::ForeachHelper::executeForeachWithoutBody(\n";
                ss << "                            jsEngine, sessionId_.value(),\n";
                ss << "                            \"" << action.param1 << "\", \"" << action.param2 << "\", \""
                   << action.param3 << "\"\n";
                ss << "                        );\n";
                ss << "                    }\n";
            } else {
                // Complex case: has iteration actions, use helper with W3C 4.6 compliant error handling
                ss << "                    {\n";
                ss << "                        // Execute foreach: array=" << action.param1
                   << ", item=" << action.param2 << ", index=" << action.param3 << "\n";
                ss << "                        this->ensureJSEngine();\n";
                ss << "                        auto& jsEngine = ::RSM::JSEngine::instance();\n";
                ss << "                        // W3C SCXML 4.6: Use ForeachHelper for centralized error handling\n";
                ss << "                        ::RSM::ForeachHelper::executeForeachWithActions(\n";
                ss << "                            jsEngine, sessionId_.value(),\n";
                ss << "                            \"" << action.param1 << "\", \"" << action.param2 << "\", \""
                   << action.param3 << "\",\n";
                ss << "                            [&](size_t i) -> bool {\n";
                ss << "                                (void)i;  // Iteration index available if needed\n";

                // Execute iteration actions using same logic as ActionExecutor::assignVariable()
                for (const auto &iterAction : action.iterationActions) {
                    if (iterAction.type == Action::ASSIGN) {
                        // Each action gets its own scope to avoid variable name conflicts
                        ss << "                                {\n";
                        ss << "                                    auto exprResult = "
                              "jsEngine.evaluateExpression(sessionId_.value(), \""
                           << escapeStringLiteral(iterAction.param2) << "\").get();\n";
                        ss << "                                    if (!::RSM::JSEngine::isSuccess(exprResult)) {\n";
                        ss << "                                        LOG_ERROR(\"Failed to evaluate expression in "
                              "foreach: "
                           << escapeStringLiteral(iterAction.param2) << "\");\n";
                        ss << "                                        return false;  // W3C SCXML 4.6: Stop foreach "
                              "execution on error\n";
                        ss << "                                    }\n";
                        ss << "                                    jsEngine.setVariable(sessionId_.value(), \""
                           << iterAction.param1 << "\", exprResult.getInternalValue());\n";
                        ss << "                                }\n";
                    }
                }

                ss << "                                return true;  // Continue to next iteration\n";
                ss << "                            }\n";
                ss << "                        );\n";
                ss << "                    }\n";
            }

            if (hasErrorExecution) {
                ss << "                } catch (const std::runtime_error&) {\n";
                ss << "                    " << engineVar << ".raise(Event::Error_execution);\n";
                ss << "                }\n";
            }
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
    // Replace dots with underscores (e.g., "error.execution" -> "Error_execution")
    std::string result = str;
    std::replace(result.begin(), result.end(), '.', '_');
    result[0] = std::toupper(static_cast<unsigned char>(result[0]));
    return result;
}

// W3C SCXML 3.5.1: Group transitions by event while preserving document order
std::vector<std::pair<std::string, std::vector<Transition>>>
StaticCodeGenerator::groupTransitionsByEventPreservingOrder(const std::vector<Transition> &transitions) {
    std::vector<std::pair<std::string, std::vector<Transition>>> result;

    for (const auto &trans : transitions) {
        if (trans.event.empty()) {
            continue;  // Skip eventless transitions
        }

        // Find existing group for this event
        bool found = false;
        for (auto &[eventName, group] : result) {
            if (eventName == trans.event) {
                group.push_back(trans);
                found = true;
                break;
            }
        }

        // Create new group if this is the first occurrence (preserves document order)
        if (!found) {
            result.push_back({trans.event, {trans}});
        }
    }

    return result;
}

std::set<std::string> StaticCodeGenerator::extractStates(const SCXMLModel &model) {
    std::set<std::string> stateNames;
    for (const auto &state : model.states) {
        stateNames.insert(state.name);
        // W3C SCXML 3.4: Include child region states for parallel states
        if (state.isParallel) {
            for (const auto &region : state.childRegions) {
                stateNames.insert(region);
                LOG_DEBUG("StaticCodeGenerator: Including parallel region '{}' in State enum", region);
            }
        }
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

std::string StaticCodeGenerator::actionToJavaScript(const std::vector<Action> &actions) {
    std::stringstream js;
    for (const auto &action : actions) {
        if (action.type == Action::ASSIGN) {
            // Assignment: location = expr;
            js << action.param1 << " = " << action.param2 << ";\n";
        } else if (action.type == Action::IF) {
            // IF: if (cond) { ... } else { ... }
            for (size_t i = 0; i < action.branches.size(); ++i) {
                const auto &branch = action.branches[i];
                if (i == 0) {
                    // First branch is if
                    js << "if (" << branch.condition << ") {\n";
                } else if (branch.isElseBranch) {
                    // Else branch
                    js << "} else {\n";
                } else {
                    // Elseif branches
                    js << "} else if (" << branch.condition << ") {\n";
                }
                // Recursively transpile branch actions
                js << actionToJavaScript(branch.actions);
            }
            if (!action.branches.empty()) {
                js << "}\n";
            }
        }
        // Add more action types as needed
    }
    return js.str();
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
