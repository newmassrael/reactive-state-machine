#pragma once

#include "IEventTarget.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace RSM {

// Forward declarations
class IActionExecutor;
class IEventRaiser;
class IEventScheduler;

/**
 * @brief Concrete implementation of IEventTargetFactory
 *
 * This factory creates appropriate event targets based on target URIs.
 * It supports registration of target creators and automatic target
 * selection based on URI schemes.
 *
 * Supported target types:
 * - "#_internal" - Internal events using action executor
 * - Future: "http://", "https://", "scxml:", etc.
 */
class EventTargetFactoryImpl : public IEventTargetFactory {
public:
    /**
     * @brief Target creator function type
     */
    /**
     * @brief Construct factory with event raiser for internal events
     *
     * @param eventRaiser Event raiser for internal event delivery
     * @param scheduler Event scheduler for delayed events (optional)
     */
    explicit EventTargetFactoryImpl(std::shared_ptr<IEventRaiser> eventRaiser,
                                    std::shared_ptr<IEventScheduler> scheduler = nullptr);

    /**
     * @brief Destructor
     */
    virtual ~EventTargetFactoryImpl() = default;

    // IEventTargetFactory implementation
    std::shared_ptr<IEventTarget> createTarget(const std::string &targetUri,
                                               const std::string &sessionId = "") override;
    void registerTargetType(const std::string &scheme,
                            std::function<std::shared_ptr<IEventTarget>(const std::string &)> creator) override;
    bool isSchemeSupported(const std::string &scheme) const override;
    std::vector<std::string> getSupportedSchemes() const override;

    /**
     * @brief Unregister a target creator for a URI scheme
     *
     * @param scheme URI scheme to unregister
     */
    void unregisterTargetCreator(const std::string &scheme);

private:
    /**
     * @brief Extract scheme from target URI
     *
     * @param targetUri URI to parse
     * @return Scheme part (e.g., "http" from "http://example.com")
     */
    std::string extractScheme(const std::string &targetUri) const;

    /**
     * @brief Create internal event target
     *
     * @param targetUri Target URI (should be "#_internal")
     * @param sessionId Session ID for session-specific EventRaiser
     * @return Internal event target
     */
    std::shared_ptr<IEventTarget> createInternalTarget(const std::string &targetUri, const std::string &sessionId);

    /**
     * @brief Create external event target (for W3C SCXML external queue compliance)
     *
     * @return External event target that uses EXTERNAL priority
     */
    std::shared_ptr<IEventTarget> createExternalTarget(const std::string &sessionId = "");

    /**
     * @brief Create parent event target for #_parent routing
     *
     * @param targetUri Target URI (should be "#_parent")
     * @return Parent event target
     */
    std::shared_ptr<IEventTarget> createParentTarget(const std::string &targetUri);

    /**
     * @brief Create invoke event target for #_invokeId routing
     *
     * @param invokeId Invoke ID to target
     * @param sessionId Current session ID (parent session)
     * @return Invoke event target
     */
    std::shared_ptr<IEventTarget> createInvokeTarget(const std::string &invokeId, const std::string &sessionId);

    std::shared_ptr<IEventRaiser> eventRaiser_;
    std::shared_ptr<IEventScheduler> scheduler_;
    std::map<std::string, std::function<std::shared_ptr<IEventTarget>(const std::string &)>> targetCreators_;
};

}  // namespace RSM