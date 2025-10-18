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
#include "actions/CancelAction.h"
#include "actions/ForeachAction.h"
#include "actions/IfAction.h"
#include "actions/LogAction.h"
#include "actions/RaiseAction.h"
#include "actions/ScriptAction.h"
#include "actions/SendAction.h"
#include "common/AssignHelper.h"
#include "common/BindingHelper.h"
#include "common/DataModelHelper.h"
#include "common/DoneDataHelper.h"
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

    // W3C SCXML 5.3: Extract binding mode ("early" or "late")
    model.bindingMode = rsmModel->getBinding();
    if (model.bindingMode.empty()) {
        model.bindingMode = "early";  // W3C SCXML 5.3: Default is early binding
    }
    LOG_DEBUG("StaticCodeGenerator: Model '{}' uses {} binding", model.name, model.bindingMode);

    // W3C SCXML 3.3: Resolve initial state recursively for composite states
    // If root initial is a composite state with its own initial, follow the chain
    std::string initialState = rsmModel->getInitialState();
    if (initialState.empty()) {
        LOG_WARN("StaticCodeGenerator: SCXML model '{}' has no initial state - generating Interpreter wrapper",
                 model.name);
        std::stringstream ss;
        bool result = generateInterpreterWrapper(ss, model, rsmModel, scxmlPath, outputDir);
        if (result) {
            fs::path outputPath = fs::path(outputDir) / (model.name + "_sm.h");
            writeToFile(outputPath.string(), ss.str());
        }
        return result;
    }

    // Recursively resolve composite state initials to find leaf state
    std::string currentState = initialState;
    std::vector<std::string> initialChain;  // Track hierarchy for entry actions
    initialChain.push_back(currentState);

    while (true) {
        // Find the state node
        auto allStates = rsmModel->getAllStates();
        std::shared_ptr<RSM::StateNode> stateNode = nullptr;
        for (const auto &state : allStates) {
            if (state->getId() == currentState) {
                stateNode = std::static_pointer_cast<RSM::StateNode>(state);
                break;
            }
        }

        if (!stateNode) {
            LOG_WARN("StaticCodeGenerator: Initial state '{}' not found in model - generating Interpreter wrapper",
                     currentState);
            std::stringstream ss;
            bool result = generateInterpreterWrapper(ss, model, rsmModel, scxmlPath, outputDir);
            if (result) {
                fs::path outputPath = fs::path(outputDir) / (model.name + "_sm.h");
                writeToFile(outputPath.string(), ss.str());
            }
            return result;
        }

        // Check if this state has an initial child
        std::string childInitial = stateNode->getInitialState();
        if (childInitial.empty()) {
            // This is a leaf state (or atomic state without children)
            break;
        }

        // Continue to child initial
        LOG_DEBUG("StaticCodeGenerator: Composite state '{}' has initial child '{}'", currentState, childInitial);
        currentState = childInitial;
        initialChain.push_back(currentState);
    }

    model.initial = currentState;
    LOG_INFO("StaticCodeGenerator: Resolved initial state chain: {} -> actual initial: {}", initialState,
             model.initial);

    // Extract datamodel variables (root level + state level)
    // W3C SCXML 5.10: Track variable names to avoid duplicates
    std::set<std::string> dataModelVarNames;

    // Root level datamodel
    for (const auto &dataItem : rsmModel->getDataModelItems()) {
        auto helperVar = DataModelHelper::extractVariable(dataItem.get());
        DataModelVariable var;
        var.name = helperVar.name;
        var.initialValue = helperVar.initialValue;
        model.dataModel.push_back(var);
        dataModelVarNames.insert(var.name);
    }

    // W3C SCXML 5.10: Extract state-level datamodel variables (global scope)
    auto allStatesForDatamodel = rsmModel->getAllStates();
    for (const auto &state : allStatesForDatamodel) {
        for (const auto &dataItem : state->getDataItems()) {
            std::string varName = dataItem->getId();

            // Skip if already added (avoid duplicates)
            if (dataModelVarNames.find(varName) != dataModelVarNames.end()) {
                LOG_DEBUG("StaticCodeGenerator: Skipping duplicate datamodel variable '{}' from state '{}'", varName,
                          state->getId());
                continue;
            }

            auto helperVar = DataModelHelper::extractVariable(dataItem.get());
            DataModelVariable var;
            var.name = helperVar.name;
            var.initialValue = helperVar.initialValue;
            var.stateName = state->getId();  // W3C SCXML 5.3: Track state for late binding
            model.dataModel.push_back(var);
            dataModelVarNames.insert(var.name);
            LOG_DEBUG("StaticCodeGenerator: Extracted state-level datamodel variable '{}' from state '{}'", var.name,
                      state->getId());
        }
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

        // W3C SCXML 3.3: Track parent state for hierarchical entry
        auto parent = state->getParent();
        if (parent) {
            stateInfo.parentState = parent->getId();
            LOG_DEBUG("StaticCodeGenerator: State '{}' has parent '{}'", stateId, stateInfo.parentState);
        } else {
            stateInfo.parentState = "";  // Root state
        }

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

        // W3C SCXML 6.4: Extract invoke elements
        for (const auto &invokeNode : state->getInvoke()) {
            InvokeInfo invokeInfo;
            invokeInfo.invokeId = invokeNode->getId();
            invokeInfo.type = invokeNode->getType();
            invokeInfo.src = invokeNode->getSrc();
            invokeInfo.srcExpr = invokeNode->getSrcExpr();
            invokeInfo.autoforward = invokeNode->isAutoForward();
            invokeInfo.finalizeContent = invokeNode->getFinalize();
            invokeInfo.namelist = invokeNode->getNamelist();
            invokeInfo.content = invokeNode->getContent();
            invokeInfo.contentExpr = invokeNode->getContentExpr();
            invokeInfo.params = invokeNode->getParams();

            stateInfo.invokes.push_back(invokeInfo);
            LOG_DEBUG("StaticCodeGenerator: State '{}' has invoke: id='{}', type='{}', src='{}', autoforward={}",
                      stateId, invokeInfo.invokeId, invokeInfo.type, invokeInfo.src, invokeInfo.autoforward);
        }

        // W3C SCXML 5.5/5.7: Extract donedata for final states
        if (stateInfo.isFinal) {
            const auto &doneData = state->getDoneData();
            stateInfo.doneData.content = doneData.getContent();

            const auto &params = doneData.getParams();
            for (const auto &param : params) {
                stateInfo.doneData.params.push_back({param.first, param.second});
                LOG_DEBUG("StaticCodeGenerator: Final state '{}' has donedata param: name='{}', location='{}'", stateId,
                          param.first, param.second);
            }

            if (!stateInfo.doneData.content.empty() || !stateInfo.doneData.params.empty()) {
                LOG_DEBUG("StaticCodeGenerator: Final state '{}' has donedata: content='{}', {} params", stateId,
                          stateInfo.doneData.content, stateInfo.doneData.params.size());
            }
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

                // W3C SCXML 3.5: Extract actions from transition (executed during transition)
                for (const auto &actionNode : transition->getActionNodes()) {
                    std::string actionType = actionNode->getActionType();

                    if (actionType == "script") {
                        if (auto scriptAction = std::dynamic_pointer_cast<RSM::ScriptAction>(actionNode)) {
                            const std::string &content = scriptAction->getContent();
                            auto extractedFuncs = extractFunctionNames(content, funcRegex);
                            trans.actions.insert(trans.actions.end(), extractedFuncs.begin(), extractedFuncs.end());
                        }
                    } else if (actionType == "assign") {
                        if (auto assignAction = std::dynamic_pointer_cast<RSM::AssignAction>(actionNode)) {
                            Action act(Action::ASSIGN, assignAction->getLocation(), assignAction->getExpr());
                            trans.transitionActions.push_back(act);
                        }
                    } else if (actionType == "send") {
                        // W3C SCXML 3.5: Send actions in transitions (test226, test276)
                        if (auto sendAction = std::dynamic_pointer_cast<RSM::SendAction>(actionNode)) {
                            std::string event = sendAction->getEvent();
                            std::string target = sendAction->getTarget();
                            std::string targetExpr = sendAction->getTargetExpr();
                            std::string eventExpr = sendAction->getEventExpr();
                            std::string delay = sendAction->getDelay();
                            std::string delayExpr = sendAction->getDelayExpr();

                            // W3C SCXML 6.2: Detect send to parent (requires parent pointer and template)
                            if (target == "#_parent") {
                                model.hasSendToParent = true;
                                LOG_DEBUG("StaticCodeGenerator: Detected #_parent in transition action");
                            }

                            Action sendActionResult(Action::SEND, event, target, targetExpr, eventExpr, delay,
                                                    delayExpr);
                            // W3C SCXML 5.10: Extract send params for event data
                            for (const auto &param : sendAction->getParamsWithExpr()) {
                                sendActionResult.sendParams.push_back({param.name, param.expr});
                            }
                            sendActionResult.sendContent = sendAction->getContent();
                            sendActionResult.sendContentExpr = sendAction->getContentExpr();
                            sendActionResult.sendId = sendAction->getSendId();
                            sendActionResult.sendIdLocation = sendAction->getIdLocation();
                            sendActionResult.sendType = sendAction->getType();
                            trans.transitionActions.push_back(sendActionResult);
                        }
                    } else if (actionType == "raise") {
                        // W3C SCXML 3.5: Raise actions in transitions
                        if (auto raiseAction = std::dynamic_pointer_cast<RSM::RaiseAction>(actionNode)) {
                            trans.transitionActions.push_back(Action(Action::RAISE, raiseAction->getEvent()));
                        }
                    } else if (actionType == "log") {
                        // W3C SCXML 3.5: Log actions in transitions
                        if (auto logAction = std::dynamic_pointer_cast<RSM::LogAction>(actionNode)) {
                            trans.transitionActions.push_back(Action(Action::LOG, logAction->getExpr()));
                        }
                    } else if (actionType == "cancel") {
                        // W3C SCXML 6.3: Cancel actions in transitions
                        if (auto cancelAction = std::dynamic_pointer_cast<RSM::CancelAction>(actionNode)) {
                            std::string sendId = cancelAction->getSendId();
                            std::string sendIdExpr = cancelAction->getSendIdExpr();
                            trans.transitionActions.push_back(Action(Action::CANCEL, sendId, sendIdExpr));
                        }
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
                // W3C SCXML 6.2: Detect send to parent (requires parent pointer and template)
                if (action.param2 == "#_parent") {  // param2 is target
                    model.hasSendToParent = true;
                    LOG_DEBUG("StaticCodeGenerator: Detected #_parent in detectFeatures");
                }
                // W3C SCXML 6.2: Detect send with delay (requires EventScheduler)
                if (!action.param5.empty() || !action.param6.empty()) {
                    model.hasSendWithDelay = true;
                }
                // W3C SCXML 5.10: Detect send with params or content (requires event data support)
                // - sendParams: <param> elements for structured event data
                // - sendContent: <content> element for raw event data (test179)
                // - sendContentExpr: <content expr="..."> for dynamic content evaluation
                if (!action.sendParams.empty() || !action.sendContent.empty() || !action.sendContentExpr.empty()) {
                    model.hasSendParams = true;
                }
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

    // W3C SCXML 3.5: Detect features in transition actions
    for (const auto &transition : model.transitions) {
        detectFeatures(transition.transitionActions);
    }

    // Detect complex datamodel (arrays, typeof)
    for (const auto &var : model.dataModel) {
        if (var.initialValue.find('[') != std::string::npos || var.initialValue.find('{') != std::string::npos) {
            model.hasComplexDatamodel = true;
        }
    }

    // Helper function to detect ECMAScript features in expressions
    auto detectECMAScriptFeatures = [&model](const std::string &expr) {
        if (expr.find("typeof") != std::string::npos) {
            model.hasComplexDatamodel = true;
        }
        // W3C SCXML 5.10: _event access requires JSEngine (test179, test319)
        if (expr.find("_event") != std::string::npos) {
            model.hasComplexECMAScript = true;
        }
        // W3C SCXML 5.9.2: In() predicate requires JSEngine (test310)
        if (expr.find("In(") != std::string::npos) {
            model.hasComplexECMAScript = true;
        }
    };

    // Helper function to recursively check actions for ECMAScript features
    std::function<void(const std::vector<Action> &)> checkActionsForECMAScript;
    checkActionsForECMAScript = [&](const std::vector<Action> &actions) {
        for (const auto &action : actions) {
            if (action.type == Action::IF) {
                // Check all branch conditions
                for (const auto &branch : action.branches) {
                    if (!branch.condition.empty()) {
                        detectECMAScriptFeatures(branch.condition);
                    }
                    // Recursively check actions in each branch
                    checkActionsForECMAScript(branch.actions);
                }
            } else if (action.type == Action::FOREACH) {
                // Check foreach iteration actions
                checkActionsForECMAScript(action.iterationActions);
            } else if (action.type == Action::ASSIGN && !action.param2.empty()) {
                // Check assign expressions
                detectECMAScriptFeatures(action.param2);
            }
        }
    };

    // Detect typeof and _event in transition guards (requires JSEngine)
    for (const auto &trans : model.transitions) {
        if (!trans.guard.empty()) {
            detectECMAScriptFeatures(trans.guard);
        }
    }

    // Detect ECMAScript features in state actions
    for (const auto &state : model.states) {
        checkActionsForECMAScript(state.entryActions);
        checkActionsForECMAScript(state.exitActions);
    }

    LOG_INFO("StaticCodeGenerator: Feature detection - forEach: {}, complexDatamodel: {}, needsJSEngine: {}",
             model.hasForEach, model.hasComplexDatamodel, model.needsJSEngine());

    // Step 5.5: Check for dynamic invokes BEFORE attempting child SCXML generation
    // W3C SCXML 6.4: Dynamic invoke detection - use Interpreter engine for entire SCXML
    // ARCHITECTURE.md: No hybrid approach - either fully JIT or fully Interpreter
    bool hasInvokes = false;
    bool hasDynamicInvokes = false;
    for (const auto &state : model.states) {
        if (!state.invokes.empty()) {
            hasInvokes = true;
            // Check for dynamic invokes
            for (const auto &invoke : state.invokes) {
                bool isStaticInvoke =
                    (invoke.type.empty() || invoke.type == "scxml" || invoke.type == "http://www.w3.org/TR/scxml/") &&
                    !invoke.src.empty() && invoke.srcExpr.empty() && invoke.content.empty() &&
                    invoke.contentExpr.empty();
                if (!isStaticInvoke) {
                    hasDynamicInvokes = true;
                    break;
                }
            }
        }
        if (hasDynamicInvokes) {
            break;
        }
    }

    // ARCHITECTURE.md All-or-Nothing: Use Interpreter wrapper if dynamic invokes OR JSEngine needed
    if (hasDynamicInvokes) {
        LOG_INFO("StaticCodeGenerator: Dynamic invoke detected in '{}' - generating Interpreter wrapper", model.name);
        std::stringstream ss;
        return generateInterpreterWrapper(ss, model, rsmModel, scxmlPath, outputDir);
    }

    // Step 5.6: Generate child SCXML code for static invokes (W3C SCXML 6.4)
    // Only reached if no dynamic invokes detected above
    std::set<std::string> childIncludes;  // Track generated child headers for include directives

    // Track static invoke information for member generation
    std::vector<StaticCodeGenerator::StaticInvokeInfo> staticInvokes;
    for (const auto &state : model.states) {
        for (const auto &invoke : state.invokes) {
            // Check if this is a static invoke (compile-time child SCXML)
            // W3C SCXML types: empty, "scxml", or "http://www.w3.org/TR/scxml/"
            bool isStaticInvoke =
                (invoke.type.empty() || invoke.type == "scxml" || invoke.type == "http://www.w3.org/TR/scxml/") &&
                !invoke.src.empty() && invoke.srcExpr.empty();

            if (isStaticInvoke) {
                // Extract child SCXML path
                std::string childSrc = invoke.src;

                // Handle file: prefix (e.g., "file:test239sub1.scxml")
                if (childSrc.find("file:") == 0) {
                    childSrc = childSrc.substr(5);  // Remove "file:" prefix
                }

                // Resolve child path relative to parent SCXML directory
                fs::path parentPath(scxmlPath);
                fs::path parentDir = parentPath.parent_path();
                fs::path childPath = parentDir / childSrc;

                // Check if child SCXML exists
                if (!fs::exists(childPath)) {
                    LOG_ERROR("StaticCodeGenerator: Child SCXML file not found: {} (referenced from {})",
                              childPath.string(), scxmlPath);
                    return false;
                }

                // Extract child class name for include
                std::string childFileName = childPath.stem().string();
                std::string childInclude = childFileName + "_sm.h";

                // Skip if already generated (avoid duplicate generation)
                if (childIncludes.find(childInclude) != childIncludes.end()) {
                    LOG_DEBUG("StaticCodeGenerator: Child '{}' already generated, skipping", childFileName);
                    continue;
                }

                // Recursively generate child SCXML code
                LOG_INFO("StaticCodeGenerator: Generating child SCXML: {}", childPath.string());
                if (!generate(childPath.string(), outputDir)) {
                    LOG_ERROR("StaticCodeGenerator: Failed to generate child SCXML: {}", childPath.string());
                    return false;
                }

                // Track this child for include directive
                childIncludes.insert(childInclude);
                LOG_DEBUG("StaticCodeGenerator: Child '{}' generated successfully", childFileName);

                // W3C SCXML 6.2: Check if child uses #_parent by reading generated header
                // ARCHITECTURE.md All-or-Nothing: Check if child is Interpreter wrapper (uses JSEngine/dynamic
                // features)
                bool childUsesParent = false;
                bool childIsInterpreterWrapper = false;
                {
                    fs::path childHeaderPath = fs::path(outputDir) / (childFileName + "_sm.h");
                    LOG_DEBUG("StaticCodeGenerator: Checking child header: {}", childHeaderPath.string());
                    if (fs::exists(childHeaderPath)) {
                        LOG_DEBUG("StaticCodeGenerator: Child header exists");
                        std::ifstream childHeaderFile(childHeaderPath);
                        if (childHeaderFile.is_open()) {
                            std::string line;
                            while (std::getline(childHeaderFile, line)) {
                                // Detect template-based parent pointer (indicates #_parent usage)
                                if (line.find("template<typename ParentStateMachine>") != std::string::npos) {
                                    childUsesParent = true;
                                    LOG_DEBUG("StaticCodeGenerator: Child '{}' uses #_parent (found template)",
                                              childFileName);
                                }
                                // Detect Interpreter wrapper (All-or-Nothing principle)
                                if (line.find("#include \"runtime/StateMachine.h\"") != std::string::npos ||
                                    line.find("std::shared_ptr<::RSM::StateMachine> interpreter_") !=
                                        std::string::npos ||
                                    line.find("W3C SCXML 6.4: Dynamic invoke detected") != std::string::npos) {
                                    childIsInterpreterWrapper = true;
                                    LOG_DEBUG("StaticCodeGenerator: Child '{}' is Interpreter wrapper", childFileName);
                                }
                            }
                        }
                    } else {
                        LOG_WARN("StaticCodeGenerator: Child header does not exist: {}", childHeaderPath.string());
                    }
                }
                LOG_DEBUG("StaticCodeGenerator: childUsesParent = {}, childIsInterpreterWrapper = {}", childUsesParent,
                          childIsInterpreterWrapper);

                // ARCHITECTURE.md All-or-Nothing: If child uses Interpreter, parent must too
                if (childIsInterpreterWrapper) {
                    LOG_WARN("StaticCodeGenerator: Child '{}' uses Interpreter wrapper - parent '{}' must also use "
                             "Interpreter (All-or-Nothing)",
                             childFileName, model.name);
                    std::stringstream ss;
                    return generateInterpreterWrapper(ss, model, rsmModel, scxmlPath, outputDir);
                }

                // Track static invoke info for member generation
                std::string invokeId = invoke.invokeId;
                if (invokeId.empty()) {
                    // Auto-generate invoke ID
                    size_t invokeCount =
                        std::count_if(staticInvokes.begin(), staticInvokes.end(),
                                      [&state](const StaticInvokeInfo &info) { return info.stateName == state.name; });
                    invokeId = state.name + "_invoke_" + std::to_string(invokeCount);
                }

                StaticInvokeInfo info;
                info.invokeId = invokeId;
                info.childName = childFileName;
                info.stateName = state.name;
                info.autoforward = invoke.autoforward;
                info.finalizeContent = invoke.finalizeContent;
                info.childNeedsParent = childUsesParent;  // W3C SCXML 6.2: Set parent requirement flag
                staticInvokes.push_back(info);
            }
        }
    }

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
    ss << "#include <unordered_map>\n";
    ss << "#include \"static/StaticExecutionEngine.h\"\n";

    // W3C SCXML 5.10: _event metadata access requires Interpreter (test198)
    // Static engine doesn't track event metadata (origintype, sendid, invokeid, etc.)
    // ARCHITECTURE.md: No hybrid approach - fall back to Interpreter for entire SCXML
    bool hasEventMetadata = false;
    for (const auto &trans : model.transitions) {
        // Check for _event.origintype, _event.sendid, _event.invokeid, _event.origin, etc.
        // But not _event.data or _event.name which static engine can handle
        if (trans.guard.find("_event.origintype") != std::string::npos ||
            trans.guard.find("_event.sendid") != std::string::npos ||
            trans.guard.find("_event.invokeid") != std::string::npos ||
            trans.guard.find("_event.origin") != std::string::npos ||
            trans.guard.find("_event.type") != std::string::npos) {
            hasEventMetadata = true;
            break;
        }
    }

    if (hasEventMetadata) {
        LOG_INFO("StaticCodeGenerator: Event metadata access detected in '{}' - generating Interpreter wrapper",
                 model.name);
        return generateInterpreterWrapper(ss, model, rsmModel, scxmlPath, outputDir);
    }

    // W3C SCXML 6.2 (test199): Unsupported send type requires TypeRegistry validation
    // Static engine cannot validate send types at compile-time - requires runtime TypeRegistry
    // ARCHITECTURE.md: No hybrid approach - fall back to Interpreter for entire SCXML
    auto isSupportedSendType = [](const std::string &sendType) -> bool {
        return sendType.empty() || sendType == "scxml" || sendType == "http://www.w3.org/TR/scxml/" ||
               sendType == "http://www.w3.org/TR/scxml/#SCXMLEventProcessor";
    };

    auto hasUnsupportedSend = [&](const std::vector<Action> &actions) -> bool {
        for (const auto &action : actions) {
            if (action.type == Action::SEND && !isSupportedSendType(action.sendType)) {
                return true;
            }
        }
        return false;
    };

    bool hasUnsupportedSendType = false;
    for (const auto &state : model.states) {
        if (hasUnsupportedSend(state.entryActions) || hasUnsupportedSend(state.exitActions)) {
            hasUnsupportedSendType = true;
            break;
        }
    }

    if (hasUnsupportedSendType) {
        LOG_INFO("StaticCodeGenerator: Unsupported send type detected in '{}' - generating Interpreter wrapper",
                 model.name);
        return generateInterpreterWrapper(ss, model, rsmModel, scxmlPath, outputDir);
    }

    // W3C SCXML 6.3 (test208): sendidexpr requires runtime evaluation
    // Static engine can only handle literal sendid values, not dynamic expressions
    // ARCHITECTURE.md: sendid="foo" → Static, sendidexpr="variable" → Interpreter
    auto hasCancelWithExpr = [](const std::vector<Action> &actions) -> bool {
        for (const auto &action : actions) {
            // param1 = sendid (literal), param2 = sendidexpr (expression)
            if (action.type == Action::CANCEL && !action.param2.empty()) {
                return true;
            }
        }
        return false;
    };

    bool hasDynamicCancel = false;
    for (const auto &state : model.states) {
        if (hasCancelWithExpr(state.entryActions) || hasCancelWithExpr(state.exitActions)) {
            hasDynamicCancel = true;
            break;
        }
    }

    if (hasDynamicCancel) {
        LOG_INFO("StaticCodeGenerator: Dynamic cancel (sendidexpr) detected in '{}' - generating Interpreter wrapper",
                 model.name);
        return generateInterpreterWrapper(ss, model, rsmModel, scxmlPath, outputDir);
    }

    if (hasInvokes) {
        // W3C SCXML 6.4: Include child SCXML headers for static invokes
        if (!childIncludes.empty()) {
            ss << "\n// Child SCXML headers (static invokes)\n";
            for (const auto &childInclude : childIncludes) {
                ss << "#include \"" << childInclude << "\"\n";
            }
        }
    }
    // Add SendHelper include if needed
    if (model.hasSend) {
        ss << "#include \"common/SendHelper.h\"\n";
    }
    // W3C SCXML 6.2: Add SendSchedulingHelper for delayed send
    if (model.needsEventScheduler()) {
        ss << "#include \"common/SendSchedulingHelper.h\"\n";
    }
    // TransitionHelper for W3C SCXML 3.12 event matching (Zero Duplication)
    ss << "#include \"common/TransitionHelper.h\"\n";
    // EventDataHelper for W3C SCXML 5.10 event data construction (Zero Duplication)
    if (model.hasSendParams) {
        ss << "#include \"common/EventDataHelper.h\"\n";
    }

    // Add JSEngine and Logger includes if needed for hybrid code generation (OUTSIDE namespace)
    if (model.needsJSEngine()) {
        ss << "#include <optional>\n";
        ss << "#include \"common/Logger.h\"\n";
        ss << "#include \"scripting/JSEngine.h\"\n";
        ss << "#include \"common/AssignHelper.h\"\n";
        ss << "#include \"common/ForeachValidator.h\"\n";
        ss << "#include \"common/ForeachHelper.h\"\n";
        ss << "#include \"common/GuardHelper.h\"\n";
    }
    ss << "\n";

    // Namespace - each test gets its own nested namespace to avoid conflicts
    ss << "namespace RSM::Generated::" << model.name << " {\n\n";

    // W3C SCXML 6.2: Forward declaration (template if child sends to parent)
    if (model.hasSendToParent) {
        ss << "template<typename ParentStateMachine>\n";
    }
    ss << "class " << model.name << ";\n\n";

    // Generate State enum
    ss << generateStateEnum(states);
    ss << "\n";

    // W3C SCXML 5.3: Add error.execution to events if JSEngine is used (for datamodel init failures)
    if (model.needsJSEngine() && events.find("error.execution") == events.end()) {
        events.insert("error.execution");
    }

    // Generate Event enum
    ss << generateEventEnum(events);
    ss << "\n";

    // Generate base class template (pass staticInvokes for member generation)
    ss << generateClass(model, staticInvokes);

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
    // W3C SCXML 6.2: Add NONE as first enum value for scheduler polling
    // Event() (default constructor) will be Event::NONE, which never matches transitions
    // This allows tick() method to poll scheduler without triggering unwanted transitions
    std::stringstream ss;
    ss << "enum class Event : uint8_t {\n";
    ss << "    NONE,  // W3C SCXML 6.2: Default event for scheduler polling (no semantic meaning)\n";

    size_t idx = 0;
    for (const auto &value : events) {
        ss << "    " << capitalize(value);
        if (idx < events.size() - 1) {
            ss << ",";
        }
        ss << "\n";
        idx++;
    }

    ss << "};\n";
    return ss.str();
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

std::string StaticCodeGenerator::generateProcessEvent(const SCXMLModel &model, const std::set<std::string> &events,
                                                      const std::vector<StaticInvokeInfo> &staticInvokes) {
    std::stringstream ss;

    // W3C SCXML 5.3: Datamodel initialization error handling
    // Pattern: Deferred error.execution raising to ensure event priority correctness
    // Execution Flow:
    //   1. ensureJSEngine() triggers lazy initialization, sets datamodelInitFailed_ on error
    //   2. Raise error.execution and return early (defers to next tick)
    //   3. Next tick processes error.execution with higher priority than onentry events
    // Rationale: Prevents onentry raised events (e.g., "foo") from being processed before
    //            error.execution, which could cause wildcard transitions to match incorrectly
    if (model.needsJSEngine()) {
        ss << "        // W3C SCXML 5.3: Ensure JSEngine initialized to detect datamodel errors\n";
        ss << "        this->ensureJSEngine();\n";
        ss << "\n";
        ss << "        // W3C SCXML 5.3: Raise error.execution and defer to next tick\n";
        ss << "        // Deferred processing ensures error.execution has priority over onentry events\n";
        ss << "        if (datamodelInitFailed_) {\n";
        ss << "            datamodelInitFailed_ = false;  // Clear flag\n";
        ss << "            engine.raise(Event::Error_execution);\n";
        ss << "            return false;  // Defer to next tick (prevents wildcard transition mismatch)\n";
        ss << "        }\n";
        ss << "\n";
    }

    // W3C SCXML 5.10: Store current event name for _event.name access (test318)
    if (model.needsJSEngine()) {
        ss << "        // W3C SCXML 5.10: Store event name for _event.name binding\n";
        ss << "        if (event != Event()) {  // Skip for eventless transitions\n";
        ss << "            pendingEventName_ = getEventName(event);\n";
        ss << "        }\n";
        ss << "\n";
    }

    // W3C SCXML 5.10: Set _event variable in JSEngine (test176, test318)
    if (model.needsJSEngine()) {
        ss << "        // W3C SCXML 5.10: Set _event variable in JSEngine (name + data)\n";
        if (model.hasSendParams) {
            ss << "        if (!pendingEventName_.empty() || !pendingEventData_.empty()) {\n";
            ss << "            setCurrentEventInJSEngine(pendingEventName_, pendingEventData_);\n";
            ss << "            // Keep pendingEventName_ for next state's onentry (W3C SCXML 5.10 - test318)\n";
            ss << "            // Only clear after state transition completes\n";
            ss << "            pendingEventData_.clear();  // Clear data immediately\n";
            ss << "        }\n";
        } else {
            ss << "        if (!pendingEventName_.empty()) {\n";
            ss << "            setCurrentEventInJSEngine(pendingEventName_);\n";
            ss << "        }\n";
        }
        ss << "\n";
    }

    // W3C SCXML 6.4: Check pending done.invoke events from child completion
    if (!staticInvokes.empty()) {
        ss << "        // W3C SCXML 6.4: Check for pending done.invoke events\n";
        for (const auto &invokeInfo : staticInvokes) {
            ss << "        if (pendingDoneInvoke_" << invokeInfo.invokeId << "_) {\n";
            ss << "            pendingDoneInvoke_" << invokeInfo.invokeId << "_ = false;\n";
            ss << "            LOG_DEBUG(\"Raising done.invoke for " << invokeInfo.invokeId << "\");\n";
            ss << "            engine.raise(Event::Done_invoke);\n";
            ss << "        }\n";
        }
        ss << "\n";
    }

    // W3C SCXML 6.2: Check for ready scheduled events
    if (model.needsEventScheduler()) {
        ss << "        // W3C SCXML 6.2: Process ready scheduled events\n";
        ss << "        {\n";
        ss << "            Event scheduledEvent;\n";
        if (model.hasSendParams && model.needsJSEngine()) {
            ss << "            std::string eventData;\n";
            ss << "            while (eventScheduler_.popReadyEvent(scheduledEvent, eventData)) {\n";
            ss << "                if (!eventData.empty()) {\n";
            ss << "                    pendingEventData_ = eventData;\n";
            ss << "                }\n";
            ss << "                engine.raise(scheduledEvent);\n";
            ss << "            }\n";
        } else {
            ss << "            while (eventScheduler_.popReadyEvent(scheduledEvent)) {\n";
            ss << "                engine.raise(scheduledEvent);\n";
            ss << "            }\n";
        }
        ss << "        }\n";
        ss << "\n";
    }

    // W3C SCXML 6.4-6.5: Generate direct invoke processing (JIT approach)
    if (!staticInvokes.empty()) {
        // Check if any child has finalize content
        bool hasFinalize = false;
        for (const auto &invokeInfo : staticInvokes) {
            if (!invokeInfo.finalizeContent.empty()) {
                hasFinalize = true;
                break;
            }
        }

        if (hasFinalize) {
            ss << "        // W3C SCXML 6.5: Finalize - Execute handler if event from child\n";
            ss << "        const std::string& originSessionId = currentEventMetadata_.originSessionId;\n";
        }

        // Generate finalize checks for each child with finalize content
        for (const auto &invokeInfo : staticInvokes) {
            if (!invokeInfo.finalizeContent.empty()) {
                ss << "        if (child_" << invokeInfo.invokeId << "_ && !originSessionId.empty()) {\n";
                ss << "            std::string childSessionId_" << invokeInfo.invokeId
                   << " = sessionId_.value() + \"_\" + \"" << invokeInfo.invokeId << "\";\n";
                ss << "            if (originSessionId == childSessionId_" << invokeInfo.invokeId << ") {\n";
                ss << "                // W3C SCXML 6.5: Execute finalize script\n";
                ss << "                this->ensureJSEngine();\n";
                ss << "                auto& jsEngine = ::RSM::JSEngine::instance();\n";
                ss << "                std::string finalizeScript = R\"(" << invokeInfo.finalizeContent << ")\";\n";
                ss << "                auto result = jsEngine.evaluateScript(sessionId_.value(), "
                      "finalizeScript).get();\n";
                ss << "                if (!::RSM::JSEngine::isSuccess(result)) {\n";
                ss << "                    LOG_ERROR(\"Finalize script execution failed for " << invokeInfo.invokeId
                   << "\");\n";
                ss << "                }\n";
                ss << "            }\n";
                ss << "        }\n";
            }
        }

        ss << "\n";
        ss << "        // W3C SCXML 6.4.1: Autoforward - Forward events to children\n";

        // Generate autoforward for each child with autoforward=true
        for (const auto &invokeInfo : staticInvokes) {
            if (invokeInfo.autoforward) {
                ss << "        if (child_" << invokeInfo.invokeId << "_) {\n";
                ss << "            // Autoforward event to child: " << invokeInfo.invokeId << "\n";
                ss << "            // TODO: Implement compile-time event mapping\n";
                ss << "            // child_" << invokeInfo.invokeId << "_->processEvent(mappedEvent);\n";
                ss << "        }\n";
            }
        }

        ss << "\n";
    }

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

        // Track if unconditional break was added (for avoiding duplicate breaks)
        bool hasUnconditionalBreak = false;

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

                // W3C SCXML 3.12.1: Separate wildcard transitions (event="*") for special handling
                // Wildcards match any event and should be checked after specific events
                std::vector<Transition> wildcardTransitions;
                for (auto it = transitionsByEvent.begin(); it != transitionsByEvent.end();) {
                    if (it->first == "*") {
                        wildcardTransitions = it->second;
                        it = transitionsByEvent.erase(it);
                    } else {
                        ++it;
                    }
                }

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
                            bool needsJSEngine = model.needsJSEngine() ||
                                                 (guardExpr.find("typeof") != std::string::npos) ||
                                                 (guardExpr.find("_event") != std::string::npos);
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
                            // W3C SCXML 5.5/5.7: Generate donedata handling if transitioning to final state
                            generateDoneDataCode(ss, trans.targetState, model, guardIndent);
                        }
                        ss << guardIndent << "transitionTaken = true;\n";
                    }

                    // Close guard chain if we had any guards
                    if (!firstGuard) {
                        ss << eventIndent << "}\n";
                    }
                }

                // W3C SCXML 3.12.1: Generate wildcard transitions (event="*") as catch-all
                // Wildcards match any event except Event::NONE (scheduler polling event)
                if (!wildcardTransitions.empty()) {
                    if (!firstEvent) {
                        // Add else if after specific event checks
                        ss << baseIndent << "} else if (event != Event::NONE) {\n";
                    } else {
                        // No specific events, just check for non-NONE
                        ss << baseIndent << "if (event != Event::NONE) {\n";
                    }

                    const std::string eventIndent = baseIndent + "    ";

                    // Generate guard-based transition chain for wildcard
                    bool firstGuard = true;
                    for (const auto &trans : wildcardTransitions) {
                        bool hasGuard = !trans.guard.empty();
                        std::string guardIndent = eventIndent;

                        if (hasGuard) {
                            // Guarded transition
                            std::string guardExpr = trans.guard;
                            bool needsJSEngine = model.needsJSEngine() ||
                                                 (guardExpr.find("typeof") != std::string::npos) ||
                                                 (guardExpr.find("_event") != std::string::npos);
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

                        // W3C SCXML: Execute transition actions
                        for (const auto &action : trans.transitionActions) {
                            generateActionCode(ss, action, "engine", events, model);
                        }

                        // W3C SCXML: Only change state if targetState exists
                        if (!trans.targetState.empty()) {
                            ss << guardIndent << "currentState = State::" << capitalize(trans.targetState) << ";\n";
                            // W3C SCXML 5.5/5.7: Generate donedata handling if transitioning to final state
                            generateDoneDataCode(ss, trans.targetState, model, guardIndent);
                        }
                        ss << guardIndent << "transitionTaken = true;\n";
                    }

                    // Close guard chain if we had any guards
                    if (!firstGuard) {
                        ss << eventIndent << "}\n";
                    }

                    // Close wildcard event check
                    ss << baseIndent << "}\n";
                } else {
                    // No wildcard transitions, close event chain normally
                    if (!firstEvent) {
                        ss << baseIndent << "}\n";
                    }
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
                        bool needsJSEngine = model.needsJSEngine() || (guardExpr.find("typeof") != std::string::npos) ||
                                             (guardExpr.find("_event") != std::string::npos);
                        bool isFunctionCall = (guardExpr.find("()") != std::string::npos);

                        // Check if first transition also uses JSEngine (to reuse jsEngine variable)
                        bool firstUsesJSEngine = false;
                        if (!firstTransition && !eventlessTransitions.empty() &&
                            !eventlessTransitions[0].guard.empty()) {
                            std::string firstGuard = eventlessTransitions[0].guard;
                            firstUsesJSEngine = model.needsJSEngine() ||
                                                (firstGuard.find("typeof") != std::string::npos) ||
                                                (firstGuard.find("_event") != std::string::npos);
                        }

                        if (needsJSEngine) {
                            // Interpreter Engine or complex guard → JSEngine evaluation using GuardHelper
                            if (firstTransition) {
                                // First guard: initialize JSEngine in its own scope
                                ss << indent << "{\n";  // Open scope for JSEngine variables
                                ss << indent << "    this->ensureJSEngine();\n";
                                ss << indent << "    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                                ss << indent
                                   << "    if (::RSM::GuardHelper::evaluateGuard(jsEngine, sessionId_.value(), \""
                                   << escapeStringLiteral(guardExpr) << "\")) {\n";
                                indent += "        ";  // Indent for code inside the if block
                            } else if (firstUsesJSEngine) {
                                // Subsequent guard: reuse jsEngine from first guard (avoid redefinition)
                                ss << "                    } else {\n";
                                ss << "                        if (::RSM::GuardHelper::evaluateGuard(jsEngine, "
                                      "sessionId_.value(), \""
                                   << escapeStringLiteral(guardExpr) << "\")) {\n";
                                indent = "                            ";  // Indent for code inside the nested if block
                            } else {
                                // Subsequent guard: first didn't use JSEngine, so initialize it now
                                ss << "                } else {\n";
                                ss << "                    this->ensureJSEngine();\n";
                                ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                                ss << "                    if (::RSM::GuardHelper::evaluateGuard(jsEngine, "
                                      "sessionId_.value(), \""
                                   << escapeStringLiteral(guardExpr) << "\")) {\n";
                                indent = "                        ";  // Indent for code inside the if block
                            }
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
                        // W3C SCXML 5.5/5.7: Generate donedata handling if transitioning to final state
                        generateDoneDataCode(ss, trans.targetState, model, indent);
                    }
                    ss << indent << "transitionTaken = true;\n";

                    // W3C SCXML 3.5: Unconditional transitions execute immediately and stop further processing
                    if (trans.guard.empty() && firstTransition) {
                        ss << indent << "break;\n";
                        hasUnconditionalBreak = true;  // Mark that we added unconditional break
                    }

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
                            // Check if last transition also has a guard (nested if/else) or is unguarded (simple else)
                            bool lastHasGuard =
                                !eventlessTransitions.empty() && !eventlessTransitions.back().guard.empty();

                            if (lastHasGuard) {
                                // Scenario: { ensureJS; if (...) {} else { if (...) {} } }
                                // Close the nested 'if' and the 'else' block
                                ss << "                        }\n";  // Close nested 'if' from line 1363
                                ss << "                    }\n";      // Close 'else' from line 1362
                            } else {
                                // Scenario: { ensureJS; if (...) {} else {...} }
                                // Close only the 'else' block (no nested if to close)
                                ss << "                }\n";  // Close 'else' from line 1404
                            }
                            // Close the outer scope block (both scenarios)
                            ss << "                }\n";  // Close '{' from line 1354
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

                        // W3C SCXML 3.12.1: Separate wildcard transitions for special handling
                        std::vector<Transition> wildcardTransitions;
                        for (auto it = transitionsByEvent.begin(); it != transitionsByEvent.end();) {
                            if (it->first == "*") {
                                wildcardTransitions = it->second;
                                it = transitionsByEvent.erase(it);
                            } else {
                                ++it;
                            }
                        }

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
                                    // W3C SCXML 5.5/5.7: Generate donedata handling if transitioning to final state
                                    generateDoneDataCode(ss, trans.targetState, model, guardIndent);
                                }

                                ss << guardIndent << "transitionTaken = true;\n";
                            }

                            // Close guard chain if we had any guards
                            if (!firstGuard) {
                                ss << eventIndent << "}\n";
                            }
                        }

                        // W3C SCXML 3.12.1: Generate wildcard transitions as catch-all
                        if (!wildcardTransitions.empty()) {
                            if (!firstEvent) {
                                ss << baseIndent << "} else if (event != Event::NONE) {\n";
                            } else {
                                ss << baseIndent << "if (event != Event::NONE) {\n";
                            }

                            const std::string eventIndent = baseIndent + "    ";

                            bool firstGuard = true;
                            for (const auto &trans : wildcardTransitions) {
                                bool hasGuard = !trans.guard.empty();
                                std::string guardIndent = eventIndent;

                                if (hasGuard) {
                                    if (firstGuard) {
                                        ss << eventIndent << "if (" << trans.guard << ") {\n";
                                        firstGuard = false;
                                    } else {
                                        ss << eventIndent << "} else if (" << trans.guard << ") {\n";
                                    }
                                    guardIndent += "    ";
                                } else {
                                    if (!firstGuard) {
                                        ss << eventIndent << "} else {\n";
                                        guardIndent += "    ";
                                    }
                                }

                                for (const auto &action : trans.transitionActions) {
                                    std::stringstream actionSS;
                                    generateActionCode(actionSS, action, "engine", events, model);
                                    std::string actionCode = actionSS.str();
                                    std::istringstream iss(actionCode);
                                    std::string line;
                                    while (std::getline(iss, line)) {
                                        if (!line.empty()) {
                                            ss << guardIndent << line << "\n";
                                        }
                                    }
                                }

                                if (!trans.targetState.empty()) {
                                    ss << guardIndent << "parallel_" << stateName << "_region" << i
                                       << "State_ = State::" << capitalize(trans.targetState) << ";\n";
                                }

                                ss << guardIndent << "transitionTaken = true;\n";
                            }

                            if (!firstGuard) {
                                ss << eventIndent << "}\n";
                            }

                            ss << baseIndent << "}\n";
                        } else {
                            // No wildcard transitions, close event chain normally
                            if (!firstEvent) {
                                ss << baseIndent << "}\n";
                            }
                        }
                    }

                    ss << "                }\n";
                }
            }

            // Only add break if unconditional break wasn't already added
            if (!eventlessTransitions.empty() && !hasUnconditionalBreak) {
                ss << "                break;\n";
            } else if (eventlessTransitions.empty()) {
                ss << "                break;\n";
            }
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
                // W3C SCXML 6.2: Store send action parameters (event, target, delay, etc.)
                std::string event = sendAction->getEvent();
                std::string target = sendAction->getTarget();
                std::string targetExpr = sendAction->getTargetExpr();
                std::string eventExpr = sendAction->getEventExpr();
                std::string delay = sendAction->getDelay();
                std::string delayExpr = sendAction->getDelayExpr();

                // W3C SCXML 5.10: Extract send params for event data construction
                Action sendActionResult(Action::SEND, event, target, targetExpr, eventExpr, delay, delayExpr);
                for (const auto &param : sendAction->getParamsWithExpr()) {
                    sendActionResult.sendParams.push_back({param.name, param.expr});
                }
                // W3C SCXML 5.10: Extract send content for event data (test179)
                sendActionResult.sendContent = sendAction->getContent();
                // W3C SCXML 5.10: Extract send contentexpr for dynamic event data
                sendActionResult.sendContentExpr = sendAction->getContentExpr();
                // W3C SCXML 6.2.5: Extract id attribute for event tracking/cancellation (test208)
                sendActionResult.sendId = sendAction->getSendId();
                // W3C SCXML 6.2.4: Extract idlocation for sendid storage (test183)
                sendActionResult.sendIdLocation = sendAction->getIdLocation();
                // W3C SCXML 6.2.4: Extract type for event processor (test193)
                sendActionResult.sendType = sendAction->getType();
                result.push_back(sendActionResult);
            }
        } else if (actionType == "cancel") {
            if (auto cancelAction = std::dynamic_pointer_cast<RSM::CancelAction>(actionNode)) {
                // W3C SCXML 6.3: Cancel scheduled send event by sendid
                // param1 = sendid (literal), param2 = sendidexpr (expression)
                std::string sendId = cancelAction->getSendId();
                std::string sendIdExpr = cancelAction->getSendIdExpr();
                result.push_back(Action(Action::CANCEL, sendId, sendIdExpr));
            }
        }
    }

    return result;
}

std::string StaticCodeGenerator::generateClass(const SCXMLModel &model,
                                               const std::vector<StaticInvokeInfo> &staticInvokes) {
    std::stringstream ss;
    std::set<std::string> events = extractEvents(model);

    // W3C SCXML Policy Generation Strategy (ARCHITECTURE.md):
    // Generate stateful Policy when any stateful feature is present
    bool needsStateful = model.needsStatefulPolicy();

    // Feature detection flags (for specific code generation needs)
    bool hasDataModel = !model.dataModel.empty() || model.needsJSEngine();
    bool hasInvokes = false;
    for (const auto &state : model.states) {
        if (!state.invokes.empty()) {
            hasInvokes = true;
            break;
        }
    }

    // Generate State Policy class
    ss << "// State policy for " << model.name << "\n";
    // W3C SCXML 6.2: Determine template parameter type
    bool anyChildNeedsParent = std::any_of(staticInvokes.begin(), staticInvokes.end(),
                                           [](const StaticInvokeInfo &info) { return info.childNeedsParent; });

    if (model.hasSendToParent) {
        // This model sends to parent - needs ParentStateMachine template for parent pointer
        ss << "template<typename ParentStateMachine>\n";
    } else if (anyChildNeedsParent) {
        // This model has children that need parent - needs SelfType for CRTP
        ss << "template<typename SelfType>\n";
    }
    ss << "struct " << model.name << "Policy {\n";
    ss << "    using State = ::RSM::Generated::" << model.name << "::State;\n";
    ss << "    using Event = ::RSM::Generated::" << model.name << "::Event;\n\n";

    // Generate datamodel member variables (for stateful policies)
    if (hasDataModel) {
        ss << "    // Datamodel variables\n";
        for (const auto &var : model.dataModel) {
            // W3C SCXML 5.10: System variables (_sessionid, _name, etc.) handled by JSEngine
            // These require runtime evaluation and cannot be static member variables
            if (var.name == "_sessionid" || var.name == "_name" || var.name == "_ioprocessors" ||
                var.name == "_event") {
                ss << "    // System variable (handled by JSEngine): " << var.name << "\n";
                continue;
            }

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
                // Check if it's a numeric literal
                bool isNumericLiteral = true;
                for (char c : var.initialValue) {
                    if (!std::isdigit(c) && c != '.' && c != '-' && c != '+') {
                        isNumericLiteral = false;
                        break;
                    }
                }

                if (isNumericLiteral) {
                    // It's a number literal, safe for static generation
                    ss << "    int " << var.name << " = " << var.initialValue << ";\n";
                } else {
                    // It's a runtime expression (identifier, function call, etc.)
                    // W3C SCXML 5.10: Requires JSEngine for evaluation
                    ss << "    // Runtime-evaluated variable (handled by JSEngine): " << var.name << " = "
                       << var.initialValue << "\n";
                }
            }
        }

        ss << "\n";
    }
    // Add session ID for JSEngine and/or Invoke (lazy-initialized)
    if (model.needsJSEngine() || hasInvokes) {
        ss << "    // Session ID for JSEngine/Invoke (lazy-initialized)\n";
        ss << "    mutable ::std::optional<::std::string> sessionId_;\n";
    }

    // Add JSEngine initialization flag (to prevent duplicate createSession calls)
    if (model.needsJSEngine()) {
        ss << "    mutable bool jsEngineInitialized_ = false;\n";
        ss << "    mutable bool datamodelInitFailed_ = false;  // W3C SCXML 5.3: Track initialization errors\n";
    }

    // W3C SCXML 5.10: Event data and name for _event variable access (test176, test318)
    if (model.needsJSEngine()) {
        ss << "    // W3C SCXML 5.10: Event name storage for _event.name access (test318)\n";
        ss << "    mutable ::std::string pendingEventName_;\n";
        if (model.hasSendParams) {
            ss << "    // W3C SCXML 5.10: Event data storage for _event.data access\n";
            ss << "    mutable ::std::string pendingEventData_;\n";
        }
    }

    // W3C SCXML 6.2: Add event scheduler for delayed send (lazy-initialized)
    if (model.needsEventScheduler()) {
        ss << "    // W3C SCXML 6.2: Event scheduler for delayed send (lazy-init)\n";
        ss << "    mutable ::RSM::SendSchedulingHelper::SimpleScheduler<Event> eventScheduler_;\n";
    }

    // W3C SCXML 5.10: Current event metadata (for invoke finalize)
    if (hasInvokes) {
        ss << "    // W3C SCXML 5.10: Current event metadata (originSessionId for finalize)\n";
        ss << "    mutable ::RSM::Core::EventMetadata currentEventMetadata_;\n";
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

    if (hasInvokes) {
        // Generate child state machine member variables (W3C SCXML 6.4)
        if (!staticInvokes.empty()) {
            ss << "    // W3C SCXML 6.4: Static invoke child state machines\n";
            for (const auto &invokeInfo : staticInvokes) {
                // W3C SCXML 6.2: If child uses #_parent, declare with template parameter
                if (invokeInfo.childNeedsParent) {
                    // Use SelfType from CRTP pattern
                    ss << "    mutable std::shared_ptr<::RSM::Generated::" << invokeInfo.childName
                       << "::" << invokeInfo.childName << "<SelfType>> child_" << invokeInfo.invokeId << "_;\n";
                } else {
                    ss << "    mutable std::shared_ptr<::RSM::Generated::" << invokeInfo.childName
                       << "::" << invokeInfo.childName << "> child_" << invokeInfo.invokeId << "_;\n";
                }
            }
            ss << "\n";

            // Add pending done.invoke flags
            if (!staticInvokes.empty()) {
                ss << "    // W3C SCXML 6.4: Pending done.invoke flags for child completion\n";
                for (const auto &invokeInfo : staticInvokes) {
                    ss << "    mutable bool pendingDoneInvoke_" << invokeInfo.invokeId << "_ = false;\n";
                }
                ss << "\n";
            }
        }

        ss << "    // W3C SCXML 6.4: Child session tracking\n";
        ss << "    struct ChildSession {\n";
        ss << "        std::string sessionId;           // Child's session ID\n";
        ss << "        std::string invokeId;            // Invoke element ID\n";
        ss << "        std::string parentSessionId;     // Parent's session ID\n";
        ss << "        bool autoforward = false;        // W3C 6.4.1: Autoforward events to child\n";
        ss << "        std::string finalizeScript;      // W3C 6.5: Finalize handler script\n";
        // Note: Child state machines stored as Policy members (child_<invokeId>_)
        ss << "    };\n";
        ss << "\n";
        ss << "    // Active child sessions indexed by invokeId\n";
        ss << "    mutable std::unordered_map<std::string, ChildSession> activeInvokes_;\n";
        ss << "\n";
        // Note: Dynamic invoke handling moved to early return (line 433-435)
        // ARCHITECTURE.md: All-or-Nothing strategy prevents reaching this code with dynamic invokes
    }

    // W3C SCXML 6.2: Parent pointer member when this model sends to parent
    if (model.hasSendToParent) {
        ss << "    // W3C SCXML 6.2: Parent state machine pointer for #_parent support\n";
        ss << "    ParentStateMachine* parent_ = nullptr;\n\n";
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

    // W3C SCXML 3.3: Generate parent state mapping using helper method
    ss << generateGetParentMethod(model);

    // Execute entry actions
    ss << "    template<typename Engine>\n";
    if (needsStateful) {
        ss << "    void executeEntryActions(State state, Engine& engine) {\n";  // non-static for stateful
    } else {
        ss << "    static void executeEntryActions(State state, Engine& engine) {\n";  // static for stateless
    }
    ss << "        (void)engine;\n";
    ss << "        switch (state) {\n";
    for (const auto &state : model.states) {
        bool hasEntryActions = !state.entryActions.empty();
        bool needsParallelInit = state.isParallel && !state.childRegions.empty();
        bool hasInvoke = !state.invokes.empty();

        // W3C SCXML 5.3: Check for state-local datamodel variables (late binding)
        bool hasStateLocalVars = false;
        for (const auto &var : model.dataModel) {
            if (var.stateName == state.name) {
                hasStateLocalVars = true;
                break;
            }
        }

        if (hasEntryActions || needsParallelInit || hasInvoke || hasStateLocalVars) {
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

            // W3C SCXML 5.3: Initialize state-local datamodel variables on state entry
            // Use BindingHelper (Single Source of Truth) for binding semantics
            if (hasStateLocalVars && model.needsJSEngine()) {
                ss << "                {\n";
                ss << "                    this->ensureJSEngine();\n";
                ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                for (const auto &var : model.dataModel) {
                    if (var.stateName == state.name) {
                        bool hasExpr = !var.initialValue.empty();
                        // State-local variables are always initialized on entry (isFirstEntry=true for state-local)
                        // Note: State-local variables only exist when state is active, so re-initialization on
                        // each entry is semantically equivalent to first-entry-only initialization
                        if (RSM::BindingHelper::shouldAssignValueOnStateEntry(model.bindingMode, true, hasExpr)) {
                            std::string initExpr = var.initialValue;
                            ss << "                    auto initExpr_" << var.name
                               << " = jsEngine.evaluateExpression(sessionId_.value(), \""
                               << escapeStringLiteral(initExpr) << "\").get();\n";
                            ss << "                    if (::RSM::JSEngine::isSuccess(initExpr_" << var.name
                               << ")) {\n";
                            ss << "                        jsEngine.setVariable(sessionId_.value(), \"" << var.name
                               << "\", initExpr_" << var.name << ".getInternalValue());\n";
                            ss << "                    }\n";
                        } else {
                            // Early binding or no expr: create with undefined
                            std::string initExpr = "undefined";
                            ss << "                    auto initExpr_" << var.name
                               << " = jsEngine.evaluateExpression(sessionId_.value(), \""
                               << escapeStringLiteral(initExpr) << "\").get();\n";
                            ss << "                    if (::RSM::JSEngine::isSuccess(initExpr_" << var.name
                               << ")) {\n";
                            ss << "                        jsEngine.setVariable(sessionId_.value(), \"" << var.name
                               << "\", initExpr_" << var.name << ".getInternalValue());\n";
                            ss << "                    }\n";
                        }
                    }
                }
                ss << "                }\n";
            }

            // Then execute entry actions
            for (const auto &action : state.entryActions) {
                generateActionCode(ss, action, "engine", events, model);
            }

            // W3C SCXML 6.4: Start invoke elements on state entry
            if (hasInvoke) {
                ss << "                // W3C SCXML 6.4: Start invoke elements\n";
                ss << "                ensureSessionId();\n";
                ss << "                if (!sessionId_.has_value()) return;  // Should not happen\n";
                ss << "\n";

                size_t invokeIndex = 0;  // For generating unique invoke IDs
                for (const auto &invoke : state.invokes) {
                    // W3C SCXML: Generate unique invoke ID if not specified
                    std::string invokeId = invoke.invokeId;
                    if (invokeId.empty()) {
                        invokeId = state.name + "_invoke_" + std::to_string(invokeIndex);
                    }
                    invokeIndex++;

                    // Classify invoke: static (compile-time known) vs dynamic (runtime only)
                    // W3C SCXML types: empty, "scxml", or "http://www.w3.org/TR/scxml/"
                    bool isStaticInvoke = (invoke.type.empty() || invoke.type == "scxml" ||
                                           invoke.type == "http://www.w3.org/TR/scxml/") &&
                                          !invoke.src.empty() && invoke.srcExpr.empty();

                    if (isStaticInvoke) {
                        // Static SCXML child: Generate direct instantiation
                        // Extract child class name from src path (e.g., "file:child.scxml" -> "child")
                        std::string childSrc = invoke.src;

                        // Handle file: prefix (e.g., "file:test239sub1.scxml")
                        if (childSrc.find("file:") == 0) {
                            childSrc = childSrc.substr(5);  // Remove "file:" prefix
                        }

                        size_t lastSlash = childSrc.find_last_of("/\\");
                        if (lastSlash != std::string::npos) {
                            childSrc = childSrc.substr(lastSlash + 1);
                        }
                        size_t dotPos = childSrc.find_last_of('.');
                        std::string childName = (dotPos != std::string::npos) ? childSrc.substr(0, dotPos) : childSrc;

                        ss << "                // W3C SCXML 6.4: Static invoke (compile-time child SCXML)\n";
                        ss << "                // Child SCXML: " << invoke.src << " (generated as " << childName
                           << "_sm.h)\n";
                        ss << "                {\n";
                        ss << "                    std::string childSessionId = sessionId_.value() + \"_\" + \""
                           << invokeId << "\";\n";
                        ss << "                    LOG_INFO(\"Starting static invoke: id=" << invokeId
                           << ", src=" << invoke.src << "\");\n";
                        ss << "                    \n";
                        ss << "                    // Instantiate and store child state machine in Policy member\n";
                        // W3C SCXML 6.2: Find if this child needs parent pointer (#_parent usage)
                        bool childNeedsParent = false;
                        for (const auto &staticInvoke : staticInvokes) {
                            if (staticInvoke.invokeId == invokeId) {
                                childNeedsParent = staticInvoke.childNeedsParent;
                                break;
                            }
                        }

                        // If child uses #_parent, pass parent pointer via template
                        if (childNeedsParent) {
                            ss << "                    child_" << invokeId
                               << "_ = std::make_shared<::RSM::Generated::" << childName << "::" << childName
                               << "<SelfType>>(static_cast<SelfType*>(&engine));\n";
                        } else {
                            ss << "                    child_" << invokeId
                               << "_ = std::make_shared<::RSM::Generated::" << childName << "::" << childName
                               << ">();\n";
                        }
                        ss << "                    \n";
                        ss << "                    // W3C SCXML 6.4: Set completion callback for done.invoke event\n";
                        ss << "                    child_" << invokeId << "_->setCompletionCallback([this]() {\n";
                        ss << "                        pendingDoneInvoke_" << invokeId << "_ = true;\n";
                        ss << "                        LOG_DEBUG(\"Child " << invokeId
                           << " completed, pending done.invoke event\");\n";
                        ss << "                    });\n";
                        ss << "                    \n";

                        // W3C SCXML 6.4: Pass params to child state machine
                        if (!invoke.params.empty()) {
                            ss << "                    // W3C SCXML 6.4: Pass params to child state machine\n";
                            for (const auto &param : invoke.params) {
                                std::string paramName = std::get<0>(param);
                                std::string paramExpr = std::get<1>(param);
                                // std::string paramLocation = std::get<2>(param);  // Not used for static invokes

                                // For static params (no JSEngine), evaluate simple expressions
                                // Complex expressions would need JSEngine evaluation
                                if (!paramExpr.empty()) {
                                    ss << "                    child_" << invokeId << "_->getPolicy()." << paramName
                                       << " = " << paramExpr << ";\n";
                                }
                            }
                            ss << "                    \n";
                        }

                        ss << "                    // W3C SCXML 6.4.6: Handle invoke failure with error.execution\n";
                        ss << "                    try {\n";
                        ss << "                        child_" << invokeId << "_->initialize();\n";
                        ss << "                        \n";
                        ss << "                        // W3C SCXML 6.4: Check if child immediately reached final "
                              "state\n";
                        ss << "                        if (child_" << invokeId << "_->isInFinalState()) {\n";
                        ss << "                            LOG_INFO(\"Child " << invokeId
                           << " immediately completed, raising done.invoke\");\n";
                        ss << "                            engine.raise(Event::Done_invoke);\n";
                        ss << "                        }\n";
                        ss << "                    } catch (const std::exception& e) {\n";
                        ss << "                        // W3C SCXML 6.4.6: Raise error.execution on invoke failure\n";
                        ss << "                        LOG_ERROR(\"Failed to initialize child " << invokeId
                           << ": {}\", e.what());\n";
                        ss << "                        engine.raise(Event::Error_execution);\n";
                        ss << "                        child_" << invokeId << "_ = nullptr;\n";
                        ss << "                    }\n";
                        ss << "                    \n";
                        ss << "                    // W3C SCXML 6.4: Track child session for lifecycle management\n";
                        ss << "                    ChildSession session;\n";
                        ss << "                    session.sessionId = childSessionId;\n";
                        ss << "                    session.invokeId = \"" << invokeId << "\";\n";
                        ss << "                    session.parentSessionId = sessionId_.value();\n";
                        ss << "                    session.autoforward = " << (invoke.autoforward ? "true" : "false")
                           << ";\n";

                        // Add finalize script if present
                        if (!invoke.finalizeContent.empty()) {
                            ss << "                    session.finalizeScript = R\"(" << invoke.finalizeContent
                               << ")\";\n";
                        } else {
                            ss << "                    session.finalizeScript = \"\";\n";
                        }

                        ss << "                    \n";
                        ss << "                    activeInvokes_[\"" << invokeId << "\"] = session;\n";
                        ss << "                    LOG_DEBUG(\"Invoke session created: id={}, autoforward={}\", \""
                           << invokeId << "\", session.autoforward);\n";
                        ss << "                }\n";
                    }
                    // Note: Dynamic invoke case handled by early return at function start (line 433-435)
                    // ARCHITECTURE.md: All-or-Nothing strategy - no hybrid JIT+Interpreter
                }
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
    if (needsStateful) {
        ss << "    void executeExitActions(State state, Engine& engine) {\n";  // non-static for stateful
    } else {
        ss << "    static void executeExitActions(State state, Engine& engine) {\n";  // static for stateless
    }
    ss << "        (void)engine;\n";
    ss << "        switch (state) {\n";
    for (const auto &state : model.states) {
        bool hasExitActions = !state.exitActions.empty();
        bool hasInvoke = !state.invokes.empty();

        if (hasExitActions || hasInvoke) {
            ss << "            case State::" << capitalize(state.name) << ":\n";

            // W3C SCXML 6.4: Cancel invoke elements on state exit first
            if (hasInvoke) {
                ss << "                // W3C SCXML 6.4: Cancel invoke elements\n";

                size_t invokeIndex = 0;  // Match entry action indexing
                for (const auto &invoke : state.invokes) {
                    // W3C SCXML: Generate unique invoke ID (same as entry action)
                    std::string invokeId = invoke.invokeId;
                    if (invokeId.empty()) {
                        invokeId = state.name + "_invoke_" + std::to_string(invokeIndex);
                    }
                    invokeIndex++;

                    // Check if this is a static invoke
                    bool isStaticInvoke = (invoke.type.empty() || invoke.type == "scxml" ||
                                           invoke.type == "http://www.w3.org/TR/scxml/") &&
                                          !invoke.src.empty() && invoke.srcExpr.empty();

                    if (isStaticInvoke) {
                        ss << "                \n";
                        ss << "                // W3C SCXML 6.4: Cleanup static invoke child\n";
                        ss << "                if (child_" << invokeId << "_) {\n";
                        ss << "                    LOG_DEBUG(\"Stopping static invoke: id=" << invokeId << "\");\n";
                        ss << "                    // W3C SCXML 6.4: Send cancel.invoke platform event\n";
                        ss << "                    engine.raise(Event::Cancel_invoke);\n";
                        ss << "                    child_" << invokeId
                           << "_.reset();  // Destroy child state machine\n";
                        ss << "                }\n";
                    } else {
                        // Dynamic invoke cleanup
                        ss << "                \n";
                        ss << "                // W3C SCXML 6.4: Cleanup dynamic invoke (Interpreter engine)\n";
                        ss << "                {\n";
                        ss << "                    auto it = interpreterEngines_.find(\"" << invokeId << "\");\n";
                        ss << "                    if (it != interpreterEngines_.end()) {\n";
                        ss << "                        LOG_DEBUG(\"Stopping dynamic invoke: id=" << invokeId
                           << "\");\n";
                        ss << "                        // W3C SCXML 6.4: Send cancel.invoke platform event\n";
                        ss << "                        engine.raise(Event::Cancel_invoke);\n";
                        ss << "                        interpreterEngines_.erase(it);  // Destroy Interpreter "
                              "instance\n";
                        ss << "                    }\n";
                        ss << "                }\n";
                    }

                    // Remove from activeInvokes tracking
                    ss << "                activeInvokes_.erase(\"" << invokeId << "\");\n";
                }
            }

            // Then execute exit actions
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
    if (needsStateful) {
        ss << "    bool processTransition(State& currentState, Event event, Engine& engine) {\n";  // non-static for
                                                                                                   // stateful
    } else {
        ss << "    static bool processTransition(State& currentState, Event event, Engine& engine) {\n";  // static for
                                                                                                          // stateless
    }
    ss << generateProcessEvent(model, events, staticInvokes);

    // Generate private helper methods
    if (needsStateful) {
        ss << "private:\n";

        // Session ID initialization helper (for Invoke and/or JSEngine)
        if (hasInvokes || model.needsJSEngine()) {
            ss << "    // Helper: Ensure session ID is initialized\n";
            ss << "    void ensureSessionId() const {\n";
            ss << "        if (!sessionId_.has_value()) {\n";
            ss << "            sessionId_ = \"session_\" + std::to_string(reinterpret_cast<uintptr_t>(this));\n";
            ss << "        }\n";
            ss << "    }\n\n";
        }
    }

    // Generate JSEngine-specific helpers
    if (model.needsJSEngine()) {
        ss << "    // Helper: Ensure JSEngine is initialized (lazy initialization)\n";
        ss << "    void ensureJSEngine() const {\n";
        ss << "        if (jsEngineInitialized_) return;  // Already initialized\n";
        ss << "        ensureSessionId();\n";
        ss << "        if (!sessionId_.has_value()) return;  // Should not happen\n";
        ss << "        auto& jsEngine = ::RSM::JSEngine::instance();\n";
        ss << "        jsEngine.createSession(sessionId_.value());\n";
        ss << "\n";

        // W3C SCXML 5.3: Initialize datamodel variables according to binding mode
        // Use BindingHelper (Single Source of Truth) for binding semantics
        bool shouldAssignValue = RSM::BindingHelper::shouldAssignValueAtDocumentLoad(model.bindingMode);

        for (const auto &var : model.dataModel) {
            bool hasExpr = !var.initialValue.empty();

            // Determine initialization expression
            // Use BindingHelper to ensure W3C SCXML 5.3 compliance with Interpreter engine
            std::string initExpr;
            if (shouldAssignValue && hasExpr) {
                // Early binding with expr: use the expression
                initExpr = var.initialValue;
            } else {
                // Late binding or no expr: use undefined
                initExpr = "undefined";
            }

            // Generate initialization code
            ss << "            auto initExpr_" << var.name << " = jsEngine.evaluateExpression(sessionId_.value(), \""
               << escapeStringLiteral(initExpr) << "\").get();\n";
            ss << "            if (!::RSM::JSEngine::isSuccess(initExpr_" << var.name << ")) {\n";
            ss << "                LOG_ERROR(\"Failed to evaluate expression for variable: " << var.name << "\");\n";
            ss << "                // W3C SCXML 5.3: Mark initialization failure for later error.execution event\n";
            ss << "                datamodelInitFailed_ = true;\n";
            ss << "            } else {\n";
            ss << "                jsEngine.setVariable(sessionId_.value(), \"" << var.name << "\", initExpr_"
               << var.name << ".getInternalValue());\n";
            ss << "            }\n";
        }

        ss << "        jsEngineInitialized_ = true;\n";
        ss << "    }\n";

        // W3C SCXML 6.4: Helper to set param in JSEngine for static invoke
        ss << "\n";
        ss << "public:\n";
        ss << "    // Helper: Set param in JSEngine for static invoke (W3C SCXML 6.4)\n";
        ss << "    void setParamInJSEngine(const std::string& paramName, const std::string& paramExpr) {\n";
        ss << "        ensureJSEngine();\n";
        ss << "        auto& jsEngine = ::RSM::JSEngine::instance();\n";
        ss << "        auto valueResult = jsEngine.evaluateExpression(sessionId_.value(), paramExpr).get();\n";
        ss << "        if (::RSM::JSEngine::isSuccess(valueResult)) {\n";
        ss << "            jsEngine.setVariable(sessionId_.value(), paramName, valueResult.getInternalValue());\n";
        ss << "        } else {\n";
        ss << "            LOG_ERROR(\"Failed to evaluate param expression for {}: {}\", paramName, paramExpr);\n";
        ss << "        }\n";
        ss << "    }\n";
        ss << "private:\n";

        // W3C SCXML 5.10: Helper to convert Event enum to string (test318)
        if (model.needsJSEngine()) {
            ss << "\n";
            ss << "    // Helper: Convert Event enum to string for _event.name (W3C SCXML 5.10 - test318)\n";
            ss << "    static std::string getEventName(Event event) {\n";
            ss << "        switch (event) {\n";
            ss << "            case Event::NONE: return \"\";\n";
            for (const auto &eventName : events) {
                ss << "            case Event::" << capitalize(eventName) << ": return \"" << eventName << "\";\n";
            }
            ss << "            default: return \"\";\n";
            ss << "        }\n";
            ss << "    }\n";
        }

        // W3C SCXML 5.10: Helper to set _event variable in JSEngine (test176, test318)
        // ARCHITECTURE.md: Zero Duplication - Uses EventHelper for cross-engine consistency
        if (model.needsJSEngine()) {
            ss << "\n";
            ss << "    // Helper: Set _event variable in JSEngine (W3C SCXML 5.10 - test176, test318)\n";
            ss << "    void setCurrentEventInJSEngine(const std::string& eventName, const std::string& eventData = "
                  "\"\") {\n";
            ss << "        if (eventName.empty()) return;\n";
            ss << "        ensureJSEngine();\n";
            ss << "        // W3C SCXML 5.10: Set _event variable in JavaScript context\n";
            ss << "        ::RSM::JSEngine::instance().setCurrentEvent(sessionId_.value(), eventName, eventData, "
                  "\"internal\");\n";
            ss << "    }\n";
        }
    }

    ss << "};\n\n";

    // Generate user-facing class using StaticExecutionEngine
    ss << "// User-facing state machine class\n";

    // W3C SCXML 6.2: Template on ParentStateMachine if child uses #_parent (Zero Overhead)
    LOG_DEBUG("StaticCodeGenerator::generateClass - hasSendToParent: {}", model.hasSendToParent);
    // W3C SCXML 6.2: Reuse anyChildNeedsParent computed earlier in Policy template section

    if (model.hasSendToParent) {
        ss << "template<typename ParentStateMachine>\n";
    }

    // W3C SCXML 6.2: Class inheritance based on Policy template type
    if (model.hasSendToParent) {
        // Child sends to parent: Policy<ParentStateMachine>
        ss << "class " << model.name << " : public ::RSM::Static::StaticExecutionEngine<" << model.name
           << "Policy<ParentStateMachine>> {\n";
    } else if (anyChildNeedsParent) {
        // Parent has children needing parent: Policy<SelfType> (CRTP)
        ss << "class " << model.name << " : public ::RSM::Static::StaticExecutionEngine<" << model.name << "Policy<"
           << model.name << ">> {\n";
    } else {
        ss << "class " << model.name << " : public ::RSM::Static::StaticExecutionEngine<" << model.name
           << "Policy> {\n";
    }
    ss << "public:\n";

    // W3C SCXML 6.2: Constructor with parent pointer for #_parent support
    if (model.hasSendToParent) {
        ss << "    // W3C SCXML 6.2: Parent state machine pointer for #_parent support (Zero Overhead)\n";
        ss << "    explicit " << model.name << "(ParentStateMachine* parent) { this->policy_.parent_ = parent; }\n";
        ss << "\n";
    } else {
        ss << "    " << model.name << "() = default;\n";
        ss << "\n";
    }
    ss << "    ~" << model.name << "() {\n";
    ss << "        // W3C SCXML 6.4: Cleanup child state machines on destruction\n";

    // Generate cleanup code for each child state machine
    for (const auto &invokeInfo : staticInvokes) {
        ss << "        if (policy_.child_" << invokeInfo.invokeId << "_) {\n";
        ss << "            policy_.child_" << invokeInfo.invokeId << "_.reset();\n";
        ss << "        }\n";
    }

    ss << "    }\n";

    // Note: parent_ is now in Policy, not in class
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
        // W3C SCXML 5.3, 5.4: <assign> with expr attribute and location validation
        // ARCHITECTURE.md: Zero Duplication - Use shared AssignHelper
        // When JSEngine is needed, use evaluateExpression + setVariable pattern
        // When JSEngine is not needed, use direct C++ assignment
        if (model.needsJSEngine()) {
            // JSEngine-based assignment: evaluate expression and set variable
            ss << "                {\n";  // Extra scope to avoid "jump to case label" error
            ss << "                    // W3C SCXML 5.3, 5.4: Validate assignment location using shared AssignHelper\n";
            ss << "                    if (::RSM::AssignHelper::isValidLocation(\"" << action.param1 << "\")) {\n";
            ss << "                        // Location is valid, proceed with assignment\n";
            ss << "                        this->ensureJSEngine();\n";
            ss << "                        auto& jsEngine = ::RSM::JSEngine::instance();\n";
            ss << "                        {\n";
            ss << "                            // W3C SCXML 5.3: Execute assignment as JavaScript expression to "
                  "preserve object references\n";
            ss << "                            // Using direct assignment (location = expr) instead of "
                  "getInternalValue() for reference semantics\n";
            ss << "                            auto exprResult = jsEngine.evaluateExpression(sessionId_.value(), \""
               << action.param1 << " = (" << escapeStringLiteral(action.param2) << ")\").get();\n";
            ss << "                            if (!::RSM::JSEngine::isSuccess(exprResult)) {\n";
            ss << "                                LOG_ERROR(\"Failed to evaluate assignment expression: "
               << action.param1 << " = (" << escapeStringLiteral(action.param2) << ")\");\n";
            ss << "                                " << engineVar << ".raise(Event::Error_execution);\n";
            ss << "                            }\n";
            ss << "                        }\n";
            ss << "                    } else {\n";
            ss << "                        // W3C SCXML 5.3/5.4/B.2: Invalid or read-only location\n";
            ss << "                        LOG_ERROR(\"W3C SCXML 5.3: {}\", "
                  "::RSM::AssignHelper::getInvalidLocationErrorMessage(\""
               << action.param1 << "\"));\n";
            ss << "                        " << engineVar << ".raise(Event::Error_execution);\n";
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
            std::string idLocation = action.sendIdLocation;

            // W3C SCXML 6.2.4: Generate unique sendid and store in idlocation if specified (test183)
            if (!idLocation.empty()) {
                ss << "                // W3C SCXML 6.2.4: Generate sendid and store in idlocation (test183)\n";
                ss << "                // Using SendHelper for Zero Duplication (shared with interpreter)\n";
                ss << "                {\n";
                if (model.needsJSEngine()) {
                    ss << "                    this->ensureJSEngine();\n";
                    ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                    ss << "                    // Generate unique sendid using shared helper (Single Source of "
                          "Truth)\n";
                    ss << "                    std::string sendId = ::RSM::SendHelper::generateSendId();\n";
                    ss << "                    // Store sendid in idlocation variable using shared helper\n";
                    ss << "                    ::RSM::SendHelper::storeInIdLocation(jsEngine, sessionId_.value(), \""
                       << idLocation << "\", sendId);\n";
                } else {
                    // Static mode: direct variable assignment (JSEngine not available)
                    ss << "                    std::string sendId = ::RSM::SendHelper::generateSendId();\n";
                    ss << "                    " << idLocation << " = sendId;\n";
                }
                ss << "                }\n";
            }

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
                ss << "                        // W3C SCXML 5.10: Invalid target raises error.execution and stops "
                      "execution\n";
                ss << "                        " << engineVar << ".raise(Event::Error_execution);\n";
                ss << "                        return;\n";
                ss << "                    }\n";
                ss << "                    // Target is valid (including #_internal for internal events)\n";
                ss << "                }\n";
            } else if (!target.empty()) {
                // W3C SCXML 6.2: Handle #_parent target (send event to parent state machine)
                if (target == "#_parent") {
                    ss << "                // W3C SCXML 6.2: Send event to parent state machine (Single Source of "
                          "Truth: SendHelper)\n";
                    ss << "                ::RSM::SendHelper::sendToParent(parent_, ParentStateMachine::Event::"
                       << capitalize(event) << ");\n";
                    // Skip further processing for #_parent target
                    break;
                }

                // Static target validation (only when targetExpr is not present and not #_parent)
                ss << "                // W3C SCXML 6.2 (tests 159, 194): Validate send target using SendHelper\n";
                ss << "                if (::RSM::SendHelper::isInvalidTarget(\"" << target << "\")) {\n";
                ss << "                    // W3C SCXML 5.10: Invalid target raises error.execution and stops "
                      "subsequent executable content\n";
                ss << "                    " << engineVar << ".raise(Event::Error_execution);\n";
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
                        {
                            ss << "                        " << engineVar << ".raise(Event::" << eventEnum << ");\n";
                        }
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
                // W3C SCXML 6.2: Handle delay/delayexpr for scheduled send
                std::string delay = action.param5;
                std::string delayExpr = action.param6;

                if (!delay.empty() || !delayExpr.empty()) {
                    // W3C SCXML 6.2: Delayed send - schedule event for future delivery
                    // W3C SCXML 5.10: Evaluate params at send time, not dispatch time (test186)
                    ss << "                // W3C SCXML 6.2: Delayed send with event scheduling\n";
                    ss << "                {\n";

                    // W3C SCXML 5.10: Evaluate params BEFORE scheduling (test186)
                    if (!action.sendParams.empty()) {
                        ss << "                    // W3C SCXML 5.10: Evaluate params at send time (test186)\n";
                        ss << "                    std::map<std::string, std::vector<std::string>> params;\n";
                        for (const auto &[paramName, paramExpr] : action.sendParams) {
                            if (model.needsJSEngine()) {
                                ss << "                    {\n";
                                ss << "                        this->ensureJSEngine();\n";
                                ss << "                        auto& jsEngine = ::RSM::JSEngine::instance();\n";
                                ss << "                        auto paramResult = "
                                      "jsEngine.getVariable(sessionId_.value(), \""
                                   << paramExpr << "\").get();\n";
                                ss << "                        if (::RSM::JSEngine::isSuccess(paramResult)) {\n";
                                ss << "                            params[\"" << paramName
                                   << "\"].push_back(::RSM::JSEngine::resultToString(paramResult));\n";
                                ss << "                        } else {\n";
                                ss << "                            LOG_ERROR(\"Failed to evaluate param expr: "
                                   << escapeStringLiteral(paramExpr) << "\");\n";
                                ss << "                            params[\"" << paramName << "\"].push_back(\"\");\n";
                                ss << "                        }\n";
                                ss << "                    }\n";
                            } else {
                                ss << "                    params[\"" << paramName << "\"].push_back(std::to_string("
                                   << paramExpr << "));\n";
                            }
                        }
                    }

                    if (!delayExpr.empty()) {
                        // Dynamic delay from variable
                        ss << "                    std::string delayStr;\n";
                        if (model.needsJSEngine()) {
                            ss << "                    this->ensureJSEngine();\n";
                            ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                            ss << "                    auto delayResult = jsEngine.getVariable(sessionId_.value(), \""
                               << delayExpr << "\").get();\n";
                            ss << "                    if (::RSM::JSEngine::isSuccess(delayResult)) {\n";
                            ss << "                        delayStr = ::RSM::JSEngine::resultToString(delayResult);\n";
                            ss << "                    }\n";
                        } else {
                            ss << "                    delayStr = " << delayExpr << ";\n";
                        }
                        ss << "                    auto delayMs = "
                              "::RSM::SendSchedulingHelper::parseDelayString(delayStr);\n";
                    } else {
                        // Static delay
                        ss << "                    auto delayMs = ::RSM::SendSchedulingHelper::parseDelayString(\""
                           << delay << "\");\n";
                    }

                    // Build event data and schedule with data
                    if (!action.sendParams.empty()) {
                        ss << "                    std::string eventData = "
                              "::RSM::EventDataHelper::buildJsonFromParams(params);\n";
                        ss << "                    eventScheduler_.scheduleEvent(Event::" << capitalize(event)
                           << ", delayMs, \"" << escapeStringLiteral(action.sendId) << "\", eventData);\n";
                    } else {
                        // W3C SCXML 6.2.5: Pass sendId for event tracking/cancellation (test208)
                        ss << "                    eventScheduler_.scheduleEvent(Event::" << capitalize(event)
                           << ", delayMs, \"" << escapeStringLiteral(action.sendId) << "\");\n";
                    }
                    ss << "                }\n";
                } else {
                    // Immediate send (no delay) - use raise as before
                    if (events.find(event) != events.end()) {
                        // W3C SCXML 5.10: Construct event data from params using EventDataHelper (Single Source of
                        // Truth)
                        if (!action.sendParams.empty()) {
                            ss << "                // W3C SCXML 5.10: Build event data using EventDataHelper (Single "
                                  "Source of Truth)\n";
                            ss << "                {\n";
                            ss << "                    std::map<std::string, std::vector<std::string>> params;\n";

                            for (const auto &[paramName, paramExpr] : action.sendParams) {
                                // Evaluate param expression and add to params map
                                if (model.needsJSEngine()) {
                                    ss << "                    {\n";
                                    ss << "                        this->ensureJSEngine();\n";
                                    ss << "                        auto& jsEngine = ::RSM::JSEngine::instance();\n";
                                    ss << "                        auto paramResult = "
                                          "jsEngine.getVariable(sessionId_.value(), \""
                                       << paramExpr << "\").get();\n";
                                    ss << "                        if (::RSM::JSEngine::isSuccess(paramResult)) {\n";
                                    ss << "                            params[\"" << paramName
                                       << "\"].push_back(::RSM::JSEngine::resultToString(paramResult));\n";
                                    ss << "                        } else {\n";
                                    ss << "                            LOG_ERROR(\"Failed to evaluate param expr: "
                                       << escapeStringLiteral(paramExpr) << "\");\n";
                                    ss << "                            params[\"" << paramName
                                       << "\"].push_back(\"\");\n";
                                    ss << "                        }\n";
                                    ss << "                    }\n";
                                } else {  // Static datamodel - direct variable reference
                                    ss << "                    params[\"" << paramName
                                       << "\"].push_back(std::to_string(" << paramExpr << "));\n";
                                }
                            }

                            ss << "                    std::string eventData = "
                                  "::RSM::EventDataHelper::buildJsonFromParams(params);\n";
                            // W3C SCXML 6.2.4: Check type attribute for queue routing (test193)
                            if (!action.sendType.empty() &&
                                action.sendType == "http://www.w3.org/TR/scxml/#SCXMLEventProcessor") {
                                ss << "                    " << engineVar
                                   << ".raiseExternal(Event::" << capitalize(event) << ", eventData);\n";
                            } else {
                                // W3C SCXML C.1 (test189): Use SendHelper to determine queue routing (Single Source of
                                // Truth)
                                ss << "                    if (::RSM::SendHelper::isInternalTarget(\"" << target
                                   << "\")) {\n";
                                ss << "                        " << engineVar << ".raise(Event::" << capitalize(event)
                                   << ", eventData);\n";
                                ss << "                    } else {\n";
                                ss << "                        " << engineVar
                                   << ".raiseExternal(Event::" << capitalize(event) << ", eventData);\n";
                                ss << "                    }\n";
                            }
                            ss << "                }\n";
                        } else if (!action.sendContent.empty()) {
                            // W3C SCXML 5.10: Set event data from <content> literal (test179)
                            ss << "                // W3C SCXML 5.10: Set _event.data from <content> literal\n";
                            // W3C SCXML 6.2.4: Check type attribute for queue routing (test193)
                            if (!action.sendType.empty() &&
                                action.sendType == "http://www.w3.org/TR/scxml/#SCXMLEventProcessor") {
                                ss << "                " << engineVar << ".raiseExternal(Event::" << capitalize(event)
                                   << ", \"" << escapeStringLiteral(action.sendContent) << "\");\n";
                            } else {
                                // W3C SCXML C.1 (test189): Use SendHelper to determine queue routing (Single Source of
                                // Truth)
                                ss << "                if (::RSM::SendHelper::isInternalTarget(\"" << target
                                   << "\")) {\n";
                                ss << "                    " << engineVar << ".raise(Event::" << capitalize(event)
                                   << ", \"" << escapeStringLiteral(action.sendContent) << "\");\n";
                                ss << "                } else {\n";
                                ss << "                    " << engineVar
                                   << ".raiseExternal(Event::" << capitalize(event) << ", \""
                                   << escapeStringLiteral(action.sendContent) << "\");\n";
                                ss << "                }\n";
                            }
                        } else if (!action.sendContentExpr.empty()) {
                            // W3C SCXML 5.10: Set event data from <content expr="..."> dynamic evaluation
                            ss << "                // W3C SCXML 5.10: Evaluate <content expr> for event data\n";
                            ss << "                {\n";
                            ss << "                    this->ensureJSEngine();\n";
                            ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                            ss << "                    auto contentResult = "
                                  "jsEngine.evaluateExpression(sessionId_.value(), \""
                               << escapeStringLiteral(action.sendContentExpr) << "\").get();\n";
                            ss << "                    std::string eventData;\n";
                            ss << "                    if (::RSM::JSEngine::isSuccess(contentResult)) {\n";
                            ss << "                        eventData = "
                                  "::RSM::JSEngine::resultToString(contentResult);\n";
                            ss << "                    } else {\n";
                            ss << "                        LOG_ERROR(\"Failed to evaluate content expr: "
                               << escapeStringLiteral(action.sendContentExpr) << "\");\n";
                            ss << "                        " << engineVar << ".raise(Event::Error_execution);\n";
                            ss << "                        eventData = \"\";\n";
                            ss << "                    }\n";
                            // W3C SCXML 6.2.4: Check type attribute for queue routing (test193)
                            if (!action.sendType.empty() &&
                                action.sendType == "http://www.w3.org/TR/scxml/#SCXMLEventProcessor") {
                                ss << "                    " << engineVar
                                   << ".raiseExternal(Event::" << capitalize(event) << ", eventData);\n";
                            } else {
                                // W3C SCXML C.1 (test189): Use SendHelper to determine queue routing (Single Source of
                                // Truth)
                                ss << "                    if (::RSM::SendHelper::isInternalTarget(\"" << target
                                   << "\")) {\n";
                                ss << "                        " << engineVar << ".raise(Event::" << capitalize(event)
                                   << ", eventData);\n";
                                ss << "                    } else {\n";
                                ss << "                        " << engineVar
                                   << ".raiseExternal(Event::" << capitalize(event) << ", eventData);\n";
                                ss << "                    }\n";
                            }
                            ss << "                }\n";
                        } else {
                            // W3C SCXML 6.2.4: Check type attribute for queue routing (test193)
                            if (!action.sendType.empty() &&
                                action.sendType == "http://www.w3.org/TR/scxml/#SCXMLEventProcessor") {
                                ss << "                " << engineVar << ".raiseExternal(Event::" << capitalize(event)
                                   << ");\n";
                            } else {
                                // W3C SCXML C.1 (test189): Use SendHelper to determine queue routing (Single Source of
                                // Truth)
                                ss << "                if (::RSM::SendHelper::isInternalTarget(\"" << target
                                   << "\")) {\n";
                                ss << "                    " << engineVar << ".raise(Event::" << capitalize(event)
                                   << ");\n";
                                ss << "                } else {\n";
                                ss << "                    " << engineVar
                                   << ".raiseExternal(Event::" << capitalize(event) << ");\n";
                                ss << "                }\n";
                            }
                        }
                    } else {
                        // Event not in enum, likely unreachable - just comment
                        ss << "                // Event '" << event << "' not defined in Event enum (unreachable)\n";
                    }
                }
            }
        }
        break;
    case Action::CANCEL:
        // W3C SCXML 6.3: Cancel scheduled send event by sendid
        // param1 = sendid (literal string), param2 = sendidexpr (should be empty for static)
        if (!action.param1.empty()) {
            ss << "                // W3C SCXML 6.3: Cancel delayed send event\n";
            ss << "                eventScheduler_.cancelEvent(\"" << escapeStringLiteral(action.param1) << "\");\n";
        }
        break;
    case Action::IF:
        // W3C SCXML 5.9: Conditional expressions in <if> elements
        // ARCHITECTURE.md: Static Hybrid approach for ECMAScript conditionals
        // - Simple conditionals (x > 0): Direct C++ evaluation
        // - ECMAScript features (typeof, _event, In()): JSEngine evaluation
        {
            bool needsJSEval = model.needsJSEngine();

            for (size_t i = 0; i < action.branches.size(); ++i) {
                const auto &branch = action.branches[i];

                if (branch.isElseBranch) {
                    // Else branch - no condition evaluation needed
                    if (needsJSEval) {
                        ss << "                    } else {\n";
                    } else {
                        ss << "                } else {\n";
                    }
                } else {
                    // If or elseif branch - evaluate condition
                    if (needsJSEval) {
                        // W3C SCXML 5.9: ECMAScript datamodel - evaluate via JSEngine
                        // ARCHITECTURE.md: Zero Duplication - Use shared GuardHelper for conditional evaluation
                        if (i == 0) {
                            ss << "                // W3C SCXML 5.9: Conditional expression via GuardHelper "
                                  "(ECMAScript datamodel)\n";
                            ss << "                {\n";
                            ss << "                    this->ensureJSEngine();\n";
                            ss << "                    auto& jsEngine = ::RSM::JSEngine::instance();\n";
                            ss << "                    if (::RSM::GuardHelper::evaluateGuard(jsEngine, "
                                  "sessionId_.value(), \""
                               << escapeStringLiteral(branch.condition) << "\")) {\n";
                        } else {
                            ss << "                    } else {\n";
                            ss << "                        if (::RSM::GuardHelper::evaluateGuard(jsEngine, "
                                  "sessionId_.value(), \""
                               << escapeStringLiteral(branch.condition) << "\")) {\n";
                        }
                    } else {
                        // Static mode: Direct C++ evaluation
                        if (i == 0) {
                            ss << "                if (" << branch.condition << ") {\n";
                        } else {
                            ss << "                } else if (" << branch.condition << ") {\n";
                        }
                    }
                }

                // Generate actions in this branch
                for (const auto &branchAction : branch.actions) {
                    if (needsJSEval) {
                        // JSEngine mode: actions go inside "if (condValue)" block
                        // Need to add 4 spaces to each line from generateActionCode output
                        std::stringstream tempStream;
                        generateActionCode(tempStream, branchAction, engineVar, events, model);

                        // Add 4 spaces to each line
                        std::string content = tempStream.str();
                        if (!content.empty()) {
                            size_t pos = 0;
                            while (pos < content.length()) {
                                size_t nextPos = content.find('\n', pos);
                                if (nextPos == std::string::npos) {
                                    // Last line without newline
                                    ss << "    " << content.substr(pos) << "\n";
                                    break;
                                } else {
                                    // Line with newline - include the newline
                                    ss << "    " << content.substr(pos, nextPos - pos + 1);
                                    pos = nextPos + 1;
                                }
                            }
                        }
                    } else {
                        // Static mode: normal indentation
                        generateActionCode(ss, branchAction, engineVar, events, model);
                    }
                }
            }

            // Close all open braces
            if (!action.branches.empty()) {
                if (needsJSEval) {
                    // W3C SCXML 5.9: Close nested JSEngine conditional structure
                    // Structure: { JSEngine { if (cond1) { ... } else { if (cond2) { ... } else { ... } } } }

                    // Count how many if (condValue) blocks need closing
                    // - Each non-else branch opens "if (condValue) {"
                    // - Each else branch after the first opens "} else {"
                    // - The final else branch doesn't open "if (condValue)"

                    size_t numIfBlocks = 0;
                    for (const auto &branch : action.branches) {
                        if (!branch.isElseBranch) {
                            numIfBlocks++;
                        }
                    }

                    // Close the last if (condValue) block (if not else)
                    if (!action.branches.back().isElseBranch) {
                        ss << "                    }\n";
                    }

                    // Close each intermediate else block (one for each branch except the first)
                    for (size_t i = 1; i < action.branches.size(); ++i) {
                        ss << "                    }\n";
                    }

                    // Close the JSEngine initialization scope
                    ss << "                }\n";
                } else {
                    ss << "                }\n";
                }
            }
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
                        ss << "                                    // W3C SCXML 5.3, 5.4: Validate assignment location "
                              "using shared AssignHelper\n";
                        ss << "                                    if (!::RSM::AssignHelper::isValidLocation(\""
                           << iterAction.param1 << "\")) {\n";
                        ss << "                                        LOG_ERROR(\"W3C SCXML 5.3: {}\", "
                              "::RSM::AssignHelper::getInvalidLocationErrorMessage(\""
                           << iterAction.param1 << "\"));\n";
                        ss << "                                        return false;  // W3C SCXML 4.6: Stop foreach "
                              "execution on error\n";
                        ss << "                                    }\n";
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

    // Handle wildcard event (W3C SCXML 3.12.1: event="*" or event=".*")
    if (str == "*" || str == ".*") {
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
            } else if (action.type == Action::SEND && !action.param1.empty()) {
                // W3C SCXML 6.2: Extract events from send actions (test208)
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

        // W3C SCXML 6.4: Add invoke-related platform events if state has invoke elements
        if (!state.invokes.empty()) {
            events.insert("done.invoke");      // W3C SCXML 6.4: Raised when child completes
            events.insert("error.execution");  // W3C SCXML 6.4.6: Raised on invoke failure
            events.insert("cancel.invoke");    // W3C SCXML 6.4: Raised when invoke is cancelled
        }
    }

    // W3C SCXML 5.10: Add error.execution if model has send (for invalid target validation)
    if (model.hasSend) {
        events.insert("error.execution");  // W3C SCXML 5.10: Raised on send validation errors
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
        case '\'':
            // W3C SCXML: Convert JavaScript single quotes to double quotes
            // This allows JavaScript expressions with string literals to work correctly
            // when passed through C++ string literals to the JSEngine
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

std::string StaticCodeGenerator::generateGetParentMethod(const SCXMLModel &model) {
    std::stringstream ss;

    // W3C SCXML 3.3: Parent state mapping for hierarchical entry
    ss << "    // W3C SCXML 3.3: Parent state mapping (for HierarchicalStateHelper)\n";
    ss << "    static std::optional<State> getParent(State state) {\n";
    ss << "        switch (state) {\n";

    // Build parent mapping from model.states
    std::map<std::string, std::string> parentMap;
    for (const auto &state : model.states) {
        if (!state.parentState.empty()) {
            parentMap[state.name] = state.parentState;
        }
    }

    // Generate cases for states with parents
    for (const auto &[stateName, parentName] : parentMap) {
        ss << "            case State::" << capitalize(stateName) << ":\n";
        ss << "                return State::" << capitalize(parentName) << ";\n";
    }

    ss << "            default:\n";
    ss << "                return std::nullopt;  // Root state\n";
    ss << "        }\n";
    ss << "    }\n\n";

    return ss.str();
}

void StaticCodeGenerator::generateDoneDataCode(std::stringstream &ss, const std::string &targetState,
                                               const SCXMLModel &model, const std::string &indent) {
    // Find the target state in model
    const State *finalState = nullptr;
    for (const auto &state : model.states) {
        if (state.name == targetState && state.isFinal) {
            finalState = &state;
            break;
        }
    }

    // Only generate code if target is a final state with donedata
    if (!finalState || (finalState->doneData.content.empty() && finalState->doneData.params.empty())) {
        return;
    }

    ss << indent << "// W3C SCXML 5.5/5.7: Evaluate donedata for final state\n";
    ss << indent << "{\n";

    const std::string innerIndent = indent + "    ";

    // Initialize JSEngine once for all donedata operations
    ss << innerIndent << "this->ensureJSEngine();\n";
    ss << innerIndent << "auto& jsEngine = ::RSM::JSEngine::instance();\n";
    ss << innerIndent << "std::string eventData;\n";

    // W3C SCXML 5.5: Handle <content> expression
    if (!finalState->doneData.content.empty()) {
        ss << innerIndent << "::RSM::DoneDataHelper::evaluateContent(\n";
        ss << innerIndent << "    jsEngine, sessionId_, \"" << escapeStringLiteral(finalState->doneData.content)
           << "\", eventData,\n";
        ss << innerIndent << "    [&engine](const std::string& msg) {\n";
        ss << innerIndent << "        engine.raise(Event::Error_execution);\n";
        ss << innerIndent << "    });\n";
    }

    // W3C SCXML 5.7: Handle <param> elements
    if (!finalState->doneData.params.empty()) {
        ss << innerIndent << "std::vector<std::pair<std::string, std::string>> params = {\n";

        for (const auto &param : finalState->doneData.params) {
            ss << innerIndent << "    {\"" << escapeStringLiteral(param.first) << "\", \""
               << escapeStringLiteral(param.second) << "\"},\n";
        }

        ss << innerIndent << "};\n";
        ss << innerIndent << "if (!::RSM::DoneDataHelper::evaluateParams(\n";
        ss << innerIndent << "        jsEngine, sessionId_, params, eventData,\n";
        ss << innerIndent << "        [&engine](const std::string& msg) {\n";
        ss << innerIndent << "            engine.raise(Event::Error_execution);\n";
        ss << innerIndent << "        })) {\n";
        ss << innerIndent << "    // W3C SCXML 5.7: Structural error, skip transition\n";
        ss << innerIndent << "    break;\n";
        ss << innerIndent << "}\n";
    }

    ss << indent << "}\n";
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

bool StaticCodeGenerator::generateInterpreterWrapper(std::stringstream &ss, const SCXMLModel &model,
                                                     std::shared_ptr<RSM::SCXMLModel> rsmModel,
                                                     const std::string &scxmlPath, const std::string &outputDir) {
    (void)rsmModel;  // Model is injected later via setModel() API

    // W3C SCXML 6.4: Dynamic invoke detected - generate Interpreter wrapper
    // ARCHITECTURE.md: Zero Duplication - reuse Interpreter engine instead of reimplementing

    LOG_INFO("StaticCodeGenerator: Generating Interpreter wrapper for '{}' (dynamic invoke fallback)", model.name);

    // Clear existing content and start fresh
    ss.str("");
    ss.clear();

    // Generate header with Interpreter includes
    ss << "#pragma once\n";
    ss << "#include <memory>\n";
    ss << "#include \"runtime/StateMachine.h\"\n";
    ss << "#include \"model/SCXMLModel.h\"\n";
    ss << "\n";
    ss << "// W3C SCXML 6.4: Dynamic invoke detected - using Interpreter engine\n";
    ss << "// ARCHITECTURE.md: No hybrid approach - entire SCXML runs on Interpreter\n";
    ss << "\n";

    // Generate namespace
    ss << "namespace RSM::Generated::" << model.name << " {\n\n";

    // Generate Interpreter wrapper class
    ss << "// Interpreter wrapper class - provides StaticExecutionEngine-compatible interface\n";
    ss << "class " << model.name << " {\n";
    ss << "private:\n";
    ss << "    std::shared_ptr<::RSM::StateMachine> interpreter_;\n";
    ss << "\n";
    ss << "public:\n";
    ss << "    " << model.name << "() = default;\n";
    ss << "\n";
    ss << "    void initialize() {\n";
    ss << "        interpreter_ = std::make_shared<::RSM::StateMachine>();\n";
    ss << "        if (!interpreter_->loadSCXML(\"" << scxmlPath << "\")) {\n";
    ss << "            throw std::runtime_error(\"" << model.name << ": Failed to load SCXML from " << scxmlPath
       << "\");\n";
    ss << "        }\n";
    ss << "        if (!interpreter_->start()) {\n";
    ss << "            throw std::runtime_error(\"" << model.name << ": Failed to start Interpreter\");\n";
    ss << "        }\n";
    ss << "    }\n";
    ss << "\n";
    ss << "    bool isInFinalState() const {\n";
    ss << "        return interpreter_ ? interpreter_->isInFinalState() : false;\n";
    ss << "    }\n";
    ss << "\n";
    ss << "    void setCompletionCallback(std::function<void()> callback) {\n";
    ss << "        if (interpreter_) {\n";
    ss << "            interpreter_->setCompletionCallback(callback);\n";
    ss << "        }\n";
    ss << "    }\n";
    ss << "\n";
    ss << "    std::string getCurrentState() const {\n";
    ss << "        return interpreter_ ? interpreter_->getCurrentState() : \"\";\n";
    ss << "    }\n";
    ss << "\n";
    ss << "    void processEvent(const std::string& eventName, const std::string& eventData = \"\") {\n";
    ss << "        if (interpreter_) {\n";
    ss << "            interpreter_->processEvent(eventName, eventData);\n";
    ss << "        }\n";
    ss << "    }\n";
    ss << "};\n\n";

    ss << "} // namespace RSM::Generated::" << model.name << "\n";

    // Write to file
    std::string filename = model.name + "_sm.h";
    std::string outputPath = fs::path(outputDir) / filename;

    LOG_INFO("StaticCodeGenerator: Writing generated code to: {}", outputPath);
    if (!writeToFile(outputPath, ss.str())) {
        LOG_ERROR("StaticCodeGenerator: Failed to write Interpreter wrapper to file");
        return false;
    }

    LOG_INFO("StaticCodeGenerator: Successfully generated Interpreter wrapper for '{}'", model.name);
    return true;
}

}  // namespace RSM::Codegen
