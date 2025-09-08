#pragma once

#include <gmock/gmock.h>
#include "runtime/DataModelEngine.h"

class MockDataModelEngine : public SCXML::DataModelEngine
{
public:
    MockDataModelEngine() : SCXML::DataModelEngine() {}
    virtual ~MockDataModelEngine() = default;

    // Mock the essential methods needed for DataNode testing
    MOCK_METHOD(DataModelResult, setValue, 
                (const std::string &location, const DataValue &value, Scope scope), (override));
    
    MOCK_METHOD(DataModelResult, getValue, 
                (const std::string &location, std::optional<Scope> scope), (const, override));
    
    MOCK_METHOD(DataModelResult, evaluateExpression, 
                (const std::string &expression, SCXML::Runtime::RuntimeContext &context), (override));
    
    MOCK_METHOD(bool, evaluateCondition, 
                (const std::string &condition, SCXML::Runtime::RuntimeContext &context), (override));
    
    MOCK_METHOD(std::string, valueToString, 
                (const DataValue &value), (const, override));
    
    MOCK_METHOD(bool, hasValue, 
                (const std::string &location, std::optional<Scope> scope), (const, override));
    
    MOCK_METHOD(void, clear, 
                (std::optional<Scope> scope), (override));
};