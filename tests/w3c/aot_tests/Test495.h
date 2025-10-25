#pragma once
#include "SimpleAotTest.h"
#include "test495_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 6.2: SCXML Event I/O Processor internal vs external queue handling
 *
 * Tests that the SCXML event I/O processor correctly routes events to internal and external queues.
 * Events sent to target="#_internal" should be placed in the internal queue and processed first,
 * while events without a target attribute should go to the external queue and be processed after
 * internal events are exhausted.
 *
 * Expected behavior:
 * - Events with target="#_internal" are routed to internal queue
 * - Events without target attribute go to external queue
 * - Internal queue events are processed before external queue events
 * - Event processing order follows W3C SCXML 6.2 queue priority rules
 *
 * Test validates correct queue routing by verifying that internal events
 * are processed in the correct order relative to external events, ensuring
 * compliance with W3C SCXML Event I/O Processor specifications.
 */
struct Test495 : public SimpleAotTest<Test495, 495> {
    static constexpr const char *DESCRIPTION = "SCXML Event I/O Processor queue handling (W3C 6.2 AOT)";
    using SM = RSM::Generated::test495::test495;
};

// Auto-register
inline static AotTestRegistrar<Test495> registrar_Test495;

}  // namespace RSM::W3C::AotTests
