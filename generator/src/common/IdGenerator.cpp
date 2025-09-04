#include "common/IdGenerator.h"
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>

namespace SCXML {
namespace Common {

namespace {
std::atomic<uint64_t> globalCounter{0};
}

std::string IdGenerator::generateSessionId(const std::string &prefix) {
    auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    return prefix + "_session_" + std::to_string(now) + "_" + std::to_string(++globalCounter);
}

std::string IdGenerator::generateEventId(const std::string &prefix) {
    return prefix + "_event_" + std::to_string(++globalCounter);
}

std::string IdGenerator::generateUniqueId(const std::string &prefix) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    return prefix + "_" + std::to_string(dis(gen)) + "_" + std::to_string(++globalCounter);
}

}  // namespace Common
}  // namespace SCXML