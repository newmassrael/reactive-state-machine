#include "SCXMLTypes.h"
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

// Test event object is read-only per SCXML W3C specification
TEST_F(EventSystemTest, W3C_EventObjectReadOnlyCompliance) {
    // Verify _event object exists and is read-only
    auto eventTypeResult = engine_->evaluateExpression(sessionId_, "typeof _event").get();
    ASSERT_TRUE(eventTypeResult.isSuccess());
    EXPECT_EQ(eventTypeResult.getValue<std::string>(), "object");

    // Test that _event properties cannot be modified
    std::vector<std::string> properties = {"name", "type", "sendid", "origin", "origintype", "invokeid", "data"};

    for (const auto &prop : properties) {
        // Try to modify property - should throw error
        std::string modifyScript = "_event." + prop + " = 'modified_value'; _event." + prop;
        auto modifyResult = engine_->executeScript(sessionId_, modifyScript).get();

        // SCXML W3C compliant: modification should fail
        EXPECT_FALSE(modifyResult.isSuccess())
            << "Modification of _event." << prop << " should fail per SCXML W3C spec";

        // Verify property remains unchanged
        std::string checkScript = "_event." + prop;
        auto checkResult = engine_->evaluateExpression(sessionId_, checkScript).get();
        ASSERT_TRUE(checkResult.isSuccess());

        // Properties should still have their default values
        if (prop == "data") {
            auto dataCheck = engine_->evaluateExpression(sessionId_, "_event.data === null").get();
            ASSERT_TRUE(dataCheck.isSuccess());
            EXPECT_TRUE(dataCheck.getValue<bool>()) << "_event.data should remain null";
        } else {
            EXPECT_EQ(checkResult.getValue<std::string>(), "") << "_event." << prop << " should remain empty string";
        }
    }
}

// Test internal event updating (used by StateMachine)
TEST_F(EventSystemTest, InternalEventDataUpdating) {
    // Test setCurrentEvent API with string data (JSON formatted)
    auto testEvent = std::make_shared<RSM::Event>("test.event", "internal");
    testEvent->setRawJsonData("\"test_data\"");  // JSON string format

    auto setResult = engine_->setCurrentEvent(sessionId_, testEvent).get();
    ASSERT_TRUE(setResult.isSuccess());

    auto checkResult = engine_->executeScript(sessionId_, "_event.name + '|' + _event.data").get();
    ASSERT_TRUE(checkResult.isSuccess());
    EXPECT_EQ(checkResult.getValue<std::string>(), "test.event|test_data");

    // Test updating with object data
    auto objectEvent = std::make_shared<RSM::Event>("object.event", "internal");
    objectEvent->setRawJsonData("{\"key\": \"value\", \"number\": 42}");

    auto objectSetResult = engine_->setCurrentEvent(sessionId_, objectEvent).get();
    ASSERT_TRUE(objectSetResult.isSuccess());

    auto objectCheckResult = engine_->executeScript(sessionId_, "_event.data.key + '_' + _event.data.number").get();
    ASSERT_TRUE(objectCheckResult.isSuccess());
    EXPECT_EQ(objectCheckResult.getValue<std::string>(), "value_42");

    // Test updating with array data
    auto arrayEvent = std::make_shared<RSM::Event>("array.event", "internal");
    arrayEvent->setRawJsonData("[1, 2, 3]");

    auto arraySetResult = engine_->setCurrentEvent(sessionId_, arrayEvent).get();
    ASSERT_TRUE(arraySetResult.isSuccess());

    auto arrayCheckResult = engine_->executeScript(sessionId_, "_event.data.length").get();
    ASSERT_TRUE(arrayCheckResult.isSuccess());
    EXPECT_EQ(arrayCheckResult.getValue<double>(), 3.0);
}

// Test event name and type handling via setCurrentEvent API
TEST_F(EventSystemTest, InternalEventNameAndTypeUpdating) {
    // Test setting event name via setCurrentEvent API
    auto loginEvent = std::make_shared<RSM::Event>("user.login", "internal");
    auto nameSetResult = engine_->setCurrentEvent(sessionId_, loginEvent).get();
    ASSERT_TRUE(nameSetResult.isSuccess());

    auto nameResult = engine_->executeScript(sessionId_, "_event.name").get();
    ASSERT_TRUE(nameResult.isSuccess());
    EXPECT_EQ(nameResult.getValue<std::string>(), "user.login");

    // Test setting event type via setCurrentEvent API
    auto platformEvent = std::make_shared<RSM::Event>("platform.event", "platform");
    auto typeSetResult = engine_->setCurrentEvent(sessionId_, platformEvent).get();
    ASSERT_TRUE(typeSetResult.isSuccess());

    auto typeResult = engine_->executeScript(sessionId_, "_event.type").get();
    ASSERT_TRUE(typeResult.isSuccess());
    EXPECT_EQ(typeResult.getValue<std::string>(), "platform");

    // Test complex event names with dots
    auto complexEvent = std::make_shared<RSM::Event>("error.execution.timeout", "internal");
    auto complexSetResult = engine_->setCurrentEvent(sessionId_, complexEvent).get();
    ASSERT_TRUE(complexSetResult.isSuccess());

    auto complexNameResult = engine_->executeScript(sessionId_, "_event.name").get();
    ASSERT_TRUE(complexNameResult.isSuccess());
    EXPECT_EQ(complexNameResult.getValue<std::string>(), "error.execution.timeout");
}

// Test event origin and invocation properties via setCurrentEvent API
TEST_F(EventSystemTest, InternalEventOriginPropertiesUpdating) {
    // Test setting origin via setCurrentEvent API
    auto internalEvent = std::make_shared<RSM::Event>("internal.event", "internal");
    internalEvent->setOrigin("#_internal");
    auto originSetResult = engine_->setCurrentEvent(sessionId_, internalEvent).get();
    ASSERT_TRUE(originSetResult.isSuccess());

    auto originResult = engine_->executeScript(sessionId_, "_event.origin").get();
    ASSERT_TRUE(originResult.isSuccess());
    EXPECT_EQ(originResult.getValue<std::string>(), "#_internal");

    // Test setting origintype via setCurrentEvent API
    auto scxmlEvent = std::make_shared<RSM::Event>("scxml.event", "internal");
    scxmlEvent->setOriginType("http://www.w3.org/TR/scxml/#SCXMLEventProcessor");
    auto origintypeSetResult = engine_->setCurrentEvent(sessionId_, scxmlEvent).get();
    ASSERT_TRUE(origintypeSetResult.isSuccess());

    auto origintypeResult = engine_->executeScript(sessionId_, "_event.origintype").get();
    ASSERT_TRUE(origintypeResult.isSuccess());
    EXPECT_EQ(origintypeResult.getValue<std::string>(), "http://www.w3.org/TR/scxml/#SCXMLEventProcessor");

    // Test setting invokeid via setCurrentEvent API
    auto invokeEvent = std::make_shared<RSM::Event>("invoke.event", "internal");
    invokeEvent->setInvokeId("invoke_123");
    auto invokeidSetResult = engine_->setCurrentEvent(sessionId_, invokeEvent).get();
    ASSERT_TRUE(invokeidSetResult.isSuccess());

    auto invokeidResult = engine_->executeScript(sessionId_, "_event.invokeid").get();
    ASSERT_TRUE(invokeidResult.isSuccess());
    EXPECT_EQ(invokeidResult.getValue<std::string>(), "invoke_123");

    // Test setting sendid via setCurrentEvent API
    auto sendEvent = std::make_shared<RSM::Event>("send.event", "internal");
    sendEvent->setSendId("send_456");
    auto sendidSetResult = engine_->setCurrentEvent(sessionId_, sendEvent).get();
    ASSERT_TRUE(sendidSetResult.isSuccess());

    auto sendidResult = engine_->executeScript(sessionId_, "_event.sendid").get();
    ASSERT_TRUE(sendidResult.isSuccess());
    EXPECT_EQ(sendidResult.getValue<std::string>(), "send_456");
}

// Test event object in expressions
TEST_F(EventSystemTest, EventInExpressions) {
    // Set up event data using setCurrentEvent API
    auto userEvent = std::make_shared<RSM::Event>("user.action", "internal");
    userEvent->setRawJsonData("{\"userId\": 123, \"action\": \"click\"}");
    auto setupResult = engine_->setCurrentEvent(sessionId_, userEvent).get();
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
    // Set up event with complex data using setCurrentEvent API
    auto complexEvent = std::make_shared<RSM::Event>("complex.event", "internal");
    complexEvent->setRawJsonData("{\"user\":{\"id\":1,\"name\":\"test\"},\"items\":[1,2,3]}");
    auto setupResult = engine_->setCurrentEvent(sessionId_, complexEvent).get();
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
    // Set event data using setCurrentEvent API
    auto persistentEvent = std::make_shared<RSM::Event>("persistent.event", "internal");
    persistentEvent->setRawJsonData("\"persistent_data\"");  // JSON string format
    auto setResult = engine_->setCurrentEvent(sessionId_, persistentEvent).get();
    ASSERT_TRUE(setResult.isSuccess());

    // Check event data persists in subsequent evaluations
    auto checkNameResult = engine_->evaluateExpression(sessionId_, "_event.name").get();
    ASSERT_TRUE(checkNameResult.isSuccess());
    EXPECT_EQ(checkNameResult.getValue<std::string>(), "persistent.event");

    auto checkDataResult = engine_->evaluateExpression(sessionId_, "_event.data").get();
    ASSERT_TRUE(checkDataResult.isSuccess());
    EXPECT_EQ(checkDataResult.getValue<std::string>(), "persistent_data");

    // Modify using another setCurrentEvent call
    auto modifiedEvent = std::make_shared<RSM::Event>("persistent.event", "internal");
    modifiedEvent->setRawJsonData("\"modified_data\"");  // JSON string format
    auto modifyResult = engine_->setCurrentEvent(sessionId_, modifiedEvent).get();
    ASSERT_TRUE(modifyResult.isSuccess());

    // Verify modification persists
    auto verifyResult = engine_->evaluateExpression(sessionId_, "_event.data").get();
    ASSERT_TRUE(verifyResult.isSuccess());
    EXPECT_EQ(verifyResult.getValue<std::string>(), "modified_data");
}

// Test SCXML W3C compliant error handling for _event modification attempts
TEST_F(EventSystemTest, W3C_EventModificationErrorHandling) {
    // First verify _event object exists
    auto typeCheckResult = engine_->evaluateExpression(sessionId_, "typeof _event").get();
    ASSERT_TRUE(typeCheckResult.isSuccess());
    EXPECT_EQ(typeCheckResult.getValue<std::string>(), "object");

    // Test that _event properties are enumerable
    auto keysResult = engine_->evaluateExpression(sessionId_, "Object.keys(_event).sort().join(',')").get();
    ASSERT_TRUE(keysResult.isSuccess());
    EXPECT_EQ(keysResult.getValue<std::string>(), "data,invokeid,name,origin,origintype,sendid,type");

    // Test that direct assignment to _event object fails (the object itself should be protected)
    auto directAssignResult =
        engine_->executeScript(sessionId_, "try { _event = {}; 'success'; } catch(e) { 'error: ' + e.message; }").get();
    ASSERT_TRUE(directAssignResult.isSuccess());
    std::string assignResult = directAssignResult.getValue<std::string>();
    EXPECT_TRUE(assignResult.find("error:") == 0 || assignResult.find("Cannot") != std::string::npos)
        << "Direct assignment to _event should fail, got: " << assignResult;

    // Test that delete operations on _event properties fail
    auto deleteResult = engine_->executeScript(sessionId_, "delete _event.name; _event.hasOwnProperty('name')").get();
    ASSERT_TRUE(deleteResult.isSuccess());
    EXPECT_TRUE(deleteResult.getValue<bool>()) << "_event.name property should still exist after delete attempt";
}
