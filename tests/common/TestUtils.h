#pragma once

#include <cstdlib>
#include <string>

namespace RSM {
namespace Test {
namespace Utils {

/**
 * @brief Check if running in Docker TSAN environment
 *
 * Checks the IN_DOCKER_TSAN environment variable to determine if HTTP tests
 * should be skipped due to cpp-httplib/SimpleMockHttpServer thread creation
 * incompatibility with TSAN.
 *
 * @return true if IN_DOCKER_TSAN is set to a truthy value (non-empty, not "0", not "false")
 */
inline bool isInDockerTsan() {
    const char *env = std::getenv("IN_DOCKER_TSAN");
    if (!env) {
        return false;
    }

    std::string value(env);
    // Treat empty string, "0", and "false" as false
    return !value.empty() && value != "0" && value != "false";
}

/**
 * @brief Get base delay for timing-sensitive tests
 *
 * Returns a base delay value (in milliseconds) that accounts for TSAN overhead.
 * In TSAN environments, scheduling and synchronization operations are slower,
 * so tests need longer delays to avoid flaky behavior.
 *
 * @param normalDelay Delay to use in normal (non-TSAN) environments
 * @return Delay value adjusted for TSAN environment if applicable
 */
inline int getBaseDelay(int normalDelay = 50) {
    // TSAN environments need 4x longer delays due to instrumentation overhead
    return isInDockerTsan() ? (normalDelay * 4) : normalDelay;
}

}  // namespace Utils
}  // namespace Test
}  // namespace RSM
