#pragma once

#include <string>

namespace RSM {

/**
 * @brief Helper functions for W3C SCXML <send> element processing
 *
 * Single Source of Truth for send action validation logic shared between:
 * - Interpreter engine (ActionExecutorImpl)
 * - JIT engine (StaticCodeGenerator)
 *
 * W3C SCXML References:
 * - 6.2: Send element semantics
 * - 5.10: Error handling for send
 */
class SendHelper {
public:
    /**
     * @brief Check if target validation failed (for error.execution detection)
     *
     * Single Source of Truth for target validation logic.
     * Used by JIT engine to determine if error.execution should be raised,
     * which stops execution of subsequent executable content per W3C SCXML 5.10.
     *
     * W3C SCXML 6.2: Target values starting with "!" are invalid.
     *
     * @param target Target to check
     * @return true if target is invalid (starts with '!')
     */
    static bool isInvalidTarget(const std::string &target) {
        // W3C SCXML 6.2: Target values starting with "!" are invalid
        return !target.empty() && target[0] == '!';
    }

    /**
     * @brief Validate send target according to W3C SCXML 6.2
     *
     * W3C SCXML 6.2 (tests 159, 194): Invalid target values (e.g., starting with "!")
     * must raise error.execution and stop subsequent executable content.
     *
     * This function reuses isInvalidTarget() to avoid code duplication.
     *
     * @param target Target string to validate
     * @param errorMsg Output parameter for error message if validation fails
     * @return true if valid, false if invalid (error.execution should be raised)
     */
    static bool validateTarget(const std::string &target, std::string &errorMsg) {
        if (isInvalidTarget(target)) {
            errorMsg = "Invalid target value: " + target;
            return false;
        }
        return true;
    }
};

}  // namespace RSM
