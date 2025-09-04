#pragma once

#include "common/Result.h"
#include <memory>
#include <string>

namespace SCXML {
namespace Common {

/**
 * Basic state machine interface for module system
 * This is a minimal implementation for testing purposes
 */
class StateMachine {
public:
    StateMachine() = default;
    virtual ~StateMachine() = default;

    /**
     * Start the state machine
     */
    virtual SCXML::Common::Result<void> start() {
        isRunning_ = true;
        return SCXML::Common::Result<void>::success();
    }

    /**
     * Stop the state machine
     */
    virtual SCXML::Common::Result<void> stop() {
        isRunning_ = false;
        return SCXML::Common::Result<void>::success();
    }

    /**
     * Check if state machine is running
     */
    virtual bool isRunning() const {
        return isRunning_;
    }

    /**
     * Get state machine ID
     */
    virtual std::string getId() const {
        return id_;
    }

    /**
     * Set state machine ID
     */
    virtual void setId(const std::string &id) {
        id_ = id;
    }

private:
    bool isRunning_ = false;
    std::string id_;
};

}  // namespace Common
}  // namespace SCXML
