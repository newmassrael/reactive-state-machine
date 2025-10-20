#pragma once

#include "AotTestBase.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace RSM::W3C::AotTests {

/**
 * @brief Singleton registry for AOT tests
 *
 * Manages automatic registration and creation of AOT test instances.
 * Tests register themselves via REGISTER_AOT_TEST macro during static initialization.
 */
class AotTestRegistry {
public:
    using TestFactory = std::function<std::unique_ptr<AotTestBase>()>;

    /**
     * @brief Get singleton instance
     */
    static AotTestRegistry &instance() {
        static AotTestRegistry registry;
        return registry;
    }

    /**
     * @brief Register a test factory
     * @param testId Test number
     * @param factory Function that creates test instance
     */
    void registerTest(int testId, TestFactory factory) {
        tests_[testId] = std::move(factory);
    }

    /**
     * @brief Create test instance by ID
     * @param testId Test number
     * @return Unique pointer to test instance, or nullptr if not found
     */
    std::unique_ptr<AotTestBase> createTest(int testId) const {
        auto it = tests_.find(testId);
        return it != tests_.end() ? it->second() : nullptr;
    }

    /**
     * @brief Check if test is registered
     * @param testId Test number
     * @return true if test exists in registry
     */
    bool hasTest(int testId) const {
        return tests_.find(testId) != tests_.end();
    }

    /**
     * @brief Get all registered test IDs
     * @return Vector of test IDs in ascending order
     */
    std::vector<int> getAllTestIds() const {
        std::vector<int> ids;
        ids.reserve(tests_.size());
        for (const auto &[id, _] : tests_) {
            ids.push_back(id);
        }
        return ids;
    }

private:
    AotTestRegistry() = default;
    std::map<int, TestFactory> tests_;
};

/**
 * @brief Automatic test registration helper
 *
 * Usage in test file:
 * @code
 * struct Test144 : public AotTestBase {
 *     static constexpr int TEST_ID = 144;
 *     static constexpr const char* DESCRIPTION = "Event queue ordering";
 *     // ... implement interface
 * };
 * REGISTER_AOT_TEST(Test144);
 * @endcode
 */
template <typename TestClass> struct AotTestRegistrar {
    AotTestRegistrar() {
        AotTestRegistry::instance().registerTest(TestClass::TEST_ID, []() { return std::make_unique<TestClass>(); });
    }
};

}  // namespace RSM::W3C::AotTests

/**
 * @brief Macro to auto-register a AOT test
 *
 * Creates static instance that registers test during program initialization.
 * Must be used at global namespace scope.
 */
#define REGISTER_AOT_TEST(TestClass)                                                                                   \
    namespace {                                                                                                        \
    static ::RSM::W3C::AotTests::AotTestRegistrar<TestClass> registrar_##TestClass{};                                  \
    }
