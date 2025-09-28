#pragma once

#include "IEventTarget.h"
#include <memory>
#include <string>

namespace RSM {

class IEventRaiser;

/**
 * @brief Event target for routing events to parent sessions (#_parent)
 *
 * W3C SCXML 6.2: This target handles the special "#_parent" target used
 * in invoke scenarios where child sessions need to send events to their
 * parent session.
 */
class ParentEventTarget : public IEventTarget {
public:
    /**
     * @brief Construct parent event target
     * @param childSessionId The child session ID that wants to send to parent
     * @param eventRaiser Event raiser for delivering events to parent session
     */
    ParentEventTarget(const std::string &childSessionId, std::shared_ptr<IEventRaiser> eventRaiser);

    virtual ~ParentEventTarget() = default;

    // IEventTarget implementation
    std::future<SendResult> send(const EventDescriptor &event) override;
    std::vector<std::string> validate() const override;
    std::string getTargetType() const override;
    bool canHandle(const std::string &targetUri) const override;
    std::string getDebugInfo() const override;

private:
    std::string childSessionId_;
    std::shared_ptr<IEventRaiser> eventRaiser_;

    /**
     * @brief Find parent session ID for the given child session
     * @param childSessionId Child session ID
     * @return Parent session ID or empty string if not found
     */
    std::string findParentSessionId(const std::string &childSessionId) const;
};

}  // namespace RSM