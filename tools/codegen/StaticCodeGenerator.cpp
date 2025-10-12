// Static code generator with SCXML parser integration
#include "StaticCodeGenerator.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "rsm/include/factory/NodeFactory.h"
#include "rsm/include/model/SCXMLModel.h"
#include "rsm/include/parsing/SCXMLParser.h"

namespace fs = std::filesystem;

namespace RSM::Codegen {

bool StaticCodeGenerator::generate(const std::string &scxmlPath, const std::string &outputDir) {
    // Step 1: Validate input
    if (scxmlPath.empty()) {
        return false;
    }

    if (!fs::exists(scxmlPath)) {
        return false;
    }

    // Step 2: Parse SCXML file using actual parser
    auto nodeFactory = std::make_shared<RSM::NodeFactory>();
    RSM::SCXMLParser parser(nodeFactory);

    auto rsmModel = parser.parseFile(scxmlPath);
    if (!rsmModel) {
        return false;
    }

    // Step 3: Validate parsed model
    if (rsmModel->getName().empty()) {
        return false;
    }

    // Step 4: Convert RSM::SCXMLModel to simplified format for code generation
    SCXMLModel model;
    model.name = rsmModel->getName();
    model.initial = rsmModel->getInitialState();

    if (model.initial.empty()) {
        return false;
    }

    // Extract all states
    auto allStates = rsmModel->getAllStates();
    if (allStates.empty()) {
        return false;
    }

    for (const auto &state : allStates) {
        model.states.push_back(state->getId());

        // Extract transitions from each state
        auto transitions = state->getTransitions();
        for (const auto &transition : transitions) {
            auto event = transition->getEvent();
            auto targets = transition->getTargets();
            if (!event.empty() && !targets.empty()) {
                // Use first target (SCXML supports multiple targets)
                model.transitions.push_back(std::make_pair(event, targets[0]));
            }
        }
    }

    // Step 5: Extract unique states and events
    auto states = extractStates(model);
    auto events = extractEvents(model);

    // Validate we have states (events can be empty for stateless machines)
    if (states.empty()) {
        return false;
    }

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

    // Generate class
    ss << generateClass(model.name, model.initial);

    ss << "\n} // namespace RSM::Generated\n";

    // Step 7: Validate output directory and write to file
    if (outputDir.empty() || !fs::exists(outputDir) || !fs::is_directory(outputDir)) {
        return false;
    }

    std::string outputPath = outputDir + "/" + model.name + "_sm.h";
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

std::string StaticCodeGenerator::generateProcessEvent(const std::string &className) {
    (void)className;  // TODO: Use className for transition generation
    std::stringstream ss;

    ss << "    void processEvent(Event event) {\n";
    ss << "        // State transition logic will be generated here\n";
    ss << "        (void)event;  // Suppress unused parameter warning\n";
    ss << "    }\n";

    return ss.str();
}

std::string StaticCodeGenerator::generateClass(const std::string &className, const std::string &initialState) {
    std::stringstream ss;

    ss << "template<typename LogicType = void>\n";
    ss << "class " << className << " {\n";
    ss << "private:\n";
    ss << "    State currentState_ = State::" << capitalize(initialState) << ";\n";
    ss << "    std::unique_ptr<LogicType> logic_;\n\n";

    ss << "public:\n";
    ss << "    " << className << "() = default;\n\n";

    // processEvent method
    ss << generateProcessEvent(className);
    ss << "\n";

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

std::string StaticCodeGenerator::sanitizeName(const std::string &name) {
    std::string result = name;
    // Replace special characters with underscores
    std::replace(result.begin(), result.end(), '-', '_');
    std::replace(result.begin(), result.end(), '.', '_');
    return result;
}

std::set<std::string> StaticCodeGenerator::extractStates(const SCXMLModel &model) {
    return std::set<std::string>(model.states.begin(), model.states.end());
}

std::set<std::string> StaticCodeGenerator::extractEvents(const SCXMLModel &model) {
    std::set<std::string> events;
    for (const auto &[event, target] : model.transitions) {
        (void)target;  // Unused in extraction
        if (!event.empty()) {
            events.insert(event);
        }
    }
    return events;
}

bool StaticCodeGenerator::writeToFile(const std::string &path, const std::string &content) {
    // Create directory
    fs::path filePath(path);
    fs::create_directories(filePath.parent_path());

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << content;
    file.close();
    return true;
}

}  // namespace RSM::Codegen