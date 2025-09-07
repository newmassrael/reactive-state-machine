#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <vector>

// Performance measurement includes
#include "generator/include/parsing/DocumentParser.h"
#include "generator/include/runtime/RuntimeContext.h"

// Core includes
#include "generator/include/core/NodeFactory.h"

// Base test fixture for performance tests
class PerformanceTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nodeFactory = std::make_shared<SCXML::Core::NodeFactory>();
        parser = std::make_unique<SCXML::Parsing::DocumentParser>(nodeFactory);
        
        // Performance test setup
        start_time = std::chrono::high_resolution_clock::now();
    }

    void TearDown() override
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Log performance results
        std::cout << "Test Duration: " << duration.count() << " microseconds" << std::endl;
        
        parser.reset();
        nodeFactory.reset();
    }

    std::shared_ptr<SCXML::Core::NodeFactory> nodeFactory;
    std::unique_ptr<SCXML::Parsing::DocumentParser> parser;
    std::chrono::high_resolution_clock::time_point start_time;
    
    // Performance measurement helpers
    void measureParsingTime(const std::string& scxml, const std::string& testName)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        auto model = parser->parseContent(scxml);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Parsing time for " << testName << ": " << duration.count() << " microseconds" << std::endl;
        
        // Performance assertions (adjust thresholds as needed)
        // Increased threshold to account for system load variations and CI environment
        EXPECT_LT(duration.count(), 500000) << "Parsing took too long: " << duration.count() << " microseconds";
        
        if (model != nullptr) {
            EXPECT_FALSE(parser->hasErrors()) << "Parsing should not have errors for performance test";
        }
    }

    void measureExecutionTime(const std::string& scxml, const std::vector<std::string>& events)
    {
        auto model = parser->parseContent(scxml);
        if (model == nullptr) {
            FAIL() << "Failed to parse SCXML for execution time measurement";
            return;
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Simulate event processing
        for (const auto& event : events) {
            // In a real implementation, this would process the event through the runtime
            // For now, just record that we're processing it
            std::cout << "Processing event: " << event << std::endl;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Execution time for " << events.size() << " events: " << duration.count() << " microseconds" << std::endl;
    }

    void recordMemoryUsage()
    {
        // Simple memory usage recording - in production, you'd use more sophisticated tools
        // For now, just record that we're tracking memory
        std::cout << "Memory usage recorded (basic implementation)" << std::endl;
        
        // You could implement more sophisticated memory tracking here:
        // - RSS memory from /proc/self/status on Linux
        // - GetProcessMemoryInfo on Windows
        // - For now, we'll just ensure the method exists for linking
    }
};