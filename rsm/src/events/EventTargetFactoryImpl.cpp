#include "events/EventTargetFactoryImpl.h"
#include "common/Logger.h"
#include "events/HttpEventTarget.h"
#include "events/InternalEventTarget.h"
#include "runtime/IEventRaiser.h"
#include <algorithm>
#include <stdexcept>

namespace RSM {

EventTargetFactoryImpl::EventTargetFactoryImpl(std::shared_ptr<IEventRaiser> eventRaiser)
    : eventRaiser_(std::move(eventRaiser)) {
    if (!eventRaiser_) {
        throw std::invalid_argument("EventTargetFactoryImpl requires a valid event raiser");
    }

    // Register internal target creator
    registerTargetType("internal", [this](const std::string &targetUri) { return createInternalTarget(targetUri); });

    // Register HTTP target creator
    registerTargetType("http",
                       [](const std::string &targetUri) { return std::make_shared<HttpEventTarget>(targetUri); });

    // Register HTTPS target creator
    registerTargetType("https",
                       [](const std::string &targetUri) { return std::make_shared<HttpEventTarget>(targetUri); });

    Logger::debug("EventTargetFactoryImpl: Factory created with internal, HTTP, and HTTPS target support");
}

std::shared_ptr<IEventTarget> EventTargetFactoryImpl::createTarget(const std::string &targetUri) {
    if (targetUri.empty()) {
        Logger::warn("EventTargetFactoryImpl: Empty target URI, defaulting to internal");
        return createInternalTarget("#_internal");
    }

    // Handle special internal target URI
    if (targetUri == "#_internal") {
        return createInternalTarget(targetUri);
    }

    // Extract scheme and find appropriate creator
    std::string scheme = extractScheme(targetUri);

    auto creatorIt = targetCreators_.find(scheme);
    if (creatorIt != targetCreators_.end()) {
        Logger::debug("EventTargetFactoryImpl: Creating '{}' target for URI: {}", scheme, targetUri);

        try {
            auto target = creatorIt->second(targetUri);
            if (!target) {
                Logger::error("EventTargetFactoryImpl: Target creator returned null for URI: {}", targetUri);
                return nullptr;
            }

            // Validate the created target
            auto errors = target->validate();
            if (!errors.empty()) {
                Logger::error("EventTargetFactoryImpl: Target validation failed for URI '{}': {}", targetUri,
                              errors.front());
                return nullptr;
            }

            return target;

        } catch (const std::exception &e) {
            Logger::error("EventTargetFactoryImpl: Error creating target for URI '{}': {}", targetUri, e.what());
            return nullptr;
        }
    }

    Logger::warn("EventTargetFactoryImpl: No creator found for scheme '{}' in URI: {}", scheme, targetUri);
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

    Logger::debug("EventTargetFactoryImpl: Registering target type for scheme: {}", scheme);
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
        Logger::warn("EventTargetFactoryImpl: Cannot unregister internal target creator");
        return;
    }

    auto removed = targetCreators_.erase(scheme);
    if (removed > 0) {
        Logger::debug("EventTargetFactoryImpl: Unregistered target creator for scheme: {}", scheme);
    } else {
        Logger::debug("EventTargetFactoryImpl: No target creator found for scheme: {}", scheme);
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

std::shared_ptr<IEventTarget> EventTargetFactoryImpl::createInternalTarget(const std::string &targetUri) {
    try {
        auto target = std::make_shared<InternalEventTarget>(eventRaiser_);

        Logger::debug("EventTargetFactoryImpl: Created internal target for URI: {}", targetUri);
        return target;

    } catch (const std::exception &e) {
        Logger::error("EventTargetFactoryImpl: Error creating internal target: {}", e.what());
        return nullptr;
    }
}

}  // namespace RSM