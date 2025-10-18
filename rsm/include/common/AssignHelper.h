#pragma once

#include <string>

namespace RSM {

/**
 * @brief Single Source of Truth for assignment location validation (W3C SCXML 5.3, 5.4)
 *
 * Shared by Interpreter engine and Static Code Generator to ensure Zero Duplication.
 *
 * W3C SCXML 5.3: "If the location expression does not denote a valid location in the
 * data model... the processor must place the error 'error.execution' in the internal
 * event queue."
 *
 * W3C SCXML 5.4: "If the location expression does not denote a valid location in the
 * data model... the SCXML Processor must place the error 'error.execution' on the
 * internal event queue."
 */
class AssignHelper {
public:
    /**
     * @brief Validates assignment location per W3C SCXML 5.3/5.4 and B.2
     *
     * @param location The location attribute value from <assign> element
     * @return true if location is valid and writable, false if invalid or read-only
     *
     * W3C SCXML B.2: System variables (_sessionid, _event, _name, _ioprocessors)
     * are read-only and cannot be assigned to. Attempting to assign triggers error.execution.
     *
     * Usage:
     * ```cpp
     * // Interpreter engine (ActionExecutorImpl.cpp)
     * if (!AssignHelper::isValidLocation(location)) {
     *     eventRaiser_->raiseEvent("error.execution", "Invalid or read-only location");
     *     return false;
     * }
     *
     * // Static Code Generator (generated code)
     * if (!AssignHelper::isValidLocation("_sessionid")) {
     *     engine.raise(Event::Error_execution);
     *     break;
     * }
     * ```
     */
    static bool isValidLocation(const std::string &location) {
        // W3C SCXML 5.3/5.4: Empty location is invalid
        if (location.empty()) {
            return false;
        }

        // W3C SCXML B.2: System variables are read-only (cannot be assigned)
        if (location == "_sessionid" || location == "_event" || location == "_name" || location == "_ioprocessors") {
            return false;
        }

        return true;
    }

    /**
     * @brief Returns error message for invalid location (W3C SCXML 5.3/5.4, B.2)
     *
     * @param location The location attribute value from <assign> element
     * @return Descriptive error message for empty location or system variable violation
     */
    static std::string getInvalidLocationErrorMessage(const std::string &location) {
        if (location.empty()) {
            return "Assignment location cannot be empty";
        }
        if (location == "_sessionid" || location == "_event" || location == "_name" || location == "_ioprocessors") {
            return "Cannot assign to read-only system variable: " + location;
        }
        return "Invalid assignment location: " + location;
    }
};

}  // namespace RSM
