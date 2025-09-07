#pragma once

#include <gmock/gmock.h>
#include "model/IExecutionContext.h"

class MockExecutionContext : public SCXML::Model::IExecutionContext
{
public:
    MockExecutionContext() = default;
    virtual ~MockExecutionContext() = default;

    // Mock all IExecutionContext methods
    MOCK_METHOD(SCXML::Common::Result<void>, setDataValue, 
                (const std::string &name, const std::string &value), (override));
    
    MOCK_METHOD(SCXML::Common::Result<std::string>, getDataValue, 
                (const std::string &name), (const, override));
    
    MOCK_METHOD(bool, hasDataValue, 
                (const std::string &name), (const, override));
    
    MOCK_METHOD(SCXML::Common::Result<void>, sendEvent, 
                (const std::string &eventName, const std::string &eventData), (override));
    
    MOCK_METHOD(SCXML::Common::Result<void>, raiseEvent, 
                (const std::string &eventName, const std::string &eventData), (override));
    
    MOCK_METHOD(SCXML::Common::Result<void>, cancelEvent, 
                (const std::string &sendId), (override));
    
    MOCK_METHOD(std::string, getCurrentStateId, (), (const, override));
    
    MOCK_METHOD(bool, isStateActive, 
                (const std::string &stateId), (const, override));
    
    MOCK_METHOD(SCXML::Common::Result<std::string>, evaluateExpression, 
                (const std::string &expression), (override));
    
    MOCK_METHOD(SCXML::Common::Result<bool>, evaluateCondition, 
                (const std::string &condition), (override));
    
    MOCK_METHOD(void, log, 
                (const std::string &level, const std::string &message), (override));
    
    MOCK_METHOD(std::string, getSessionInfo, (), (const, override));
};