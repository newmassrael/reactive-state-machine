#pragma once
#include "IExecutionContext.h"
#include "common/Result.h"
#include <memory>
#include <string>

namespace SCXML {
namespace Model {

// Forward declarations

/**
 * @brief Interface for SCXML <script> element
 *
 * The <script> element is used to add scripting capability to the state machine.
 * It allows executable content to be evaluated when the document is loaded.
 */
class IScriptNode {
public:
    virtual ~IScriptNode() = default;

    /**
     * @brief Get the script content/source
     * @return Script source code
     */
    virtual const std::string &getContent() const = 0;

    /**
     * @brief Set the script content
     * @param content Script source code
     */
    virtual void setContent(const std::string &content) = 0;

    /**
     * @brief Get the script source file location (if any)
     * @return Source file path or empty string
     */
    virtual const std::string &getSrc() const = 0;

    /**
     * @brief Set the script source file location
     * @param src Source file path
     */
    virtual void setSrc(const std::string &src) = 0;

    /**
     * @brief Execute the script in the given execution context
     * @param context Execution context for script execution
     * @return Result of script execution
     */
    virtual SCXML::Common::Result<void> execute(IExecutionContext &context) = 0;

    /**
     * @brief Validate the script node configuration
     * @return Vector of validation errors (empty if valid)
     */
    virtual std::vector<std::string> validate() const = 0;

    /**
     * @brief Clone this script node
     * @return Shared pointer to cloned script node
     */
    virtual std::shared_ptr<IScriptNode> clone() const = 0;

    /**
     * @brief Get script type/language (e.g., "ecmascript")
     * @return Script type identifier
     */
    virtual const std::string &getType() const = 0;

    /**
     * @brief Set script type/language
     * @param type Script type identifier
     */
    virtual void setType(const std::string &type) = 0;

    /**
     * @brief Check if script should be executed at document load time
     * @return true if script is for initialization
     */
    virtual bool isInitializationScript() const = 0;

    /**
     * @brief Set whether script is for initialization
     * @param isInit true if script should run at document load
     */
    virtual void setInitializationScript(bool isInit) = 0;
};

}  // namespace Model
}  // namespace SCXML
