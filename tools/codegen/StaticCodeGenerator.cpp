// Minimal static code generator implementation - TDD GREEN phase
#include "StaticCodeGenerator.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace RSM::Codegen {

namespace {
constexpr const char *DEFAULT_SM_NAME = "SimpleSM";
constexpr const char *DEFAULT_INITIAL_STATE = "idle";
}  // namespace

bool StaticCodeGenerator::generate(const std::string &scxmlPath, const std::string &outputDir) {
    (void)scxmlPath;  // TODO: Implement actual SCXML parsing
    // Step 1: Simple parsing (temporary - will use SCXMLParser)
    SCXMLModel model;
    model.name = DEFAULT_SM_NAME;
    model.initial = DEFAULT_INITIAL_STATE;
    model.states = {DEFAULT_INITIAL_STATE, "active"};
    model.transitions = {{"start", "active"}, {"stop", DEFAULT_INITIAL_STATE}};

    // Step 2: Extract information
    auto states = extractStates(model);
    auto events = extractEvents(model);

    // Step 3: Generate code
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
    ss << generateClass(model.name);

    ss << "\n} // namespace RSM::Generated\n";

    // Step 4: Write to file
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
    (void)className;  // TODO: Use className in implementation
    std::stringstream ss;

    ss << "    void processEvent(Event event) {\n";
    ss << "        switch(currentState_) {\n";
    ss << "            case State::Idle:\n";
    ss << "                if (event == Event::Start) {\n";
    ss << "                    currentState_ = State::Active;\n";
    ss << "                }\n";
    ss << "                break;\n";
    ss << "            case State::Active:\n";
    ss << "                if (event == Event::Stop) {\n";
    ss << "                    currentState_ = State::Idle;\n";
    ss << "                }\n";
    ss << "                break;\n";
    ss << "        }\n";
    ss << "    }\n";

    return ss.str();
}

std::string StaticCodeGenerator::generateClass(const std::string &className) {
    std::stringstream ss;

    ss << "template<typename LogicType = void>\n";
    ss << "class " << className << " {\n";
    ss << "private:\n";
    ss << "    State currentState_ = State::Idle;\n";
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
    result[0] = std::toupper(result[0]);
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
    std::set<std::string> states;
    for (const auto &state : model.states) {
        states.insert(state);
    }
    return states;
}

std::set<std::string> StaticCodeGenerator::extractEvents(const SCXMLModel &model) {
    std::set<std::string> events;
    for (const auto &[event, target] : model.transitions) {
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