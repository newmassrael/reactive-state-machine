#pragma once

#include "IEventTarget.h"
#include <memory>
#include <string>

namespace RSM {

/**
 * @brief Event target for invoke session communication (#_invokeid)
 *
 * W3C SCXML: Handles events targeted to invoke sessions using their invoke ID.
 * Routes events to the external queue of the specified child session via JSEngine.
 */
class InvokeEventTarget : public IEventTarget {
public:
    /**
     * @brief Constructor with invoke ID and parent session
     * @param invokeId The invoke ID to target
     * @param parentSessionId The parent session that created this invoke
     */
    InvokeEventTarget(const std::string &invokeId, const std::string &parentSessionId);

    ~InvokeEventTarget() override = default;

    // IEventTarget interface
    std::future<SendResult> send(const EventDescriptor &event) override;
    std::string getTargetType() const override;
    bool canHandle(const std::string &targetUri) const override;
    std::vector<std::string> validate() const override;
    std::string getDebugInfo() const override;

private:
    std::string invokeId_;
    std::string parentSessionId_;
};

}  // namespace RSM