#include "traffic_light_sm.h"
#include "wrappers/AutoProcessStateMachine.h"
#include <iostream>

int main() {
    using namespace RSM::Generated::traffic_light;

    std::cout << "=== Traffic Light Example ===" << "\n\n";

    // Option 1: Easy API - Auto-processing wrapper (recommended for beginners)
    std::cout << "Using easy API (AutoProcessStateMachine):" << "\n";
    {
        RSM::Wrappers::AutoProcessStateMachine<traffic_light> light;

        light.initialize();
        std::cout << "  Initial: " << (light.getCurrentState() == State::Red ? "Red" : "Other") << "\n";

        light.processEvent(Event::Timer);  // Auto-processes!
        std::cout << "  After timer: " << (light.getCurrentState() == State::Green ? "Green" : "Other") << "\n";

        light.processEvent(Event::Timer);
        std::cout << "  After timer: " << (light.getCurrentState() == State::Yellow ? "Yellow" : "Other") << "\n";

        light.processEvent(Event::Timer);
        std::cout << "  After timer: " << (light.getCurrentState() == State::Red ? "Red" : "Other") << "\n";
    }

    std::cout << "\n";

    // Option 2: Low-level API - Manual control (for advanced users)
    std::cout << "Using low-level API (manual step):" << "\n";
    {
        traffic_light light;

        light.initialize();
        std::cout << "  Initial: " << (light.getCurrentState() == State::Red ? "Red" : "Other") << "\n";

        light.raiseExternal(Event::Timer);
        light.step();  // Explicit queue processing
        std::cout << "  After timer: " << (light.getCurrentState() == State::Green ? "Green" : "Other") << "\n";

        light.raiseExternal(Event::Timer);
        light.step();
        std::cout << "  After timer: " << (light.getCurrentState() == State::Yellow ? "Yellow" : "Other") << "\n";

        light.raiseExternal(Event::Timer);
        light.step();
        std::cout << "  After timer: " << (light.getCurrentState() == State::Red ? "Red" : "Other") << "\n";
    }

    return 0;
}
