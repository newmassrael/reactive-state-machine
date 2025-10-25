#pragma once

#include "scripting/JSEngine.h"
#include <future>
#include <string>
#include <vector>

namespace RSM {

/**
 * @brief W3C SCXML 5.10: System Variable Setup Helper
 *
 * Single Source of Truth for system variable initialization logic.
 * ARCHITECTURE.md: Zero Duplication - used by both Interpreter and AOT engines.
 *
 * Usage:
 * - Interpreter: StateMachine initialization (rsm/src/runtime/StateMachine.cpp)
 * - AOT: StaticCodeGenerator jsengine_helpers.jinja2 template (tools/codegen/templates/jsengine_helpers.jinja2)
 *
 * W3C SCXML 5.10 (test 500): System variables (_sessionid, _name, _ioprocessors)
 * must be initialized before datamodel initialization and made read-only.
 *
 * W3C SCXML 6.3.1 (test 500): _ioprocessors['scxml']['location'] field
 * must be accessible for SCXML Event I/O Processor location access.
 */
class SystemVariableHelper {
public:
    /**
     * @brief Setup W3C SCXML 5.10 system variables
     *
     * Initializes the three required system variables as read-only properties:
     * - _sessionid: Unique session identifier
     * - _name: State machine name from <scxml name="...">
     * - _ioprocessors: I/O processor metadata with location fields
     *
     * W3C SCXML 5.10: These variables must be read-only and available
     * before any datamodel initialization occurs.
     *
     * W3C SCXML 6.3.1 (test 500): _ioprocessors['scxml']['location']
     * provides the location field for SCXML Event I/O Processor.
     *
     * @param sessionId Unique session identifier
     * @param sessionName State machine name
     * @param ioProcessors List of I/O processor types (e.g., ["scxml"])
     * @return Future with JSResult indicating success/failure
     */
    static std::future<JSResult> setupSystemVariables(const std::string &sessionId, const std::string &sessionName,
                                                      const std::vector<std::string> &ioProcessors) {
        return JSEngine::instance().setupSystemVariables(sessionId, sessionName, ioProcessors);
    }
};

}  // namespace RSM
