#pragma once

#include "runtime/IEventRaiser.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace RSM {
namespace Test {

/**
 * @brief Mock implementation of IEventRaiser for testing
 *
 * Records all raised events and can optionally delegate to a callback
 */
class MockEventRaiser : public IEventRaiser {
public:
    /**
     * @brief Constructor with optional callback
     * @param callback Optional callback for event handling
     */
    explicit MockEventRaiser(std::function<bool(const std::string &, const std::string &)> callback = nullptr);

    /**
     * @brief Destructor
     */
    virtual ~MockEventRaiser() = default;

    // IEventRaiser interface
    bool raiseEvent(const std::string &eventName, const std::string &eventData = "") override;
    bool isReady() const override;
    void setImmediateMode(bool immediate) override;
    void processQueuedEvents() override;

    // Test inspection methods
    const std::vector<std::pair<std::string, std::string>> &getRaisedEvents() const;
    void clearEvents();
    int getEventCount() const;

    // Test configuration
    void setCallback(std::function<bool(const std::string &, const std::string &)> callback);
    void setReady(bool ready);

private:
    std::vector<std::pair<std::string, std::string>> raisedEvents_;
    std::function<bool(const std::string &, const std::string &)> callback_;
    bool ready_ = true;
};

}  // namespace Test
}  // namespace RSM