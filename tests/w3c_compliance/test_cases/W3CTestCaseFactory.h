/**
 * @file W3CTestCaseFactory.h
 * @brief Factory for creating specialized W3C test case implementations
 */

#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include "Test144EventOrdering.h"
#include "Test147ConditionalExecution.h"
#include "Test185SendDelay.h"
#include "W3CTestCase.h"

namespace W3CCompliance {

/**
 * @brief Factory class for creating specialized W3C test case implementations
 *
 * This factory manages the creation of test-specific implementations while
 * maintaining the single binary architecture. Each test can have its own
 * specialized validation logic while sharing common infrastructure.
 */
class W3CTestCaseFactory {
public:
    using TestCaseCreator = std::function<std::unique_ptr<W3CTestCase>()>;

    static W3CTestCaseFactory &getInstance() {
        static W3CTestCaseFactory instance;
        return instance;
    }

    /**
     * @brief Create appropriate test case implementation for given test ID
     * @param testId W3C test case ID (144-580)
     * @return Specialized test case implementation or default if none exists
     */
    std::unique_ptr<W3CTestCase> createTestCase(int testId) {
        auto it = creators_.find(testId);
        if (it != creators_.end()) {
            return it->second();
        }

        // Return default implementation for tests without specialized logic
        return std::make_unique<W3CDefaultTestCase>();
    }

    /**
     * @brief Check if test has specialized implementation
     */
    bool hasSpecializedImplementation(int testId) const {
        return creators_.find(testId) != creators_.end();
    }

private:
    W3CTestCaseFactory() {
        registerBuiltinTestCases();
    }

    /**
     * @brief Register all built-in specialized test case implementations
     */
    void registerBuiltinTestCases() {
        // Test 144: Event queue ordering
        creators_[144] = []() -> std::unique_ptr<W3CTestCase> { return std::make_unique<Test144EventOrdering>(); };

        // Test 147: Conditional execution with <if> elements
        creators_[147] = []() -> std::unique_ptr<W3CTestCase> {
            return std::make_unique<Test147ConditionalExecution>();
        };

        // Test 185: Send delay validation
        creators_[185] = []() -> std::unique_ptr<W3CTestCase> { return std::make_unique<Test185SendDelay>(); };

        // Additional specialized test cases can be registered here
        // Example:
        // creators_[148] = []() -> std::unique_ptr<W3CTestCase> {
        //     return std::make_unique<Test148SomeOtherTest>();
        // };
    }

    std::unordered_map<int, TestCaseCreator> creators_;
};

}  // namespace W3CCompliance