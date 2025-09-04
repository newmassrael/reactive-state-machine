#pragma once

#include <memory>
#include <string>

namespace SCXML {
namespace Runtime {

// Forward declarations
class IStateManager;
class IEventManager;
class IDataContextManager;
class IInvokeSessionManager;

/**
 * @brief Interface for runtime context
 *
 * Provides access to specialized managers for different aspects
 * of SCXML runtime execution.
 */
class IRuntimeContext {
public:
    virtual ~IRuntimeContext() = default;

    // Manager access
    virtual IStateManager &getStateManager() = 0;
    virtual const IStateManager &getStateManager() const = 0;

    virtual IEventManager &getEventManager() = 0;
    virtual const IEventManager &getEventManager() const = 0;

    virtual IDataContextManager &getDataContextManager() = 0;
    virtual const IDataContextManager &getDataContextManager() const = 0;

    virtual IInvokeSessionManager &getInvokeSessionManager() = 0;
    virtual const IInvokeSessionManager &getInvokeSessionManager() const = 0;

    // Utility methods
    virtual void log(const std::string &level, const std::string &message) = 0;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;
    virtual std::string getStatusSummary() const = 0;
};

}  // namespace Runtime
}  // namespace SCXML