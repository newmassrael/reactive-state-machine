#include "../interfaces/ITestSuite.h"
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace RSM::W3C {

/**
 * @brief W3C test suite implementation
 */
class W3CTestSuite : public ITestSuite {
private:
    std::string resourcePath_;

public:
    explicit W3CTestSuite(const std::string &resourcePath = "resources") : resourcePath_(resourcePath) {}

    ~W3CTestSuite() override = default;

    TestSuiteInfo getInfo() const override {
        TestSuiteInfo info;
        info.name = "W3C SCXML Test Suite";
        info.description = "Official W3C SCXML 1.0 Conformance Tests";
        info.resourcePath = resourcePath_;

        // Count available tests
        try {
            auto tests = const_cast<W3CTestSuite *>(this)->discoverTests();
            info.totalTests = tests.size();
        } catch (...) {
            info.totalTests = 0;
        }

        return info;
    }

    std::vector<std::string> discoverTests() override {
        std::vector<std::string> testDirs;

        try {
            for (const auto &entry : std::filesystem::directory_iterator(resourcePath_)) {
                if (entry.is_directory()) {
                    std::string dirName = entry.path().filename().string();

                    // Check if directory name is numeric (W3C test ID)
                    if (isNumericTestDir(dirName)) {
                        // Verify required files exist
                        std::string txmlPath = getTXMLPath(entry.path().string());
                        std::string metadataPath = getMetadataPath(entry.path().string());

                        if (std::filesystem::exists(txmlPath) && std::filesystem::exists(metadataPath)) {
                            testDirs.push_back(entry.path().string());
                        }
                    }
                }
            }
        } catch (const std::exception &e) {
            throw std::runtime_error("Failed to discover W3C tests: " + std::string(e.what()));
        }

        // Sort test directories by test ID
        std::sort(testDirs.begin(), testDirs.end(), [](const std::string &a, const std::string &b) {
            int idA = extractTestId(a);
            int idB = extractTestId(b);
            return idA < idB;
        });

        return testDirs;
    }

    std::string getTXMLPath(const std::string &testDirectory) override {
        // Extract test ID from directory path
        int testId = extractTestId(testDirectory);
        return testDirectory + "/test" + std::to_string(testId) + ".txml";
    }

    std::string getMetadataPath(const std::string &testDirectory) override {
        return testDirectory + "/metadata.txt";
    }

    std::vector<std::string> filterTests(const std::string &conformanceLevel, const std::string &specSection) override {
        auto allTests = discoverTests();

        if (conformanceLevel.empty() && specSection.empty()) {
            return allTests;
        }

        std::vector<std::string> filteredTests;

        // For now, return all tests
        // TODO: Implement filtering based on metadata parsing
        return allTests;
    }

private:
    bool isNumericTestDir(const std::string &dirName) {
        return !dirName.empty() && std::all_of(dirName.begin(), dirName.end(), ::isdigit);
    }

    static int extractTestId(const std::string &testPath) {
        std::filesystem::path path(testPath);
        std::string dirName = path.filename().string();

        try {
            return std::stoi(dirName);
        } catch (...) {
            return 0;
        }
    }
};

}  // namespace RSM::W3C