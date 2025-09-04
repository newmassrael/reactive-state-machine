#pragma once
#include "common/SCXMLCommon.h"
#include "model/IExecutionContext.h"
#include "model/IForeachNode.h"
#include <memory>
#include <string>
#include <vector>

using SCXML::Model::IForeachNode;
using SCXML::Model::ILogNode;
using SCXML::Model::IExecutionContext;


namespace SCXML {
namespace Core {

/**
 * @brief Concrete implementation of SCXML <foreach> element
 *
 * This class implements the IForeachNode interface and provides
 * iteration over arrays and collections in the data model.
 */
class ForeachNode : public IForeachNode {
public:
    ForeachNode();
    virtual ~ForeachNode() = default;

    // IForeachNode interface implementation
    SCXML::Common::Result<void> execute(SCXML::Model::IExecutionContext &context) override;

    const std::string &getArray() const override;
    void setArray(const std::string &array) override;

    const std::string &getItem() const override;
    void setItem(const std::string &item) override;

    const std::string &getIndex() const override;
    void setIndex(const std::string &index) override;

    void addChild(std::shared_ptr<void> child) override;
    std::vector<std::shared_ptr<void>> getChildren() const override;

    std::vector<std::string> validate() const override;
    std::shared_ptr<IForeachNode> clone() const override;

    SCXML::Common::Result<std::vector<std::string>>
    resolveArray(SCXML::Model::IExecutionContext &context) const override;

private:
    std::string array_;                            ///< Array expression to iterate over
    std::string item_;                             ///< Variable name for each item
    std::string index_;                            ///< Variable name for current index
    std::vector<std::shared_ptr<void>> children_;  ///< Child executable elements

    /**
     * @brief Execute child elements for a specific iteration
     * @param context Runtime context
     * @param itemValue Current item value
     * @param indexValue Current index value
     * @return Result indicating success or failure
     */
    SCXML::Common::Result<void> executeIteration(SCXML::Model::IExecutionContext &context, const std::string &itemValue,
                                                 int indexValue);

    /**
     * @brief Parse array from JSON string or data model location
     * @param arrayData Array data as string
     * @param context Runtime context
     * @return Vector of array items
     */
    std::vector<std::string> parseArrayData(const std::string &arrayData,
                                            SCXML::Model::IExecutionContext &context) const;
};

/**
 * @brief Concrete implementation of SCXML <log> element
 *
 * This class implements the ILogNode interface and provides
 * logging functionality for debugging and diagnostic output.
 */
class LogNode : public ILogNode {
public:
    LogNode();
    virtual ~LogNode() = default;

    // ILogNode interface implementation
    SCXML::Common::Result<void> execute(SCXML::Model::IExecutionContext &context) override;

    const std::string &getExpr() const override;
    void setExpr(const std::string &expr) override;

    const std::string &getLabel() const override;
    void setLabel(const std::string &label) override;

    const std::string &getLevel() const override;
    void setLevel(const std::string &level) override;

    std::vector<std::string> validate() const override;
    std::shared_ptr<ILogNode> clone() const override;

    SCXML::Common::Result<std::string> resolveMessage(SCXML::Model::IExecutionContext &context) const override;

private:
    std::string expr_;   ///< Log message expression
    std::string label_;  ///< Log label for categorization
    std::string level_;  ///< Log level (info, debug, warn, error)

    /**
     * @brief Check if log level is valid
     * @param level Log level to validate
     * @return True if level is valid
     */
    bool isValidLogLevel(const std::string &level) const;

    /**
     * @brief Format log message with timestamp and metadata
     * @param message Resolved log message
     * @param context Runtime context
     * @return Formatted log message
     */
    std::string formatLogMessage(const std::string &message, SCXML::Model::IExecutionContext &context) const;
};

} // namespace Core
}  // namespace SCXML
