#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Core includes
#include "generator/include/core/StateNode.h"
#include "generator/include/core/TransitionNode.h"
#include "generator/include/core/ActionNode.h"
#include "generator/include/core/GuardNode.h"
#include "generator/include/core/DataModelItem.h"
#include "generator/include/core/InvokeNode.h"

// Mock includes
#include "../mocks/MockNodeFactory.h"
#include "../mocks/MockStateNode.h"
#include "../mocks/MockTransitionNode.h"
#include "../mocks/MockActionNode.h"
#include "../mocks/MockDataModelItem.h"

// Base test fixture for core module tests
class CoreTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockFactory = std::make_shared<MockNodeFactory>();
    }

    void TearDown() override
    {
        mockFactory.reset();
    }

    std::shared_ptr<MockNodeFactory> mockFactory;
};