#include "common/TypeRegistry.h"
#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

#include "common/TestUtils.h"

namespace RSM {

class TypeRegistryThreadSafetyTest : public ::testing::Test {
protected:
    static constexpr int NUM_THREADS = 10;
    static constexpr int OPERATIONS_PER_THREAD = 1000;

    void SetUp() override {
        TypeRegistry::getInstance().clear();
    }

    void TearDown() override {
        TypeRegistry::getInstance().clear();
    }
};

TEST_F(TypeRegistryThreadSafetyTest, StressTestConcurrentOperations) {
    TypeRegistry &registry = TypeRegistry::getInstance();

    std::atomic<bool> startFlag{false};
    std::vector<std::future<std::pair<int, int>>> futures;

    // Launch threads that perform mixed operations
    for (int i = 0; i < NUM_THREADS; ++i) {
        auto future = std::async(std::launch::async, [&registry, &startFlag, i]() -> std::pair<int, int> {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> opDist(0, 3);   // 4 types of operations
            std::uniform_int_distribution<> catDist(0, 3);  // 4 categories

            std::vector<TypeRegistry::Category> categories = {
                TypeRegistry::Category::EVENT_PROCESSOR, TypeRegistry::Category::INVOKE_PROCESSOR,
                TypeRegistry::Category::DATA_MODEL, TypeRegistry::Category::CONTENT_TYPE};

            int successCount = 0;
            int failureCount = 0;

            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                int operation = opDist(gen);
                int categoryIdx = catDist(gen);
                TypeRegistry::Category category = categories[categoryIdx];

                try {
                    switch (operation) {
                    case 0: {  // Register new type
                        std::string uri = "stress_thread" + std::to_string(i) + "_op" + std::to_string(j);
                        std::string canonical = "canonical_" + std::to_string(i) + "_" + std::to_string(j);
                        if (registry.registerType(category, uri, canonical)) {
                            successCount++;
                        } else {
                            failureCount++;
                        }
                        break;
                    }
                    case 1: {  // Check if type is registered
                        std::string uri =
                            "stress_thread" + std::to_string((i + 1) % NUM_THREADS) + "_op" + std::to_string(j / 2);
                        registry.isRegisteredType(category, uri);
                        successCount++;
                        break;
                    }
                    case 2: {  // Get canonical name
                        std::string uri =
                            "stress_thread" + std::to_string((i + 2) % NUM_THREADS) + "_op" + std::to_string(j / 3);
                        registry.getCanonicalName(category, uri);
                        successCount++;
                        break;
                    }
                    case 3: {  // Get all registered types
                        registry.getRegisteredTypes(category);
                        successCount++;
                        break;
                    }
                    }
                } catch (const std::exception &e) {
                    failureCount++;
                }
            }

            return {successCount, failureCount};
        });

        futures.push_back(std::move(future));
    }

    // Start all threads simultaneously
    startFlag = true;

    // Collect results
    int totalSuccess = 0;
    int totalFailure = 0;

    for (auto &future : futures) {
        auto result = future.get();
        totalSuccess += result.first;
        totalFailure += result.second;
    }

    // Most operations should succeed (some conflicts are expected)
    EXPECT_GT(totalSuccess, NUM_THREADS * OPERATIONS_PER_THREAD * 0.95);
    EXPECT_LT(totalFailure, NUM_THREADS * OPERATIONS_PER_THREAD * 0.05);
}

TEST_F(TypeRegistryThreadSafetyTest, MassiveReaderWriterContentionTest) {
    TypeRegistry &registry = TypeRegistry::getInstance();

    constexpr int NUM_WRITERS = 3;
    constexpr int NUM_READERS = 30;
    constexpr int DURATION_SECONDS = 2;

    std::atomic<bool> stopFlag{false};
    std::atomic<int> writeCount{0};
    std::atomic<int> readCount{0};
    std::atomic<int> errorCount{0};

    std::vector<std::thread> writers;
    std::vector<std::thread> readers;

    // Writer threads - continuously register new types
    for (int i = 0; i < NUM_WRITERS; ++i) {
        writers.emplace_back([&registry, &stopFlag, &writeCount, &errorCount, i]() {
            int opCount = 0;
            while (!stopFlag.load()) {
                try {
                    std::string uri = "massive_writer" + std::to_string(i) + "_" + std::to_string(opCount);
                    std::string canonical = "massive_canonical" + std::to_string(i) + "_" + std::to_string(opCount);

                    if (registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, uri, canonical)) {
                        writeCount++;
                    } else {
                        errorCount++;
                    }

                    opCount++;
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                } catch (const std::exception &e) {
                    errorCount++;
                }
            }
        });
    }

    // Reader threads - continuously read types
    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back([&registry, &stopFlag, &readCount, &errorCount]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> writerDist(0, NUM_WRITERS - 1);
            std::uniform_int_distribution<> opDist(0, 99);

            while (!stopFlag.load()) {
                try {
                    int writerId = writerDist(gen);
                    int opId = opDist(gen);
                    std::string uri = "massive_writer" + std::to_string(writerId) + "_" + std::to_string(opId);

                    // Perform different read operations
                    registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, uri);
                    registry.getCanonicalName(TypeRegistry::Category::EVENT_PROCESSOR, uri);
                    registry.getRegisteredTypes(TypeRegistry::Category::EVENT_PROCESSOR);

                    readCount++;
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                } catch (const std::exception &e) {
                    errorCount++;
                }
            }
        });
    }

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SECONDS));
    stopFlag = true;

    // Wait for all threads to finish
    for (auto &writer : writers) {
        writer.join();
    }
    for (auto &reader : readers) {
        reader.join();
    }

    // Verify significant activity with minimal errors
    // Note: Heavy read contention (30 readers vs 3 writers) significantly reduces write throughput
    // Measured baseline: 33-93 writes in 2 seconds (avg ~50)
    EXPECT_GT(writeCount.load(), 20);  // Should have some writes despite contention
    EXPECT_GT(readCount.load(), 500);  // Should have many reads
    EXPECT_LT(errorCount.load(), (writeCount.load() + readCount.load()) * 0.01);  // Less than 1% errors
}

TEST_F(TypeRegistryThreadSafetyTest, DataCorruptionDetectionTest) {
    TypeRegistry &registry = TypeRegistry::getInstance();

    // Pre-register some baseline types
    registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, "baseline1", "baseline1");
    registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, "baseline2", "baseline2");
    registry.registerType(TypeRegistry::Category::DATA_MODEL, "baseline3", "baseline3");

    constexpr int NUM_CORRUPTOR_THREADS = 5;
    constexpr int NUM_VALIDATOR_THREADS = 5;
    constexpr int OPERATIONS_COUNT = 500;

    std::atomic<bool> startFlag{false};
    std::atomic<int> corruptionDetected{0};

    std::vector<std::thread> corruptors;
    std::vector<std::thread> validators;

    // Threads that might cause data corruption through concurrent access
    for (int i = 0; i < NUM_CORRUPTOR_THREADS; ++i) {
        corruptors.emplace_back([&registry, &startFlag, i]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }

            for (int j = 0; j < OPERATIONS_COUNT; ++j) {
                // Rapid registration and access
                std::string uri = "corrupt_test" + std::to_string(i) + "_" + std::to_string(j);
                registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, uri, "corrupt_canonical");

                // Immediate access
                registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, uri);
                registry.getCanonicalName(TypeRegistry::Category::EVENT_PROCESSOR, uri);
            }
        });
    }

    // Threads that validate data integrity
    for (int i = 0; i < NUM_VALIDATOR_THREADS; ++i) {
        validators.emplace_back([&registry, &startFlag, &corruptionDetected]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }

            for (int j = 0; j < OPERATIONS_COUNT; ++j) {
                // Check baseline types are still consistent
                bool exists1 = registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, "baseline1");
                std::string canonical1 =
                    registry.getCanonicalName(TypeRegistry::Category::EVENT_PROCESSOR, "baseline1");

                bool exists2 = registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, "baseline2");
                std::string canonical2 =
                    registry.getCanonicalName(TypeRegistry::Category::EVENT_PROCESSOR, "baseline2");

                bool exists3 = registry.isRegisteredType(TypeRegistry::Category::DATA_MODEL, "baseline3");
                std::string canonical3 = registry.getCanonicalName(TypeRegistry::Category::DATA_MODEL, "baseline3");

                // Detect inconsistencies
                if ((exists1 && canonical1 != "baseline1") || (exists2 && canonical2 != "baseline2") ||
                    (exists3 && canonical3 != "baseline3") || (!exists1 || !exists2 || !exists3)) {
                    corruptionDetected++;
                }

                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Start all threads
    startFlag = true;

    for (auto &corruptor : corruptors) {
        corruptor.join();
    }
    for (auto &validator : validators) {
        validator.join();
    }

    // No data corruption should be detected
    EXPECT_EQ(0, corruptionDetected.load()) << "Data corruption detected in concurrent access scenarios";
}

TEST_F(TypeRegistryThreadSafetyTest, DeadlockDetectionTest) {
    TypeRegistry &registry = TypeRegistry::getInstance();

    constexpr int NUM_DEADLOCK_THREADS = 10;
    constexpr int MAX_WAIT_SECONDS = 5;

    std::atomic<bool> startFlag{false};
    std::atomic<int> completedThreads{0};

    std::vector<std::future<bool>> futures;

    // Create threads that could potentially deadlock
    for (int i = 0; i < NUM_DEADLOCK_THREADS; ++i) {
        auto future = std::async(std::launch::async, [&registry, &startFlag, &completedThreads, i]() -> bool {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }

            try {
                // Complex operations that could cause deadlock
                for (int j = 0; j < 100; ++j) {
                    std::string uri1 = "deadlock_test" + std::to_string(i) + "_" + std::to_string(j);
                    std::string uri2 =
                        "deadlock_test" + std::to_string((i + 1) % NUM_DEADLOCK_THREADS) + "_" + std::to_string(j);

                    // Interleaved operations on different URIs
                    registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, uri1, "canonical1");
                    registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, uri2);
                    registry.getCanonicalName(TypeRegistry::Category::EVENT_PROCESSOR, uri1);
                    registry.registerType(TypeRegistry::Category::DATA_MODEL, uri2, "canonical2");

                    auto allTypes = registry.getRegisteredTypes(TypeRegistry::Category::EVENT_PROCESSOR);
                }

                completedThreads++;
                return true;
            } catch (const std::exception &e) {
                return false;
            }
        });

        futures.push_back(std::move(future));
    }

    // Start all threads
    startFlag = true;

    // Wait for completion with timeout
    auto startTime = std::chrono::steady_clock::now();
    bool allCompleted = false;

    while (!allCompleted && std::chrono::steady_clock::now() - startTime < std::chrono::seconds(MAX_WAIT_SECONDS)) {
        std::this_thread::sleep_for(RSM::Test::Utils::STANDARD_WAIT_MS);
        allCompleted = (completedThreads.load() == NUM_DEADLOCK_THREADS);
    }

    // Check if threads completed without deadlock
    EXPECT_TRUE(allCompleted) << "Potential deadlock detected - not all threads completed within timeout";
    EXPECT_EQ(NUM_DEADLOCK_THREADS, completedThreads.load());

    // Ensure all futures complete
    for (auto &future : futures) {
        if (future.wait_for(RSM::Test::Utils::STANDARD_WAIT_MS) == std::future_status::ready) {
            EXPECT_TRUE(future.get()) << "Thread completed with error";
        }
    }
}

}  // namespace RSM