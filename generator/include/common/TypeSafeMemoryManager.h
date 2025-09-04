#ifndef TYPESAFEMEMORYMANAGER_H
#define TYPESAFEMEMORYMANAGER_H

#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

namespace SCXML {
namespace Common {

/**
 * @brief Type-safe memory management with leak detection
 */
class TypeSafeMemoryManager {
public:
    /**
     * @brief Allocation information with type safety
     */
    template <typename T> struct TypedAllocationInfo {
        std::unique_ptr<T> pointer;
        size_t size;
        std::string type;
        std::string location;
        std::chrono::steady_clock::time_point allocTime;

        TypedAllocationInfo(std::unique_ptr<T> ptr, size_t sz, const std::string &tp, const std::string &loc)
            : pointer(std::move(ptr)), size(sz), type(tp), location(loc), allocTime(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Memory statistics
     */
    struct MemoryStats {
        size_t totalAllocations = 0;
        size_t totalDeallocations = 0;
        size_t activeAllocations = 0;
        size_t totalBytesAllocated = 0;
        size_t leakCount = 0;
        size_t peakMemoryUsage = 0;
    };

    /**
     * @brief Create typed memory allocation
     */
    template <typename T, typename... Args> std::shared_ptr<T> makeShared(const std::string &location, Args &&...args) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto ptr = std::make_shared<T>(std::forward<Args>(args)...);

        stats_.totalAllocations++;
        stats_.activeAllocations++;
        stats_.totalBytesAllocated += sizeof(T);

        if (stats_.totalBytesAllocated > stats_.peakMemoryUsage) {
            stats_.peakMemoryUsage = stats_.totalBytesAllocated;
        }

        // Store weak reference for leak detection
        std::weak_ptr<T> weakPtr = ptr;
        activePointers_[reinterpret_cast<uintptr_t>(ptr.get())] = {sizeof(T), typeid(T).name(), location,
                                                                   std::chrono::steady_clock::now()};

        return ptr;
    }

    /**
     * @brief Create typed unique pointer
     */
    template <typename T, typename... Args> std::unique_ptr<T> makeUnique(const std::string &location, Args &&...args) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T *rawPtr = ptr.get();

        stats_.totalAllocations++;
        stats_.activeAllocations++;
        stats_.totalBytesAllocated += sizeof(T);

        if (stats_.totalBytesAllocated > stats_.peakMemoryUsage) {
            stats_.peakMemoryUsage = stats_.totalBytesAllocated;
        }

        activePointers_[reinterpret_cast<uintptr_t>(rawPtr)] = {sizeof(T), typeid(T).name(), location,
                                                                std::chrono::steady_clock::now()};

        return ptr;
    }

    /**
     * @brief Record deallocation
     */
    template <typename T> void recordDeallocation(T *ptr) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = activePointers_.find(reinterpret_cast<uintptr_t>(ptr));
        if (it != activePointers_.end()) {
            stats_.totalDeallocations++;
            stats_.activeAllocations--;
            stats_.totalBytesAllocated -= it->second.size;
            activePointers_.erase(it);
        }
    }

    /**
     * @brief Get memory statistics
     */
    MemoryStats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    /**
     * @brief Detect memory leaks
     */
    std::vector<std::string> detectLeaks() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> leaks;
        for (const auto &[addr, info] : activePointers_) {
            auto duration = std::chrono::steady_clock::now() - info.allocTime;
            auto hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();

            if (hours > 1) {  // Consider as leak if alive for more than 1 hour
                leaks.push_back(info.type + " at " + info.location + " (size: " + std::to_string(info.size) +
                                ", age: " + std::to_string(hours) + "h)");
            }
        }

        return leaks;
    }

    /**
     * @brief Get singleton instance
     */
    static TypeSafeMemoryManager &getInstance() {
        static TypeSafeMemoryManager instance;
        return instance;
    }

private:
    struct AllocationInfo {
        size_t size;
        std::string type;
        std::string location;
        std::chrono::steady_clock::time_point allocTime;
    };

    mutable std::mutex mutex_;
    MemoryStats stats_;
    std::unordered_map<uintptr_t, AllocationInfo> activePointers_;

    TypeSafeMemoryManager() = default;
    ~TypeSafeMemoryManager() = default;
    TypeSafeMemoryManager(const TypeSafeMemoryManager &) = delete;
    TypeSafeMemoryManager &operator=(const TypeSafeMemoryManager &) = delete;
};

// Helper macros for convenient usage
#define SCXML_MAKE_SHARED(T, ...)                                                                                      \
    SCXML::Common::TypeSafeMemoryManager::getInstance().makeShared<T>(__FILE__ ":" + std::to_string(__LINE__),         \
                                                                      __VA_ARGS__)

#define SCXML_MAKE_UNIQUE(T, ...)                                                                                      \
    SCXML::Common::TypeSafeMemoryManager::getInstance().makeUnique<T>(__FILE__ ":" + std::to_string(__LINE__),         \
                                                                      __VA_ARGS__)

}  // namespace Common

}  // namespace SCXML

#endif  // TYPESAFEMEMORYMANAGER_H