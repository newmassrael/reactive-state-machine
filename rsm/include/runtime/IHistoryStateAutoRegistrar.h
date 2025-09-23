#pragma once

#include <memory>
#include <string>

namespace RSM {

// Forward declarations
class SCXMLModel;
class IHistoryManager;

/**
 * @brief Interface for automatic registration of history states from SCXML models
 *
 * This interface follows SOLID principles:
 * - Single Responsibility: Only handles auto-registration of history states
 * - Open/Closed: Open for extension, closed for modification
 * - Interface Segregation: Focused interface for auto-registration
 * - Dependency Inversion: Depends on abstractions (IHistoryManager)
 *
 * SCXML W3C Section 3.6 Compliance:
 * Automatically registers history states declared in SCXML documents
 * according to W3C SCXML 1.0 specification requirements.
 */
class IHistoryStateAutoRegistrar {
public:
    virtual ~IHistoryStateAutoRegistrar() = default;

    /**
     * @brief Auto-register all history states from SCXML model
     * @param model SCXML model containing parsed history states
     * @param historyManager History manager to register states with
     * @return true if all history states were successfully registered
     */
    virtual bool autoRegisterHistoryStates(const std::shared_ptr<SCXMLModel> &model,
                                           IHistoryManager *historyManager) = 0;

    /**
     * @brief Get count of auto-registered history states
     * @return Number of history states that were auto-registered
     */
    virtual size_t getRegisteredHistoryStateCount() const = 0;

    /**
     * @brief Check if auto-registration is enabled
     * @return true if auto-registration is enabled
     */
    virtual bool isAutoRegistrationEnabled() const = 0;

    /**
     * @brief Enable or disable auto-registration
     * @param enabled Whether to enable auto-registration
     */
    virtual void setAutoRegistrationEnabled(bool enabled) = 0;
};

}  // namespace RSM