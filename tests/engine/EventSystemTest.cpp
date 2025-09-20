#include "runtime/SCXMLTypes.h"
#include "scripting/JSEngine.h"
#include <gtest/gtest.h>
#include <memory>

class EventSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = &RSM::JSEngine::instance();
        // JSEngine 리셋으로 테스트 간 격리 보장
        engine_->reset();

        sessionId_ = "test_session_events";
        bool result = engine_->createSession(sessionId_, "");
        ASSERT_TRUE(result) << "Failed to create session";
    }

    void TearDown() override {
        if (engine_) {
            engine_->destroySession(sessionId_);
            engine_->shutdown();
        }
    }

    RSM::JSEngine *engine_;
    std::string sessionId_;
};

// Test _event object exists and has required properties
TEST_F(EventSystemTest, EventObjectStructure) {
    // Test _event exists
    auto eventResult = engine_->evaluateExpression(sessionId_, "typeof _event").get();
    ASSERT_TRUE(eventResult.isSuccess());
    EXPECT_EQ(eventResult.getValue<std::string>(), "object");

    // Test required SCXML event properties
    std::vector<std::string> requiredProps = {"name", "type", "sendid", "origin", "origintype", "invokeid", "data"};

    for (const auto &prop : requiredProps) {
        std::string expr = "_event.hasOwnProperty('" + prop + "')";
        auto propResult = engine_->evaluateExpression(sessionId_, expr).get();
        ASSERT_TRUE(propResult.isSuccess()) << "Failed to check property: " << prop;
        EXPECT_TRUE(propResult.getValue<bool>()) << "_event should have property: " << prop;
    }
}

// Test default event values
TEST_F(EventSystemTest, DefaultEventValues) {
    // Test default name is empty string
    auto nameResult = engine_->evaluateExpression(sessionId_, "_event.name").get();
    ASSERT_TRUE(nameResult.isSuccess());
    EXPECT_EQ(nameResult.getValue<std::string>(), "");

    // Test default type is empty string
    auto typeResult = engine_->evaluateExpression(sessionId_, "_event.type").get();
    ASSERT_TRUE(typeResult.isSuccess());
    EXPECT_EQ(typeResult.getValue<std::string>(), "");

    // Test data is initially null (check by typeof)
    auto dataResult = engine_->evaluateExpression(sessionId_, "_event.data === null").get();
    ASSERT_TRUE(dataResult.isSuccess());
    EXPECT_TRUE(dataResult.getValue<bool>());
}

// Test event object is read-only in proper implementation
TEST_F(EventSystemTest, EventObjectModification) {
    // Try to modify _event properties
    auto modifyResult = engine_->executeScript(sessionId_, "_event.name = 'modified'; _event.name").get();
    ASSERT_TRUE(modifyResult.isSuccess());

    // In current implementation, modification works (not enforcing read-only yet)
    // This documents current behavior - in full SCXML implementation, _event
    // should be read-only
    EXPECT_EQ(modifyResult.getValue<std::string>(), "modified");

    // TODO: In complete SCXML implementation, _event should be read-only
    // and attempts to modify should trigger 'error.execution' event
}

// Test event data handling
TEST_F(EventSystemTest, EventDataHandling) {
    // Test setting simple data
    auto simpleDataResult = engine_->executeScript(sessionId_, "_event.data = 'simple_data'; _event.data").get();
    ASSERT_TRUE(simpleDataResult.isSuccess());
    EXPECT_EQ(simpleDataResult.getValue<std::string>(), "simple_data");

    // Test setting object data
    auto objectDataResult = engine_
                                ->executeScript(sessionId_, "_event.data = {key: 'value', number: 42}; "
                                                            "_event.data.key + '_' + _event.data.number")
                                .get();
    ASSERT_TRUE(objectDataResult.isSuccess());
    EXPECT_EQ(objectDataResult.getValue<std::string>(), "value_42");

    // Test setting array data
    auto arrayDataResult = engine_->executeScript(sessionId_, "_event.data = [1, 2, 3]; _event.data.length").get();
    ASSERT_TRUE(arrayDataResult.isSuccess());
    EXPECT_EQ(arrayDataResult.getValue<double>(), 3.0);
}

// Test event name and type handling
TEST_F(EventSystemTest, EventNameAndType) {
    // Test setting event name
    auto nameResult = engine_->executeScript(sessionId_, "_event.name = 'user.login'; _event.name").get();
    ASSERT_TRUE(nameResult.isSuccess());
    EXPECT_EQ(nameResult.getValue<std::string>(), "user.login");

    // Test setting event type
    auto typeResult = engine_->executeScript(sessionId_, "_event.type = 'platform'; _event.type").get();
    ASSERT_TRUE(typeResult.isSuccess());
    EXPECT_EQ(typeResult.getValue<std::string>(), "platform");

    // Test complex event names with dots
    auto complexNameResult =
        engine_->executeScript(sessionId_, "_event.name = 'error.execution.timeout'; _event.name").get();
    ASSERT_TRUE(complexNameResult.isSuccess());
    EXPECT_EQ(complexNameResult.getValue<std::string>(), "error.execution.timeout");
}

// Test event origin and invocation properties
TEST_F(EventSystemTest, EventOriginProperties) {
    // Test setting origin
    auto originResult = engine_->executeScript(sessionId_, "_event.origin = '#_internal'; _event.origin").get();
    ASSERT_TRUE(originResult.isSuccess());
    EXPECT_EQ(originResult.getValue<std::string>(), "#_internal");

    // Test setting origintype
    auto origintypeResult = engine_
                                ->executeScript(sessionId_, "_event.origintype = "
                                                            "'http://www.w3.org/TR/scxml/#SCXMLEventProcessor'; "
                                                            "_event.origintype")
                                .get();
    ASSERT_TRUE(origintypeResult.isSuccess());
    EXPECT_EQ(origintypeResult.getValue<std::string>(), "http://www.w3.org/TR/scxml/#SCXMLEventProcessor");

    // Test setting invokeid
    auto invokeidResult = engine_->executeScript(sessionId_, "_event.invokeid = 'invoke_123'; _event.invokeid").get();
    ASSERT_TRUE(invokeidResult.isSuccess());
    EXPECT_EQ(invokeidResult.getValue<std::string>(), "invoke_123");

    // Test setting sendid
    auto sendidResult = engine_->executeScript(sessionId_, "_event.sendid = 'send_456'; _event.sendid").get();
    ASSERT_TRUE(sendidResult.isSuccess());
    EXPECT_EQ(sendidResult.getValue<std::string>(), "send_456");
}

// Test event object in expressions
TEST_F(EventSystemTest, EventInExpressions) {
    // Set up event data
    auto setupResult = engine_
                           ->executeScript(sessionId_, "_event.name = 'user.action'; _event.data = {userId: "
                                                       "123, action: 'click'}; true")
                           .get();
    ASSERT_TRUE(setupResult.isSuccess());

    // Test using event in conditional expressions
    auto conditionalResult =
        engine_->evaluateExpression(sessionId_, "_event.name === 'user.action' && _event.data.userId === 123").get();
    ASSERT_TRUE(conditionalResult.isSuccess());
    EXPECT_TRUE(conditionalResult.getValue<bool>());

    // Test accessing nested event data
    auto nestedResult = engine_->evaluateExpression(sessionId_, "_event.data.action").get();
    ASSERT_TRUE(nestedResult.isSuccess());
    EXPECT_EQ(nestedResult.getValue<std::string>(), "click");

    // Test using event data in calculations
    auto calcResult = engine_->evaluateExpression(sessionId_, "_event.data.userId * 2").get();
    ASSERT_TRUE(calcResult.isSuccess());
    EXPECT_EQ(calcResult.getValue<double>(), 246.0);
}

// Test event object serialization
TEST_F(EventSystemTest, EventSerialization) {
    // Set up event with complex data
    auto setupResult = engine_
                           ->executeScript(sessionId_, "_event.name = 'complex.event'; "
                                                       "_event.data = {user: {id: 1, name: "
                                                       "'test'}, items: [1, 2, 3]}; true")
                           .get();
    ASSERT_TRUE(setupResult.isSuccess());

    // Test JSON serialization of event data
    auto serializeResult = engine_->evaluateExpression(sessionId_, "JSON.stringify(_event.data)").get();
    ASSERT_TRUE(serializeResult.isSuccess());

    std::string expected = "{\"user\":{\"id\":1,\"name\":\"test\"},\"items\":[1,2,3]}";
    EXPECT_EQ(serializeResult.getValue<std::string>(), expected);

    // Test serializing entire event object (excluding functions)
    auto fullSerializeResult =
        engine_->evaluateExpression(sessionId_, "JSON.stringify({name: _event.name, data: _event.data})").get();
    ASSERT_TRUE(fullSerializeResult.isSuccess());

    std::string expectedFull = "{\"name\":\"complex.event\",\"data\":{\"user\":{"
                               "\"id\":1,\"name\":\"test\"},\"items\":[1,2,3]}}";
    EXPECT_EQ(fullSerializeResult.getValue<std::string>(), expectedFull);
}

// Test event object across multiple evaluations
TEST_F(EventSystemTest, EventPersistence) {
    // Set event data in first evaluation
    auto setResult = engine_
                         ->executeScript(sessionId_, "_event.name = 'persistent.event'; "
                                                     "_event.data = 'persistent_data'; true")
                         .get();
    ASSERT_TRUE(setResult.isSuccess());

    // Check event data persists in subsequent evaluations
    auto checkNameResult = engine_->evaluateExpression(sessionId_, "_event.name").get();
    ASSERT_TRUE(checkNameResult.isSuccess());
    EXPECT_EQ(checkNameResult.getValue<std::string>(), "persistent.event");

    auto checkDataResult = engine_->evaluateExpression(sessionId_, "_event.data").get();
    ASSERT_TRUE(checkDataResult.isSuccess());
    EXPECT_EQ(checkDataResult.getValue<std::string>(), "persistent_data");

    // Modify in another evaluation
    auto modifyResult = engine_->executeScript(sessionId_, "_event.data = 'modified_data'; _event.data").get();
    ASSERT_TRUE(modifyResult.isSuccess());
    EXPECT_EQ(modifyResult.getValue<std::string>(), "modified_data");

    // Verify modification persists
    auto verifyResult = engine_->evaluateExpression(sessionId_, "_event.data").get();
    ASSERT_TRUE(verifyResult.isSuccess());
    EXPECT_EQ(verifyResult.getValue<std::string>(), "modified_data");
}
