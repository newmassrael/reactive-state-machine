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
     * @brief Validates assignment location per W3C SCXML 5.3/5.4
     *
     * @param location The location attribute value from <assign> element
     * @return true if location is valid (non-empty), false if invalid (empty)
     *
     * Usage:
     * ```cpp
     * // Interpreter engine (ActionExecutorImpl.cpp)
     * if (!AssignHelper::isValidLocation(location)) {
     *     eventRaiser_->raiseEvent("error.execution", "Assignment location cannot be empty");
     *     return false;
     * }
     *
     * // Static Code Generator (generated code)
     * if (!AssignHelper::isValidLocation("")) {
     *     engine.raiseInternalEvent("error.execution");
     *     return;
     * }
     * ```
     */
    static bool isValidLocation(const std::string &location) {
        return !location.empty();
    }

    /**
     * @brief Returns error message for invalid location (W3C SCXML 5.3/5.4)
     *
     * @return Standard error message for empty/invalid location
     */
    static const char *getInvalidLocationErrorMessage() {
        return "Assignment location cannot be empty";
    }
};

}  // namespace RSM
