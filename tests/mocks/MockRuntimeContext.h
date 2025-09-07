#pragma once

#include <gmock/gmock.h>
#include "runtime/RuntimeContext.h"
#include "runtime/DataModelEngine.h"

class MockDataModelEngine;

class MockRuntimeContext : public SCXML::Runtime::RuntimeContext
{
public:
    MockRuntimeContext() : SCXML::Runtime::RuntimeContext() {}
    virtual ~MockRuntimeContext() = default;

    // Mock the getDataModelEngine method
    MOCK_METHOD(SCXML::DataModelEngine*, getDataModelEngine, (), (const));
    
    // Mock other commonly used methods
    MOCK_METHOD(void, setCurrentState, (const std::string &stateId), (override));
    MOCK_METHOD(std::string, getCurrentState, (), (const override));
    MOCK_METHOD(void, raiseEvent, (const std::string &eventName, const std::string &data), (override));
    MOCK_METHOD(void, setDataValue, (const std::string &id, const std::string &value), (override));
    MOCK_METHOD(std::string, getDataValue, (const std::string &id), (const override));
    MOCK_METHOD(bool, hasDataValue, (const std::string &id), (const override));
    MOCK_METHOD(std::string, evaluateExpression, (const std::string &expression), (const override));
    MOCK_METHOD(bool, evaluateCondition, (const std::string &condition), (const override));
};