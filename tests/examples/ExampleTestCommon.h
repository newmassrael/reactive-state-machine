#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Example system includes
#include "generator/include/parsing/DocumentParser.h"
#include "generator/include/model/DocumentModel.h"

// Core includes
#include "generator/include/core/NodeFactory.h"

// Base test fixture for example tests
class ExampleTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nodeFactory = std::make_shared<SCXML::Core::NodeFactory>();
        parser = std::make_unique<SCXML::Parsing::DocumentParser>(nodeFactory);
    }

    void TearDown() override
    {
        parser.reset();
        nodeFactory.reset();
    }

    std::shared_ptr<SCXML::Core::NodeFactory> nodeFactory;
    std::unique_ptr<SCXML::Parsing::DocumentParser> parser;
    
    // Helper methods for example testing
    std::string loadExampleSCXML(const std::string& filename);
    bool verifyExampleExecution(const std::string& exampleName);
};