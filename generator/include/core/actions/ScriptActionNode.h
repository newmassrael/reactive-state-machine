#pragma once
#include "common/Result.h"
#include "core/ActionNode.h"
#include <memory>

namespace SCXML {
namespace Events {
class Event;
using EventPtr = std::shared_ptr<Event>;
}  // namespace Events

namespace Runtime {
class RuntimeContext;
}

namespace Core {

/**
 * @brief SCXML <script> action implementation
 *
 * The <script> element allows embedding of script code (typically ECMAScript/JavaScript)
 * that will be executed within the SCXML context. This provides powerful programmatic
 * control over the state machine behavior.
 */
class ScriptActionNode : public ActionNode {
public:
    /**
     * @brief Construct a new Script Action Node
     * @param id Action identifier
     */
    explicit ScriptActionNode(const std::string &id);

    /**
     * @brief Destructor
     */
    virtual ~ScriptActionNode() = default;

    /**
     * @brief Set the script content to execute
     * @param content Script code content (typically ECMAScript)
     */
    void setContent(const std::string &content);

    /**
     * @brief Get the script content
     * @return script content string
     */
    const std::string &getContent() const {
        return content_;
    }

    /**
     * @brief Set the script source file URL
     * @param src URL or path to external script file
     */
    void setSrc(const std::string &src);

    /**
     * @brief Get the script source URL
     * @return source URL string
     */
    const std::string &getSrc() const {
        return src_;
    }

    /**
     * @brief Set the script language/type
     * @param lang Script language (e.g., "ecmascript", "javascript")
     */
    void setLang(const std::string &lang);

    /**
     * @brief Get the script language
     * @return language string
     */
    const std::string &getLang() const {
        return lang_;
    }

    /**
     * @brief Execute the script action
     * @param context Runtime context for execution
     * @return true if script execution was successful
     */
    virtual bool execute(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Get action type name
     * @return "script"
     */
    std::string getActionType() const {
        return "script";
    }

    /**
     * @brief Clone this action node
     * @return Deep copy of this ScriptActionNode
     */
    std::shared_ptr<IActionNode> clone() const;

    /**
     * @brief Validate script action configuration
     * @return Vector of validation error messages (empty if valid)
     */
    std::vector<std::string> validate() const;

protected:
    /**
     * @brief Load script content from external source
     * @param context Runtime context for file access
     * @return Loaded script content, empty if load fails
     */
    std::string loadScriptFromSrc(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Determine the effective script content (inline or from src)
     * @param context Runtime context for src loading
     * @return Script content to execute
     */
    std::string getEffectiveContent(::SCXML::Runtime::RuntimeContext &context);

    /**
     * @brief Execute ECMAScript/JavaScript content
     * @param scriptContent Script code to execute
     * @param context Runtime context with data model access
     * @return true if execution succeeded
     */
    bool executeECMAScript(const std::string &scriptContent, ::SCXML::Runtime::RuntimeContext &context);

private:
    std::string content_;  // Inline script content
    std::string src_;      // External script file URL
    std::string lang_;     // Script language (default: "ecmascript")
};

} // namespace Core
}  // namespace SCXML
