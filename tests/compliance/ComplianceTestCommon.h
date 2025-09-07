#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// W3C SCXML compliance testing includes
#include "generator/include/parsing/DocumentParser.h"
#include "generator/include/runtime/RuntimeContext.h"

// Mock includes
#include "../mocks/MockNodeFactory.h"

// Base test fixture for W3C compliance tests
class ComplianceTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockFactory = std::make_shared<MockNodeFactory>();
        parser = std::make_unique<SCXML::Parsing::DocumentParser>(mockFactory);
    }

    void TearDown() override
    {
        parser.reset();
        mockFactory.reset();
    }

    std::shared_ptr<MockNodeFactory> mockFactory;
    std::unique_ptr<SCXML::Parsing::DocumentParser> parser;
    
    // W3C compliance helpers
    bool validateW3CCompliance(const std::string& scxml);
    void runW3CTestSuite(const std::string& testSuiteName);
    std::vector<std::string> loadW3CTestCases(const std::string& category);
};