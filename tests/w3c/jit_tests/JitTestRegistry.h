#pragma once

#include "JitTestBase.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace RSM::W3C::JitTests {

/**
 * @brief Singleton registry for JIT tests
 *
 * Manages automatic registration and creation of JIT test instances.
 * Tests register themselves via REGISTER_JIT_TEST macro during static initialization.
 */
class JitTestRegistry {
public:
    using TestFactory = std::function<std::unique_ptr<JitTestBase>()>;

    /**
     * @brief Get singleton instance
     */
    static JitTestRegistry &instance() {
        static JitTestRegistry registry;
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
    std::unique_ptr<JitTestBase> createTest(int testId) const {
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
    JitTestRegistry() = default;
    std::map<int, TestFactory> tests_;
};

/**
 * @brief Automatic test registration helper
 *
 * Usage in test file:
 * @code
 * struct Test144 : public JitTestBase {
 *     static constexpr int TEST_ID = 144;
 *     static constexpr const char* DESCRIPTION = "Event queue ordering";
 *     // ... implement interface
 * };
 * REGISTER_JIT_TEST(Test144);
 * @endcode
 */
template <typename TestClass> struct JitTestRegistrar {
    JitTestRegistrar() {
        JitTestRegistry::instance().registerTest(TestClass::TEST_ID, []() { return std::make_unique<TestClass>(); });
    }
};

}  // namespace RSM::W3C::JitTests

/**
 * @brief Macro to auto-register a JIT test
 *
 * Creates static instance that registers test during program initialization.
 * Must be used at global namespace scope.
 */
#define REGISTER_JIT_TEST(TestClass)                                                                                   \
    namespace {                                                                                                        \
    static ::RSM::W3C::JitTests::JitTestRegistrar<TestClass> registrar_##TestClass{};                                  \
    }
