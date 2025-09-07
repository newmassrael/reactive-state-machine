#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Runtime includes
#include "generator/include/runtime/RuntimeContext.h"
#include "generator/include/runtime/GuardEvaluator.h"
#include "generator/include/runtime/DataModelEngine.h"
#include "generator/include/runtime/HistoryStateManager.h"
#include "generator/include/runtime/ExecutableContentProcessor.h"
#include "generator/include/runtime/ActionProcessor.h"
#include "generator/include/runtime/ActionExecutor.h"

// Mock includes
#include "../mocks/MockNodeFactory.h"
#include "../mocks/MockStateNode.h"
#include "../mocks/MockTransitionNode.h"
#include "../mocks/MockActionNode.h"

// Base test fixture for runtime module tests
class RuntimeTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockFactory = std::make_shared<MockNodeFactory>();
        // Runtime specific setup can be added here
    }

    void TearDown() override
    {
        mockFactory.reset();
    }

    std::shared_ptr<MockNodeFactory> mockFactory;
};