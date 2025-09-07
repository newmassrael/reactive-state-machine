#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Full system includes
#include "generator/include/parsing/DocumentParser.h"
#include "generator/include/runtime/RuntimeContext.h"
#include "generator/include/model/DocumentModel.h"

// Real factory include
#include "generator/include/core/NodeFactory.h"

// Base test fixture for integration tests
class IntegrationTestBase : public ::testing::Test
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
    
    // Helper methods for integration testing
    bool validateSCXMLDocument(const std::string& scxml);
    void executeStateMachine(const std::string& scxml, const std::vector<std::string>& events);
};