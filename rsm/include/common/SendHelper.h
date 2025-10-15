#pragma once

#include "common/UniqueIdGenerator.h"
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

    /**
     * @brief Generate unique sendid (Single Source of Truth)
     *
     * Used by both Interpreter and JIT engines to ensure consistent sendid format.
     * Delegates to centralized UniqueIdGenerator for thread-safe, collision-free IDs.
     *
     * W3C SCXML 6.2: Each send action must have a unique sendid for tracking.
     *
     * @return Unique sendid string (format: "send_timestamp_counter")
     */
    static std::string generateSendId() {
        return UniqueIdGenerator::generateSendId();
    }

    /**
     * @brief Store sendid in idlocation variable (Single Source of Truth)
     *
     * W3C SCXML 6.2.4 (test 183): The idlocation attribute specifies a variable
     * where the generated sendid should be stored for later reference.
     *
     * This method encapsulates the idlocation storage logic shared between:
     * - Interpreter engine (ActionExecutorImpl::executeSendAction)
     * - JIT engine (StaticCodeGenerator::generateActionCode for SEND)
     *
     * @param jsEngine JSEngine instance for variable operations
     * @param sessionId Session identifier
     * @param idLocation Variable name to store sendid (empty = no storage)
     * @param sendId Generated sendid value to store
     */
    template <typename JSEngineType>
    static void storeInIdLocation(JSEngineType &jsEngine, const std::string &sessionId, const std::string &idLocation,
                                  const std::string &sendId) {
        if (!idLocation.empty()) {
            jsEngine.setVariable(sessionId, idLocation, sendId);
        }
    }
};

}  // namespace RSM
