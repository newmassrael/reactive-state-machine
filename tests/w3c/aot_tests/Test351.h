#pragma once
#include "SimpleAotTest.h"
#include "test351_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10.1: Event sendid field validation
 *
 * Tests that _event.sendid is set to the send element's id attribute when present,
 * and blank otherwise. Validates proper event metadata propagation in the AOT engine.
 *
 * Key validation points:
 * - <send id="send1"> sets _event.sendid to "send1"
 * - <send> without id leaves _event.sendid blank
 * - Conditional transitions on sendid values work correctly
 */
struct Test351 : public SimpleAotTest<Test351, 351> {
    static constexpr const char *DESCRIPTION = "_event.sendid field validation (W3C 5.10.1 AOT)";
    using SM = RSM::Generated::test351::test351;
};

// Auto-register
inline static AotTestRegistrar<Test351> registrar_Test351;

}  // namespace RSM::W3C::AotTests
