#include "SimpleLight_sm.h"
#include <iostream>
#include <string>
#include <vector>

using namespace RSM::Generated;

// User implementation inheriting from generated base class
class LightController : public SimpleLightBase<LightController> {
public:
    std::vector<std::string> eventLog;

    // Action methods
    void onLightOff() {
        eventLog.push_back("Light is OFF");
    }

    void onLightOn() {
        eventLog.push_back("Light is ON");
    }

    void turnOn() {
        eventLog.push_back("Turning on...");
    }

    void turnOff() {
        eventLog.push_back("Turning off...");
    }

    void printLog() {
        for (const auto &msg : eventLog) {
            std::cout << "  - " << msg << std::endl;
        }
        eventLog.clear();
    }

    // Friend declaration for base class access
    friend class SimpleLightBase<LightController>;
};

int main() {
    std::cout << "=== RSM CMake Function Example ===" << std::endl;
    std::cout << std::endl;

    LightController light;

    std::cout << "1. Initializing light (should be OFF)" << std::endl;
    light.initialize();
    light.printLog();
    std::cout << "   Current state: " << (light.getCurrentState() == State::Off ? "OFF" : "ON") << std::endl;
    std::cout << std::endl;

    std::cout << "2. Switching light ON" << std::endl;
    light.processEvent(Event::Switch_on);
    light.printLog();
    std::cout << "   Current state: " << (light.getCurrentState() == State::Off ? "OFF" : "ON") << std::endl;
    std::cout << std::endl;

    std::cout << "3. Switching light OFF" << std::endl;
    light.processEvent(Event::Switch_off);
    light.printLog();
    std::cout << "   Current state: " << (light.getCurrentState() == State::Off ? "OFF" : "ON") << std::endl;
    std::cout << std::endl;

    std::cout << "=== Example Complete ===" << std::endl;
    return 0;
}
