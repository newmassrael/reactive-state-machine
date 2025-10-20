#pragma once
#include "SimpleAotTest.h"
#include "test346_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.9.1: System variable protection validation
 *
 * Tests that attempts to modify read-only system variables (_sessionid, _event,
 * _ioprocessors, _name) raise error.execution events as required by W3C SCXML spec.
 * Uses Static Hybrid approach with JSEngine for system variable protection.
 */
struct Test346 : public SimpleAotTest<Test346, 346> {
    static constexpr const char *DESCRIPTION = "System variable protection (W3C 5.9.1 AOT)";
    using SM = RSM::Generated::test346::test346;
};

// Auto-register
inline static AotTestRegistrar<Test346> registrar_Test346;

}  // namespace RSM::W3C::AotTests
