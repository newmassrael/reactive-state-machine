// W3C Static Test CLI
// Command-line interface for running W3C SCXML tests with static code generation

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Include all generated state machine headers
#include "test144_sm.h"
#include "test147_sm.h"
#include "test148_sm.h"

// Add more as needed: #include "test149_sm.h"

// Test registry structure
struct StaticTest {
    int number;
    const char *description;
    bool (*runner)();
};

// Individual test runner implementations
namespace TestRunners {

bool test144() {
    RSM::Generated::test144::test144 sm;
    sm.initialize();

    // Test 144: Verify SCXML event queue ordering (foo before bar)
    // After initialize(), onentry of s0 should have raised foo, then bar
    // Internal queue should have processed foo first (transition s0->s1)
    // Then processed bar (transition s1->pass)
    // Verify we're in the Pass final state
    return sm.isInFinalState() && sm.getCurrentState() == RSM::Generated::test144::State::Pass;
}

bool test147() {
    RSM::Generated::test147::test147 sm;
    sm.initialize();

    // Test 147: Verify SCXML if/elseif/else and datamodel
    // After initialize(), onentry of s0 should execute elseif(true) branch:
    // - Raise bar event, increment Var1 to 1
    // - Then raise bat event
    // Internal queue processes bar with guard Var1==1, transition to Pass
    return sm.isInFinalState() && sm.getCurrentState() == RSM::Generated::test147::State::Pass;
}

bool test148() {
    RSM::Generated::test148::test148 sm;
    sm.initialize();

    // Test 148: Verify SCXML else clause execution
    // After initialize(), onentry of s0 should execute else branch:
    // - if(false) and elseif(false) both skip
    // - else branch executes: raise baz, increment Var1 to 1
    // - Then raise bat event
    // Internal queue processes baz with guard Var1==1, transition to Pass
    return sm.isInFinalState() && sm.getCurrentState() == RSM::Generated::test148::State::Pass;
}

// Add more test runners as needed

}  // namespace TestRunners

// Test registry
static const StaticTest STATIC_TESTS[] = {
    {144, "Event queue ordering", TestRunners::test144},
    {147, "If/elseif/else conditionals with datamodel", TestRunners::test147},
    {148, "Else clause execution with datamodel", TestRunners::test148},
    // Add more tests here
};

static const size_t NUM_STATIC_TESTS = sizeof(STATIC_TESTS) / sizeof(STATIC_TESTS[0]);

void printUsage(const char *progName) {
    std::cout << "W3C Static Test CLI - SCXML static code generation test runner\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << progName << " <test_number>      Run specific test\n";
    std::cout << "  " << progName << " ID1 ID2 ...        Run multiple specific tests\n";
    std::cout << "  " << progName << " START~END          Run tests in range (e.g., 144~200)\n";
    std::cout << "  " << progName << " ~NUMBER            Run all tests up to NUMBER (e.g., ~200)\n";
    std::cout << "  " << progName << " --list             List all available tests\n";
    std::cout << "  " << progName << " --all              Run all tests\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " 144                Run test 144\n";
    std::cout << "  " << progName << " 144 147 148        Run tests 144, 147, 148\n";
    std::cout << "  " << progName << " 144~200            Run tests from 144 to 200\n";
    std::cout << "  " << progName << " ~200               Run all tests up to 200\n";
    std::cout << "  " << progName << " --all              Run all tests\n";
}

void listTests() {
    std::cout << "Available static W3C tests:\n\n";
    for (size_t i = 0; i < NUM_STATIC_TESTS; ++i) {
        std::cout << "  " << STATIC_TESTS[i].number << ": " << STATIC_TESTS[i].description << "\n";
    }
    std::cout << "\nTotal: " << NUM_STATIC_TESTS << " tests\n";
}

bool runTest(int testNum) {
    for (size_t i = 0; i < NUM_STATIC_TESTS; ++i) {
        if (STATIC_TESTS[i].number == testNum) {
            std::cout << "Running test " << testNum << ": " << STATIC_TESTS[i].description << " ... ";

            try {
                bool result = STATIC_TESTS[i].runner();
                if (result) {
                    std::cout << "PASS\n";
                    return true;
                } else {
                    std::cout << "FAIL\n";
                    return false;
                }
            } catch (const std::exception &e) {
                std::cout << "EXCEPTION: " << e.what() << "\n";
                return false;
            }
        }
    }

    std::cerr << "Error: Test " << testNum << " not found\n";
    return false;
}

bool runAllTests() {
    int passed = 0;
    int failed = 0;

    std::cout << "Running " << NUM_STATIC_TESTS << " static W3C tests...\n\n";

    for (size_t i = 0; i < NUM_STATIC_TESTS; ++i) {
        if (runTest(STATIC_TESTS[i].number)) {
            passed++;
        } else {
            failed++;
        }
    }

    std::cout << "\n===============================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "===============================\n";

    return failed == 0;
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    std::vector<int> testNums;
    bool runAll = (argc == 1);  // Run all tests if no arguments provided

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }

        if (arg == "--list" || arg == "-l") {
            listTests();
            return 0;
        }

        if (arg == "--all" || arg == "-a") {
            runAll = true;
            break;
        }

        // Handle ~NUMBER format (run up to)
        if (arg.length() > 1 && arg[0] == '~') {
            try {
                int upTo = std::stoi(arg.substr(1));
                for (size_t j = 0; j < NUM_STATIC_TESTS; ++j) {
                    if (STATIC_TESTS[j].number <= upTo) {
                        testNums.push_back(STATIC_TESTS[j].number);
                    }
                }
                continue;
            } catch (...) {
                std::cerr << "Error: Invalid ~NUMBER format: " << arg << "\n";
                return 1;
            }
        }

        // Handle START~END range format
        size_t tildePos = arg.find('~');
        if (tildePos != std::string::npos && tildePos > 0) {
            try {
                int start = std::stoi(arg.substr(0, tildePos));
                int end = std::stoi(arg.substr(tildePos + 1));

                if (start > end) {
                    std::cerr << "Error: Invalid range - start must be <= end\n";
                    return 1;
                }

                // Add all available tests in range
                for (size_t j = 0; j < NUM_STATIC_TESTS; ++j) {
                    if (STATIC_TESTS[j].number >= start && STATIC_TESTS[j].number <= end) {
                        testNums.push_back(STATIC_TESTS[j].number);
                    }
                }
                continue;
            } catch (...) {
                std::cerr << "Error: Invalid range format: " << arg << "\n";
                return 1;
            }
        }

        // Try to parse as single test number
        try {
            int testNum = std::stoi(arg);
            testNums.push_back(testNum);
        } catch (...) {
            std::cerr << "Error: Invalid argument '" << arg << "'\n\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Execute tests
    if (runAll) {
        return runAllTests() ? 0 : 1;
    }

    if (testNums.empty()) {
        std::cerr << "Error: No tests specified\n\n";
        printUsage(argv[0]);
        return 1;
    }

    // Run specified tests
    int passed = 0;
    int failed = 0;

    std::cout << "Running " << testNums.size() << " static W3C test(s)...\n\n";

    for (int testNum : testNums) {
        if (runTest(testNum)) {
            passed++;
        } else {
            failed++;
        }
    }

    if (testNums.size() > 1) {
        std::cout << "\n===============================\n";
        std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
        std::cout << "===============================\n";
    }

    return failed == 0 ? 0 : 1;
}
