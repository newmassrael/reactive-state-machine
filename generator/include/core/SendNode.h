#pragma once
#include "common/SCXMLCommon.h"
#include "model/IExecutionContext.h"
#include "model/ISendNode.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

using SCXML::Model::ISendNode;
using SCXML::Model::ICancelNode;
using SCXML::Model::IExecutionContext;


namespace SCXML {
namespace Core {

// Forward declarations

/**
 * @brief Concrete implementation of SCXML <send> element
 *
 * This class implements the ISendNode interface and provides full
 * support for SCXML send operations including external event delivery,
 * delayed sending, and various event processors.
 */
class SendNode : public ISendNode {
public:
    SendNode();
    virtual ~SendNode() = default;

    // ISendNode interface implementation
    SCXML::Common::Result<void> execute(SCXML::Model::IExecutionContext &context) override;

    const std::string &getEvent() const override;
    void setEvent(const std::string &event) override;

    const std::string &getTarget() const override;
    void setTarget(const std::string &target) override;

    const std::string &getType() const override;
    void setType(const std::string &type) override;

    const std::string &getId() const override;
    void setId(const std::string &id) override;

    const std::string &getDelay() const override;
    void setDelay(const std::string &delay) override;

    const std::string &getContent() const override;
    void setContent(const std::string &content) override;

    const std::string &getContentExpr() const override;
    void setContentExpr(const std::string &contentExpr) override;

    const std::map<std::string, std::string> &getNameLocationPairs() const override;
    void addNameLocationPair(const std::string &name, const std::string &location) override;

    std::vector<std::string> validate() const override;
    std::shared_ptr<ISendNode> clone() const override;

    bool isDelayed() const override;
    bool isInternalTarget() const override;

    SCXML::Common::Result<std::string> resolveEventData(SCXML::Model::IExecutionContext &context) const override;
    SCXML::Common::Result<std::string> resolveTarget(SCXML::Model::IExecutionContext &context) const override;
    SCXML::Common::Result<std::string> resolveEventName(SCXML::Model::IExecutionContext &context) const override;

private:
    std::string event_;                                     ///< Event name or expression
    std::string target_;                                    ///< Target URI or expression
    std::string type_;                                      ///< Event processor type
    std::string id_;                                        ///< Send ID for cancellation
    std::string delay_;                                     ///< Delay before sending
    std::string content_;                                   ///< Static content data
    std::string contentExpr_;                               ///< Expression for dynamic content
    std::map<std::string, std::string> nameLocationPairs_;  ///< Name-location pairs for data

    /**
     * @brief Resolve delay expression to milliseconds
     * @param context Runtime context for evaluation
     * @return Result containing delay in milliseconds or error
     */
    SCXML::Common::Result<int> resolveDelay(SCXML::Model::IExecutionContext &context) const;

    /**
     * @brief Parse delay string to milliseconds
     * @param delayStr Delay string (e.g., "1s", "500ms")
     * @return Result containing milliseconds or error
     */
    SCXML::Common::Result<int> parseDelayString(const std::string &delayStr) const;

    /**
     * @brief Check if event processor type is valid
     * @param type Event processor type to validate
     * @return True if type is valid
     */
    bool isValidEventProcessorType(const std::string &type) const;

    /**
     * @brief Check if delay format is valid
     * @param delay Delay string to validate
     * @return True if format is valid
     */
    bool isValidDelayFormat(const std::string &delay) const;
};

/**
 * @brief Concrete implementation of SCXML <cancel> element
 *
 * This class implements the ICancelNode interface and provides
 * cancellation of previously sent events using their send IDs.
 */
class CancelNode : public ICancelNode {
public:
    CancelNode();
    virtual ~CancelNode() = default;

    // ICancelNode interface implementation
    SCXML::Common::Result<void> execute(SCXML::Model::IExecutionContext &context) override;

    const std::string &getSendId() const override;
    void setSendId(const std::string &sendId) override;

    const std::string &getSendIdExpr() const override;
    void setSendIdExpr(const std::string &sendIdExpr) override;

    std::vector<std::string> validate() const override;
    std::shared_ptr<ICancelNode> clone() const override;

    SCXML::Common::Result<std::string> resolveSendId(SCXML::Model::IExecutionContext &context) const override;

private:
    std::string sendId_;      ///< Send ID to cancel
    std::string sendIdExpr_;  ///< Expression for send ID
};

} // namespace Core
}  // namespace SCXML
