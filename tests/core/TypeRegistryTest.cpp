#include "common/TypeRegistry.h"
#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

namespace RSM {

class TypeRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry before each test
        TypeRegistry::getInstance().clear();
    }

    void TearDown() override {
        TypeRegistry::getInstance().clear();
    }
};

// Basic functionality tests
TEST_F(TypeRegistryTest, BasicRegistration) {
    TypeRegistry &registry = TypeRegistry::getInstance();

    EXPECT_TRUE(registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, "test", "test-canonical"));
    EXPECT_TRUE(registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, "test"));
    EXPECT_EQ("test-canonical", registry.getCanonicalName(TypeRegistry::Category::EVENT_PROCESSOR, "test"));
}

TEST_F(TypeRegistryTest, URINormalization) {
    TypeRegistry &registry = TypeRegistry::getInstance();

    // Test URI normalization consistency
    EXPECT_TRUE(registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, "HTTP", "http"));
    EXPECT_TRUE(registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, "http"));
    EXPECT_TRUE(registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, "HTTP"));
    EXPECT_TRUE(registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, " http "));
    EXPECT_TRUE(registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, "http/"));
}

// Thread safety tests - these will initially fail
TEST_F(TypeRegistryTest, ConcurrentRegistration) {
    TypeRegistry &registry = TypeRegistry::getInstance();
    constexpr int NUM_THREADS = 10;
    constexpr int REGISTRATIONS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> failureCount{0};

    // Each thread registers different types concurrently
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&registry, &successCount, &failureCount, i]() {
            for (int j = 0; j < REGISTRATIONS_PER_THREAD; ++j) {
                std::string uri = "thread" + std::to_string(i) + "_type" + std::to_string(j);
                std::string canonical = "canonical" + std::to_string(i) + "_" + std::to_string(j);

                if (registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, uri, canonical)) {
                    successCount++;
                } else {
                    failureCount++;
                }
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    // All registrations should succeed since they're unique
    EXPECT_EQ(NUM_THREADS * REGISTRATIONS_PER_THREAD, successCount.load());
    EXPECT_EQ(0, failureCount.load());

    // Verify all registered types are accessible
    for (int i = 0; i < NUM_THREADS; ++i) {
        for (int j = 0; j < REGISTRATIONS_PER_THREAD; ++j) {
            std::string uri = "thread" + std::to_string(i) + "_type" + std::to_string(j);
            EXPECT_TRUE(registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, uri))
                << "Failed to find registered type: " << uri;
        }
    }
}

TEST_F(TypeRegistryTest, ConcurrentReadAccess) {
    TypeRegistry &registry = TypeRegistry::getInstance();

    // Pre-register some types
    registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, "http", "basic-http");
    registry.registerType(TypeRegistry::Category::EVENT_PROCESSOR, "scxml", "scxml");
    registry.registerType(TypeRegistry::Category::DATA_MODEL, "ecmascript", "ecmascript");

    constexpr int NUM_READERS = 20;
    constexpr int READS_PER_THREAD = 1000;

    std::vector<std::thread> readers;
    std::atomic<int> readSuccessCount{0};
    std::atomic<int> readFailureCount{0};

    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back([&registry, &readSuccessCount, &readFailureCount]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, 2);

            std::vector<std::string> testUris = {"http", "scxml", "ecmascript"};
            std::vector<TypeRegistry::Category> testCategories = {TypeRegistry::Category::EVENT_PROCESSOR,
                                                                  TypeRegistry::Category::EVENT_PROCESSOR,
                                                                  TypeRegistry::Category::DATA_MODEL};

            for (int j = 0; j < READS_PER_THREAD; ++j) {
                int idx = dist(gen);
                bool found = registry.isRegisteredType(testCategories[idx], testUris[idx]);
                std::string canonical = registry.getCanonicalName(testCategories[idx], testUris[idx]);

                if (found && !canonical.empty()) {
                    readSuccessCount++;
                } else {
                    readFailureCount++;
                }
            }
        });
    }

    for (auto &reader : readers) {
        reader.join();
    }

    // All reads should succeed
    EXPECT_EQ(NUM_READERS * READS_PER_THREAD, readSuccessCount.load());
    EXPECT_EQ(0, readFailureCount.load());
}

TEST_F(TypeRegistryTest, ReaderWriterContention) {
    TypeRegistry &registry = TypeRegistry::getInstance();

    constexpr int NUM_WRITERS = 5;
    constexpr int NUM_READERS = 15;
    constexpr int OPERATIONS_PER_THREAD = 100;

    std::atomic<bool> startFlag{false};
    std::atomic<int> writerSuccessCount{0};
    std::atomic<int> readerSuccessCount{0};

    std::vector<std::thread> writers;
    std::vector<std::thread> readers;

    // Writer threads
    for (int i = 0; i < NUM_WRITERS; ++i) {
        writers.emplace_back([&registry, &startFlag, &writerSuccessCount, i]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }

            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                std::string uri = "writer" + std::to_string(i) + "_op" + std::to_string(j);
                std::string canonical = "canonical_" + std::to_string(i) + "_" + std::to_string(j);

                if (registry.registerType(TypeRegistry::Category::CONTENT_TYPE, uri, canonical)) {
                    writerSuccessCount++;
                }

                // Small delay to increase contention
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    // Reader threads
    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back([&registry, &startFlag, &readerSuccessCount]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }

            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                // Try to read from different categories
                auto types = registry.getRegisteredTypes(TypeRegistry::Category::CONTENT_TYPE);
                if (!types.empty()) {
                    readerSuccessCount++;
                }

                // Small delay to increase contention
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    // Start all threads simultaneously
    startFlag = true;

    for (auto &writer : writers) {
        writer.join();
    }
    for (auto &reader : readers) {
        reader.join();
    }

    // Verify writers succeeded
    EXPECT_EQ(NUM_WRITERS * OPERATIONS_PER_THREAD, writerSuccessCount.load());

    // Readers should have mostly succeeded (some might fail if read during initialization)
    EXPECT_GT(readerSuccessCount.load(), NUM_READERS * OPERATIONS_PER_THREAD * 0.8);
}

TEST_F(TypeRegistryTest, RealWorldActionExecutorScenario) {
    TypeRegistry &registry = TypeRegistry::getInstance();

    // Simulate ActionExecutor scenario where multiple threads validate send types
    constexpr int NUM_ACTION_THREADS = 8;
    constexpr int ACTIONS_PER_THREAD = 500;

    std::vector<std::string> sendTypes = {"http",
                                          "https",
                                          "scxml",
                                          "basichttp",
                                          "internal",
                                          "http://www.w3.org/TR/scxml/#SCXMLEventProcessor",
                                          "http://www.w3.org/TR/scxml/#BasicHTTPEventProcessor"};

    std::vector<std::thread> actionThreads;
    std::atomic<int> validationSuccessCount{0};
    std::atomic<int> validationFailureCount{0};

    for (int i = 0; i < NUM_ACTION_THREADS; ++i) {
        actionThreads.emplace_back([&registry, &sendTypes, &validationSuccessCount, &validationFailureCount]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, sendTypes.size() - 1);

            for (int j = 0; j < ACTIONS_PER_THREAD; ++j) {
                std::string sendType = sendTypes[dist(gen)];

                // Simulate ActionExecutor validation logic
                bool isRegistered = registry.isRegisteredType(TypeRegistry::Category::EVENT_PROCESSOR, sendType);
                if (isRegistered) {
                    std::string canonical =
                        registry.getCanonicalName(TypeRegistry::Category::EVENT_PROCESSOR, sendType);
                    if (!canonical.empty()) {
                        validationSuccessCount++;
                    } else {
                        validationFailureCount++;
                    }
                } else {
                    // For ActionExecutor, unregistered types are handled gracefully
                    validationSuccessCount++;
                }
            }
        });
    }

    for (auto &thread : actionThreads) {
        thread.join();
    }

    // All validations should succeed (either registered or handled gracefully)
    EXPECT_EQ(NUM_ACTION_THREADS * ACTIONS_PER_THREAD, validationSuccessCount.load());
    EXPECT_EQ(0, validationFailureCount.load());
}

}  // namespace RSM