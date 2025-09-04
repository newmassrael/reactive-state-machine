#include "core/ScriptNode.h"
#include "common/Logger.h"
#include "runtime/RuntimeContext.h"
#include <fstream>
#include <sstream>

namespace SCXML {
namespace Core {

ScriptNode::ScriptNode(const std::string &content, const std::string &type)
    : content_(content), type_(type), isInitializationScript_(true), executionPriority_(0) {
    SCXML::Common::Logger::debug("ScriptNode::Constructor - Creating script node with type: " + type);
}

const std::string &ScriptNode::getContent() const {
    return content_;
}

void ScriptNode::setContent(const std::string &content) {
    SCXML::Common::Logger::debug("ScriptNode::setContent - Setting script content (" + std::to_string(content.length()) + " chars)");
    content_ = content;
}

const std::string &ScriptNode::getSrc() const {
    return src_;
}

void ScriptNode::setSrc(const std::string &src) {
    SCXML::Common::Logger::debug("ScriptNode::setSrc - Setting script source: " + src);
    src_ = src;
}

SCXML::Common::Result<void> ScriptNode::execute(SCXML::Runtime::RuntimeContext &context) {
    SCXML::Common::Logger::debug("ScriptNode::execute - Executing script of type: " + type_);

    try {
        // Load content from file if src is specified and content is empty
        std::string scriptContent = content_;
        if (scriptContent.empty() && !src_.empty()) {
            auto contentResult = loadContentFromSrc();
            if (!contentResult.isSuccess()) {
                return SCXML::Common::Result<void>(
                    SCXML::Common::ErrorInfo{"ScriptNode::execute", "Failed to load script from source: " + src_,
                                             contentResult.getErrors()[0].details});
            }
            scriptContent = contentResult.getValue();
        }

        if (scriptContent.empty()) {
            SCXML::Common::Logger::warning("ScriptNode::execute - Empty script content");
            return SCXML::Common::Result<void>();
        }

        // Execute based on script type
        if (type_ == "ecmascript" || type_ == "javascript") {
            return executeECMAScript(context);
        } else {
            return SCXML::Common::Result<void>(
                SCXML::Common::ErrorInfo{"ScriptNode::execute", "Unsupported script type: " + type_,
                                         "Only 'ecmascript' and 'javascript' are supported"});
        }

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>(
            SCXML::Common::ErrorInfo{"ScriptNode::execute", "Script execution failed", e.what()});
    }
}

std::vector<std::string> ScriptNode::validate() const {
    std::vector<std::string> errors;

    // Check that we have either content or src
    if (content_.empty() && src_.empty()) {
        errors.push_back("Script node must have either content or src attribute");
    }

    // Check that we don't have both content and src
    if (!content_.empty() && !src_.empty()) {
        errors.push_back("Script node cannot have both content and src attribute");
    }

    // Validate script type
    if (type_ != "ecmascript" && type_ != "javascript") {
        errors.push_back("Unsupported script type: " + type_ + ". Only 'ecmascript' and 'javascript' are supported");
    }

    // Validate src file exists if specified
    if (!src_.empty()) {
        std::ifstream file(src_);
        if (!file.good()) {
            errors.push_back("Script source file not found: " + src_);
        }
    }

    return errors;
}

std::shared_ptr<IScriptNode> ScriptNode::clone() const {
    auto cloned = std::make_shared<ScriptNode>(content_, type_);
    cloned->setSrc(src_);
    cloned->setInitializationScript(isInitializationScript_);
    cloned->setExecutionPriority(executionPriority_);
    return cloned;
}

const std::string &ScriptNode::getType() const {
    return type_;
}

void ScriptNode::setType(const std::string &type) {
    SCXML::Common::Logger::debug("ScriptNode::setType - Setting script type: " + type);
    type_ = type;
}

bool ScriptNode::isInitializationScript() const {
    return isInitializationScript_;
}

void ScriptNode::setInitializationScript(bool isInit) {
    SCXML::Common::Logger::debug("ScriptNode::setInitializationScript - " + std::string(isInit ? "true" : "false"));
    isInitializationScript_ = isInit;
}

SCXML::Common::Result<void> ScriptNode::loadFromFile(const std::string &filepath) {
    SCXML::Common::Logger::debug("ScriptNode::loadFromFile - Loading from: " + filepath);

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{"ScriptNode::loadFromFile",
                                                                        "Cannot open script file: " + filepath,
                                                                        "File may not exist or be accessible"});
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        content_ = buffer.str();
        src_ = filepath;

        SCXML::Common::Logger::debug("ScriptNode::loadFromFile - Loaded " + std::to_string(content_.length()) + " characters");
        return SCXML::Common::Result<void>();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>(
            SCXML::Common::ErrorInfo{"ScriptNode::loadFromFile", "Failed to load script file: " + filepath, e.what()});
    }
}

int ScriptNode::getExecutionPriority() const {
    return executionPriority_;
}

void ScriptNode::setExecutionPriority(int priority) {
    SCXML::Common::Logger::debug("ScriptNode::setExecutionPriority - Setting priority: " + std::to_string(priority));
    executionPriority_ = priority;
}

SCXML::Common::Result<void> ScriptNode::executeECMAScript(SCXML::Runtime::RuntimeContext &context) {
    SCXML::Common::Logger::debug("ScriptNode::executeECMAScript - Executing ECMAScript content");

    try {
        // Get the data model engine for script execution
        auto dataModel = context.getDataModelEngine();
        if (!dataModel) {
            return SCXML::Common::Result<void>(
                SCXML::Common::ErrorInfo{"ScriptNode::executeECMAScript", "No data model engine available",
                                         "RuntimeContext must have an initialized data model engine"});
        }

        // Get script content (from content_ or loaded from src_)
        std::string scriptContent = content_;
        if (scriptContent.empty() && !src_.empty()) {
            auto contentResult = loadContentFromSrc();
            if (!contentResult.isSuccess()) {
                return SCXML::Common::Result<void>(contentResult.getErrors()[0]);
            }
            scriptContent = contentResult.getValue();
        }

        if (scriptContent.empty()) {
            SCXML::Common::Logger::warning("ScriptNode::executeECMAScript - No script content to execute");
            return SCXML::Common::Result<void>();
        }

        // Execute the script using the data model engine
        auto result = dataModel->evaluateExpression(scriptContent);
        if (!result.isSuccess()) {
            return SCXML::Common::Result<void>(SCXML::Common::ErrorInfo{
                "ScriptNode::executeECMAScript", "Script execution failed", result.getErrorMessage()});
        }

        SCXML::Common::Logger::debug("ScriptNode::executeECMAScript - Script executed successfully");
        return SCXML::Common::Result<void>();

    } catch (const std::exception &e) {
        return SCXML::Common::Result<void>(
            SCXML::Common::ErrorInfo{"ScriptNode::executeECMAScript", "ECMAScript execution error", e.what()});
    }
}

SCXML::Common::Result<std::string> ScriptNode::loadContentFromSrc() const {
    if (src_.empty()) {
        return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
            "ScriptNode::loadContentFromSrc", "No source file specified", "src_ attribute is empty"});
    }

    try {
        std::ifstream file(src_);
        if (!file.is_open()) {
            return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{"ScriptNode::loadContentFromSrc",
                                                                               "Cannot open source file: " + src_,
                                                                               "File may not exist or be accessible"});
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        SCXML::Common::Logger::debug("ScriptNode::loadContentFromSrc - Loaded " + std::to_string(content.length()) +
                      " characters from " + src_);
        return SCXML::Common::Result<std::string>(content);

    } catch (const std::exception &e) {
        return SCXML::Common::Result<std::string>(SCXML::Common::ErrorInfo{
            "ScriptNode::loadContentFromSrc", "Error loading from source file: " + src_, e.what()});
    }
}

}  // namespace Core
}  // namespace SCXML
