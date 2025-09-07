#pragma once
#include "common/Result.h"
#include "model/IScriptNode.h"
#include <memory>
#include <string>
#include <vector>

using SCXML::Model::IScriptNode;


namespace SCXML {
namespace Core {

/**
 * @brief Implementation of SCXML <script> element
 *
 * Provides executable script content for state machine initialization
 * and data model manipulation.
 */
class ScriptNode : public IScriptNode {
public:
    /**
     * @brief Constructor
     * @param content Script source code (optional)
     * @param type Script type/language (default: "ecmascript")
     */
    explicit ScriptNode(const std::string &content = "", const std::string &type = "ecmascript");

    /**
     * @brief Destructor
     */
    virtual ~ScriptNode() = default;

    // IScriptNode interface implementation
    virtual const std::string &getContent() const override;
    virtual void setContent(const std::string &content) override;
    virtual const std::string &getSrc() const override;
    virtual void setSrc(const std::string &src) override;
    virtual SCXML::Common::Result<void> execute(SCXML::Model::IExecutionContext &context) override;
    virtual std::vector<std::string> validate() const override;
    virtual std::shared_ptr<IScriptNode> clone() const override;
    virtual const std::string &getType() const override;
    virtual void setType(const std::string &type) override;
    virtual bool isInitializationScript() const override;
    virtual void setInitializationScript(bool isInit) override;

    /**
     * @brief Load script content from file
     * @param filepath Path to script file
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> loadFromFile(const std::string &filepath);

    /**
     * @brief Get script execution order/priority
     * @return Execution priority (lower numbers execute first)
     */
    int getExecutionPriority() const;

    /**
     * @brief Set script execution order/priority
     * @param priority Execution priority
     */
    void setExecutionPriority(int priority);

private:
    std::string content_;          ///< Script source code
    std::string src_;              ///< Source file path
    std::string type_;             ///< Script type/language
    bool isInitializationScript_;  ///< Whether script runs at document load
    int executionPriority_;        ///< Execution order priority



    /**
     * @brief Load content from src file if specified
     * @return Result of file loading
     */
    SCXML::Common::Result<std::string> loadContentFromSrc() const;
};

} // namespace Core
}  // namespace SCXML
