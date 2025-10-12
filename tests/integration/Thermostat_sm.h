#pragma once
#include <cstdint>
#include <memory>

namespace RSM::Generated {

enum class State : uint8_t { Cooling, Idle };

enum class Event : uint8_t { Temp_high, Temp_normal };

template <typename Derived> class ThermostatBase {
private:
    State currentState_ = State::Idle;

protected:
    Derived &derived() {
        return static_cast<Derived &>(*this);
    }

    const Derived &derived() const {
        return static_cast<const Derived &>(*this);
    }

public:
    ThermostatBase() = default;

    void initialize() {
        derived().onEnterIdle();
    }

    void processEvent(Event event) {
        switch (currentState_) {
        case State::Cooling:
            if (event == Event::Temp_normal) {
                derived().onExitCooling();
                derived().stopCooling();
                derived().onEnterIdle();
                currentState_ = State::Idle;
            } else if (event == Event::Temp_normal) {
                derived().onExitCooling();
                derived().stopCooling();
                derived().onEnterIdle();
                currentState_ = State::Idle;
            }
            break;
        case State::Idle:
            if (event == Event::Temp_high) {
                if (derived().shouldCool()) {
                    derived().startCooling();
                    derived().onEnterCooling();
                    currentState_ = State::Cooling;
                }
            }
            break;
        }
    }

    State getCurrentState() const {
        return currentState_;
    }
};

}  // namespace RSM::Generated
