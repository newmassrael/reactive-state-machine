#include "core/actions/ScriptActionNode.h"
#include "common/Logger.h"
#include "core/actions/ScriptActionNode.h"
#include "runtime/DataModelEngine.h"
#include "runtime/RuntimeContext.h"
#include "runtime/ActionExecutor.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace SCXML {
namespace Core {

ScriptActionNode::ScriptActionNode(const std::string &id)
    : ActionNode(id), lang_("ecmascript") {  // Default to ECMAScript
    SCXML::Common::Logger::debug("ScriptActionNode::Constructor - Creating script action: " + id);
}

void ScriptActionNode::setContent(const std::string &content) {
    content_ = content;
    SCXML::Common::Logger::debug("ScriptActionNode::setContent - Set script content (" + std::to_string(content.length()) +
                  " characters)");
}

void ScriptActionNode::setSrc(const std::string &src) {
    src_ = src;
    SCXML::Common::Logger::debug("ScriptActionNode::setSrc - Set script source: " + src);
}

void ScriptActionNode::setLang(const std::string &lang) {
    lang_ = lang;
    SCXML::Common::Logger::debug("ScriptActionNode::setLang - Set script language: " + lang);
}

bool ScriptActionNode::execute(::SCXML::Runtime::RuntimeContext &context) {
    // Use Executor pattern - create static factory
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        SCXML::Common::Logger::error("ScriptActionNode::execute - No executor available for action type: " + getActionType());
        return false;
    }

    return executor->execute(*this, context);
}

std::shared_ptr<SCXML::Model::IActionNode> ScriptActionNode::clone() const {
    auto clone = std::make_shared<ScriptActionNode>(getId());
    clone->setContent(content_);
    clone->setSrc(src_);
    clone->setLang(lang_);
    return clone;
}

std::vector<std::string> ScriptActionNode::validate() const {
    // Use Executor pattern - delegate to ScriptActionExecutor
    ::SCXML::Runtime::DefaultActionExecutorFactory factory;
    auto executor = factory.createExecutor(getActionType());
    
    if (!executor) {
        return {"No executor available for action type: " + getActionType()};
    }

    return executor->validate(*this);
}

std::string ScriptActionNode::loadScriptFromSrc(::SCXML::Runtime::RuntimeContext & /* context */) {
    if (src_.empty()) {
        return "";
    }

    try {
        // For now, assume src is a local file path
        // In a full implementation, this would support HTTP/HTTPS URLs
        std::ifstream file(src_);
        if (!file.is_open()) {
            SCXML::Common::Logger::error("ScriptActionNode::loadScriptFromSrc - Cannot open script file: " + src_);
            return "";
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        SCXML::Common::Logger::debug("ScriptActionNode::loadScriptFromSrc - Loaded script from " + src_ + " (" +
                      std::to_string(content.length()) + " characters)");

        return content;

    } catch (const std::exception &e) {
        SCXML::Common::Logger::error("ScriptActionNode::loadScriptFromSrc - Exception loading script: " + std::string(e.what()));
        return "";
    }
}

std::string ScriptActionNode::getEffectiveContent(::SCXML::Runtime::RuntimeContext &context) {
    if (!content_.empty()) {
        // Inline content takes precedence
        return content_;
    } else if (!src_.empty()) {
        // Load from external source
        return loadScriptFromSrc(context);
    }

    return "";
}

bool ScriptActionNode::executeECMAScript(const std::string &scriptContent, ::SCXML::Runtime::RuntimeContext &context) {
    SCXML::Common::Logger::debug("ScriptActionNode::executeECMAScript - Executing ECMAScript content");

    // Get data model engine for script execution
    auto dataModel = context.getDataModelEngine();
    if (!dataModel) {
        SCXML::Common::Logger::error("ScriptActionNode::executeECMAScript - No data model engine for script execution");
        return false;
    }

    try {
        // Execute script through data model engine
        // The data model engine should handle ECMAScript execution
        auto result = dataModel->evaluateExpression(scriptContent, context);

        if (result.success) {
            SCXML::Common::Logger::debug("ScriptActionNode::executeECMAScript - Script executed successfully");
            return true;  // Success
        } else {
            SCXML::Common::Logger::warning(
                "ScriptActionNode::executeECMAScript - Script execution failed, but continuing gracefully: " +
                result.errorMessage);
            // Return true to allow state transitions to continue even if script fails
            // This is more graceful behavior for SCXML compliance
            return true;
        }

    } catch (const std::exception &e) {
        SCXML::Common::Logger::warning(
            "ScriptActionNode::executeECMAScript - Exception during script execution, continuing gracefully: " +
            std::string(e.what()));
        // Return true to allow state transitions to continue even if script fails
        return true;
    }
}

}  // namespace Core
}  // namespace SCXML
