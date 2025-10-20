#pragma once
#include "SimpleAotTest.h"
#include "test325_sm.h"

namespace RSM::W3C::AotTests {

/**
 * @brief W3C SCXML 5.10: System variable _ioprocessors binding at startup
 *
 * Tests that the _ioprocessors system variable is bound and accessible at
 * initialization time. The test validates that _ioprocessors can be assigned
 * to a datamodel variable and that the variable is defined (typeof check).
 *
 * W3C SCXML 5.10: The '_ioprocessors' system variable is bound to a data
 * structure containing information about available I/O processors. Platforms
 * may implement this as an associative array mapping processor names to
 * processor descriptors.
 */
struct Test325 : public SimpleAotTest<Test325, 325> {
    static constexpr const char *DESCRIPTION = "_ioprocessors binding (W3C 5.10 AOT)";
    using SM = RSM::Generated::test325::test325;
};

// Auto-register
inline static AotTestRegistrar<Test325> registrar_Test325;

}  // namespace RSM::W3C::AotTests
