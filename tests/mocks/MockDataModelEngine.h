#pragma once

#include <gmock/gmock.h>
#include "runtime/DataModelEngine.h"

// Mock DataModel evaluation result for simplicity  
namespace SCXML {
namespace Runtime {
namespace DataModel {

struct EvaluationResult {
    bool isSuccess_ = false;
    std::string value_;
    std::string errorMessage_;

    EvaluationResult() = default;
    EvaluationResult(bool s, const std::string& v) : isSuccess_(s), value_(v) {}

    static EvaluationResult success(const std::string& result) {
        return EvaluationResult(true, result);
    }

    static EvaluationResult failure(const std::string& error) {
        EvaluationResult result;
        result.isSuccess_ = false;
        result.errorMessage_ = error;
        return result;
    }

    bool isSuccess() const { return isSuccess_; }
    const std::string& getValue() const { return value_; }
    const std::string& getErrorMessage() const { return errorMessage_; }
};

} // namespace DataModel
} // namespace Runtime
} // namespace SCXML

class MockDataModelEngine : public SCXML::DataModelEngine
{
public:
    MockDataModelEngine() : SCXML::DataModelEngine() {}
    virtual ~MockDataModelEngine() = default;

    // Mock the evaluateExpression method - simplified signature
    MOCK_METHOD(SCXML::Runtime::DataModel::EvaluationResult, evaluateExpression, 
                (const std::string &expression), (const));
};