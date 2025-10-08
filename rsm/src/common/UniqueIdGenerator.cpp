#include "common/UniqueIdGenerator.h"
#include "common/Logger.h"

#include <iomanip>
#include <sstream>

namespace RSM {

// Static member initialization
std::atomic<uint64_t> UniqueIdGenerator::globalCounter_{0};
std::mt19937_64 UniqueIdGenerator::rng_{std::random_device{}()};
std::mutex UniqueIdGenerator::rngMutex_;

// Statistics counters
std::atomic<uint64_t> UniqueIdGenerator::sessionIdCount_{0};
std::atomic<uint64_t> UniqueIdGenerator::sendIdCount_{0};
std::atomic<uint64_t> UniqueIdGenerator::invokeIdCount_{0};
std::atomic<uint64_t> UniqueIdGenerator::eventIdCount_{0};
std::atomic<uint64_t> UniqueIdGenerator::correlationIdCount_{0};
std::atomic<uint64_t> UniqueIdGenerator::actionIdCount_{0};
std::atomic<uint64_t> UniqueIdGenerator::genericIdCount_{0};

std::string UniqueIdGenerator::generateSessionId(const std::string &prefix) {
    return generateBaseId(prefix, sessionIdCount_);
}

std::string UniqueIdGenerator::generateSendId() {
    return generateBaseId("send", sendIdCount_);
}

std::string UniqueIdGenerator::generateInvokeId(const std::string &stateId) {
    // W3C SCXML 6.4: Invoke ID format MUST be "stateid.platformid" (test 224)
    if (!stateId.empty()) {
        // Increment the specific counter for this ID type
        uint64_t typeCounter = invokeIdCount_.fetch_add(1);

        // Increment global counter for overall uniqueness
        uint64_t globalCount = globalCounter_.fetch_add(1);

        // W3C compliant format: stateid.platformid
        std::ostringstream oss;
        oss << stateId << ".invoke_" << globalCount;

        std::string id = oss.str();
        LOG_DEBUG("UniqueIdGenerator: Generated W3C invoke ID: {} (type counter: {})", id, typeCounter);

        return id;
    }

    // Legacy format for backward compatibility when no state ID provided
    return generateBaseId("invoke", invokeIdCount_);
}

std::string UniqueIdGenerator::generateEventId() {
    return generateBaseId("event", eventIdCount_);
}

std::string UniqueIdGenerator::generateCorrelationId() {
    return generateBaseId("corr", correlationIdCount_);
}

std::string UniqueIdGenerator::generateActionId(const std::string &prefix) {
    return generateBaseId(prefix, actionIdCount_);
}

std::string UniqueIdGenerator::generateUniqueId(const std::string &prefix) {
    return generateBaseId(prefix, genericIdCount_);
}

uint64_t UniqueIdGenerator::generateNumericSessionId() {
    sessionIdCount_.fetch_add(1);
    return globalCounter_.fetch_add(1) + getCurrentTimestamp();
}

bool UniqueIdGenerator::isGeneratedId(const std::string &id) {
    // Check if ID matches our format: prefix_timestamp_counter_random
    if (id.empty()) {
        return false;
    }

    // Count underscores to verify format
    size_t underscoreCount = 0;
    for (char c : id) {
        if (c == '_') {
            underscoreCount++;
        }
    }

    // Our format has exactly 3 underscores: prefix_timestamp_counter_random
    return underscoreCount == 3;
}

void UniqueIdGenerator::resetForTesting() {
    LOG_DEBUG("UniqueIdGenerator: Resetting counters for testing");
    globalCounter_.store(0);
    sessionIdCount_.store(0);
    sendIdCount_.store(0);
    invokeIdCount_.store(0);
    eventIdCount_.store(0);
    correlationIdCount_.store(0);
    actionIdCount_.store(0);
    genericIdCount_.store(0);

    // Reset RNG to deterministic state for testing
    std::lock_guard<std::mutex> lock(rngMutex_);
    rng_.seed(12345);  // Fixed seed for reproducible testing
}

std::string UniqueIdGenerator::getStatistics() {
    std::ostringstream stats;
    stats << "UniqueIdGenerator Statistics:\n";
    stats << "  Session IDs: " << sessionIdCount_.load() << "\n";
    stats << "  Send IDs: " << sendIdCount_.load() << "\n";
    stats << "  Invoke IDs: " << invokeIdCount_.load() << "\n";
    stats << "  Event IDs: " << eventIdCount_.load() << "\n";
    stats << "  Correlation IDs: " << correlationIdCount_.load() << "\n";
    stats << "  Action IDs: " << actionIdCount_.load() << "\n";
    stats << "  Generic IDs: " << genericIdCount_.load() << "\n";
    stats << "  Total IDs: "
          << (sessionIdCount_.load() + sendIdCount_.load() + invokeIdCount_.load() + eventIdCount_.load() +
              correlationIdCount_.load() + actionIdCount_.load() + genericIdCount_.load());
    return stats.str();
}

std::string UniqueIdGenerator::generateBaseId(const std::string &prefix, std::atomic<uint64_t> &counterRef) {
    // Increment the specific counter for this ID type
    uint64_t typeCounter = counterRef.fetch_add(1);

    // Increment global counter for overall uniqueness
    uint64_t globalCount = globalCounter_.fetch_add(1);

    // Get current timestamp
    uint64_t timestamp = getCurrentTimestamp();

    // Get random component for additional uniqueness
    uint64_t randomComponent = getRandomComponent();

    // Construct ID: prefix_timestamp_counter_random
    std::ostringstream oss;
    oss << prefix << "_" << timestamp << "_" << globalCount << "_" << std::hex << randomComponent;

    std::string id = oss.str();
    LOG_DEBUG("UniqueIdGenerator: Generated ID: {} (type counter: {})", id, typeCounter);

    return id;
}

uint64_t UniqueIdGenerator::getCurrentTimestamp() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

uint64_t UniqueIdGenerator::getRandomComponent() {
    std::lock_guard<std::mutex> lock(rngMutex_);
    // Use only lower 16 bits to keep ID length reasonable
    return rng_() & 0xFFFF;
}

}  // namespace RSM