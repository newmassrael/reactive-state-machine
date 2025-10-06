#include "events/EventTargetFactoryImpl.h"
#include "common/Logger.h"
#include "events/EventRaiserService.h"
#include "events/HttpEventTarget.h"
#include "events/InternalEventTarget.h"
#include "events/InvokeEventTarget.h"
#include "events/ParentEventTarget.h"
#include "runtime/IEventRaiser.h"
#include <algorithm>
#include <stdexcept>

namespace RSM {

EventTargetFactoryImpl::EventTargetFactoryImpl(std::shared_ptr<IEventRaiser> eventRaiser,
                                               std::shared_ptr<IEventScheduler> scheduler)
    : eventRaiser_(std::move(eventRaiser)), scheduler_(std::move(scheduler)) {
    if (!eventRaiser_) {
        throw std::invalid_argument("EventTargetFactoryImpl requires a valid event raiser");
    }

    // Register internal target creator
    registerTargetType("internal",
                       [this](const std::string &targetUri) { return createInternalTarget(targetUri, ""); });

    // Register HTTP target creator
    registerTargetType("http", [](const std::string &targetUri) {
        LOG_DEBUG("EventTargetFactoryImpl: Creating HTTP target for URI: {}", targetUri);
        auto target = std::make_shared<HttpEventTarget>(targetUri);
        LOG_DEBUG("EventTargetFactoryImpl: HTTP target created successfully: {}", target->getDebugInfo());
        return target;
    });

    // Register HTTPS target creator
    registerTargetType("https", [](const std::string &targetUri) {
        LOG_DEBUG("EventTargetFactoryImpl: Creating HTTPS target for URI: {}", targetUri);
        auto target = std::make_shared<HttpEventTarget>(targetUri);
        LOG_DEBUG("EventTargetFactoryImpl: HTTPS target created successfully: {}", target->getDebugInfo());
        return target;
    });

    LOG_DEBUG("EventTargetFactoryImpl: Factory created with internal, HTTP, and HTTPS target support");
}

std::shared_ptr<IEventTarget> EventTargetFactoryImpl::createTarget(const std::string &targetUri,
                                                                   const std::string &sessionId) {
    if (targetUri.empty()) {
        // W3C SCXML compliance: Empty target means external queue (Test 189)
        LOG_DEBUG("EventTargetFactoryImpl: Empty target URI, creating external queue target");
        return createExternalTarget(sessionId);
    }

    // Handle special internal target URI
    if (targetUri == "#_internal") {
        return createInternalTarget(targetUri, sessionId);
    }

    // Handle special parent target URI (#_parent)
    if (targetUri == "#_parent") {
        LOG_DEBUG("EventTargetFactoryImpl::createTarget() - Creating #_parent target");
        return createParentTarget(targetUri);
    }

    // Handle invoke ID target URI (#_invokeId)
    if (targetUri.starts_with("#_") && targetUri != "#_internal" && targetUri != "#_parent") {
        std::string invokeId = targetUri.substr(2);  // Remove "#_" prefix
        LOG_DEBUG("EventTargetFactoryImpl::createTarget() - Creating invoke target for ID: {}", invokeId);
        return createInvokeTarget(invokeId, sessionId);
    }

    // Extract scheme and find appropriate creator
    std::string scheme = extractScheme(targetUri);

    auto creatorIt = targetCreators_.find(scheme);
    if (creatorIt != targetCreators_.end()) {
        LOG_DEBUG("EventTargetFactoryImpl: Creating '{}' target for URI: {}", scheme, targetUri);

        try {
            auto target = creatorIt->second(targetUri);
            if (!target) {
                LOG_ERROR("EventTargetFactoryImpl: Target creator returned null for URI: {}", targetUri);
                return nullptr;
            }

            // Validate the created target
            auto errors = target->validate();
            if (!errors.empty()) {
                LOG_ERROR("EventTargetFactoryImpl: Target validation failed for URI '{}': {}", targetUri,
                          errors.front());
                return nullptr;
            }

            return target;

        } catch (const std::exception &e) {
            LOG_ERROR("EventTargetFactoryImpl: Error creating target for URI '{}': {}", targetUri, e.what());
            return nullptr;
        }
    }

    LOG_WARN("EventTargetFactoryImpl: No creator found for scheme '{}' in URI: {}", scheme, targetUri);
    return nullptr;
}

std::vector<std::string> EventTargetFactoryImpl::getSupportedSchemes() const {
    std::vector<std::string> schemes;
    schemes.reserve(targetCreators_.size() + 1);  // +1 for internal

    schemes.push_back("internal");  // Always supported

    for (const auto &pair : targetCreators_) {
        if (pair.first != "internal") {  // Avoid duplicating internal
            schemes.push_back(pair.first);
        }
    }

    return schemes;
}

void EventTargetFactoryImpl::registerTargetType(
    const std::string &scheme, std::function<std::shared_ptr<IEventTarget>(const std::string &)> creator) {
    if (scheme.empty()) {
        throw std::invalid_argument("Target scheme cannot be empty");
    }

    if (!creator) {
        throw std::invalid_argument("Target creator cannot be null");
    }

    LOG_DEBUG("EventTargetFactoryImpl: Registering target type for scheme: {}", scheme);
    targetCreators_[scheme] = creator;
}

bool EventTargetFactoryImpl::isSchemeSupported(const std::string &scheme) const {
    if (scheme.empty()) {
        return false;
    }

    if (scheme == "internal") {
        return true;  // Internal scheme always supported
    }

    return targetCreators_.find(scheme) != targetCreators_.end();
}

void EventTargetFactoryImpl::unregisterTargetCreator(const std::string &scheme) {
    if (scheme == "internal") {
        LOG_WARN("EventTargetFactoryImpl: Cannot unregister internal target creator");
        return;
    }

    auto removed = targetCreators_.erase(scheme);
    if (removed > 0) {
        LOG_DEBUG("EventTargetFactoryImpl: Unregistered target creator for scheme: {}", scheme);
    } else {
        LOG_DEBUG("EventTargetFactoryImpl: No target creator found for scheme: {}", scheme);
    }
}

std::string EventTargetFactoryImpl::extractScheme(const std::string &targetUri) const {
    if (targetUri.empty()) {
        return "internal";
    }

    // Handle special internal URI
    if (targetUri == "#_internal") {
        return "internal";
    }

    // Find scheme separator
    size_t colonPos = targetUri.find(':');
    if (colonPos == std::string::npos) {
        // No scheme specified, assume internal
        return "internal";
    }

    std::string scheme = targetUri.substr(0, colonPos);

    // Convert to lowercase for case-insensitive matching
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);

    return scheme;
}

std::shared_ptr<IEventTarget> EventTargetFactoryImpl::createInternalTarget(const std::string &targetUri,
                                                                           const std::string &sessionId) {
    try {
        // Use session-specific EventRaiser if sessionId is provided
        std::shared_ptr<IEventRaiser> targetEventRaiser = eventRaiser_;  // Default fallback

        if (!sessionId.empty()) {
            LOG_DEBUG("EventTargetFactoryImpl: Looking up EventRaiser for sessionId='{}'", sessionId);

            auto sessionEventRaiser = EventRaiserService::getInstance().getEventRaiser(sessionId);
            if (sessionEventRaiser) {
                targetEventRaiser = sessionEventRaiser;
                LOG_DEBUG("EventTargetFactoryImpl: Found session-specific EventRaiser for session: '{}', ready={}",
                          sessionId, sessionEventRaiser->isReady());
            } else {
                LOG_DEBUG("EventTargetFactoryImpl: Session EventRaiser not found for session: '{}', using default",
                          sessionId);
            }
        }

        // W3C SCXML 5.10: Pass sessionId for _event.origin (test 336)
        auto target =
            std::make_shared<InternalEventTarget>(targetEventRaiser, false, sessionId);  // Internal queue priority

        LOG_DEBUG("EventTargetFactoryImpl: Created internal target for URI: {} with session: {}", targetUri, sessionId);
        return target;

    } catch (const std::exception &e) {
        LOG_ERROR("EventTargetFactoryImpl: Error creating internal target: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<IEventTarget> EventTargetFactoryImpl::createExternalTarget(const std::string &sessionId) {
    try {
        // Use session-specific EventRaiser if sessionId is provided
        std::shared_ptr<IEventRaiser> targetEventRaiser = eventRaiser_;  // Default fallback

        if (!sessionId.empty()) {
            auto sessionEventRaiser = EventRaiserService::getInstance().getEventRaiser(sessionId);
            if (sessionEventRaiser) {
                targetEventRaiser = sessionEventRaiser;
                LOG_DEBUG("EventTargetFactoryImpl: Using session-specific EventRaiser for EXTERNAL target, session: {}",
                          sessionId);
            } else {
                LOG_DEBUG("EventTargetFactoryImpl: Session EventRaiser not found for EXTERNAL target, session: {}, "
                          "using default",
                          sessionId);
            }
        }

        // W3C SCXML compliance: External target uses EXTERNAL priority for proper queue ordering
        // W3C SCXML 5.10: Pass sessionId for _event.origin (test 336)
        auto target =
            std::make_shared<InternalEventTarget>(targetEventRaiser, true, sessionId);  // External queue priority

        LOG_DEBUG("EventTargetFactoryImpl: Created external target for W3C SCXML compliance with session: {}",
                  sessionId);
        return target;

    } catch (const std::exception &e) {
        LOG_ERROR("EventTargetFactoryImpl: Error creating external target: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<RSM::IEventTarget> RSM::EventTargetFactoryImpl::createParentTarget(const std::string &targetUri) {
    try {
        // For parent target, we need to determine the child session ID
        // This will be passed through the event dispatcher context
        // For now, we create a ParentEventTarget that will resolve the child session at send time

        // Note: The actual child session ID will be determined when the event is sent
        // ParentEventTarget will use the current session context to find the parent
        auto target = std::make_shared<RSM::ParentEventTarget>("dynamic", eventRaiser_, scheduler_);

        LOG_DEBUG("EventTargetFactoryImpl: Created parent target for URI: {}", targetUri);
        return target;

    } catch (const std::exception &e) {
        LOG_ERROR("EventTargetFactoryImpl: Error creating parent target: {}", e.what());
        return nullptr;
    }
}

std::shared_ptr<RSM::IEventTarget> RSM::EventTargetFactoryImpl::createInvokeTarget(const std::string &invokeId,
                                                                                   const std::string &sessionId) {
    try {
        // Create invoke event target for routing to child session
        auto target = std::make_shared<RSM::InvokeEventTarget>(invokeId, sessionId);

        LOG_DEBUG("EventTargetFactoryImpl: Created invoke target for ID '{}' from session '{}'", invokeId, sessionId);
        return target;

    } catch (const std::exception &e) {
        LOG_ERROR("EventTargetFactoryImpl: Error creating invoke target for ID '{}': {}", invokeId, e.what());
        return nullptr;
    }
}

}  // namespace RSM
