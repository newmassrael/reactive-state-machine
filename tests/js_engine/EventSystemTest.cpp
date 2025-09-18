#include <gtest/gtest.h>
#include <memory>
#include "../../scxml/include/SCXMLEngine.h"
#include "../../scxml/include/SCXMLTypes.h"

using namespace SCXML;

class EventSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = createSCXMLEngine();
        ASSERT_NE(engine, nullptr);
        ASSERT_TRUE(engine->initialize());
        ASSERT_TRUE(engine->createSession("test_session"));
    }

    void TearDown() override {
        if (engine) {
            engine->destroySession("test_session");
            engine->shutdown();
        }
        engine.reset();
    }

    std::unique_ptr<SCXMLEngine> engine;
    const std::string session_id = "test_session";
};

// Test basic event object structure according to SCXML spec
TEST_F(EventSystemTest, BasicEventObject) {
    // Create a basic event
    auto event = std::make_shared<Event>("user.click", "external");
    event->setSendId("send123");
    event->setOrigin("http://example.com");
    event->setOriginType("http");
    event->setInvokeId("invoke456");
    
    // Set current event
    auto set_result = engine->setCurrentEvent(session_id, event).get();
    EXPECT_TRUE(set_result.success);
    
    // Test _event object properties
    auto name_result = engine->executeScript(session_id, "_event.name;").get();
    EXPECT_TRUE(name_result.success);
    EXPECT_EQ(std::get<std::string>(name_result.value), "user.click");
    
    auto type_result = engine->executeScript(session_id, "_event.type;").get();
    EXPECT_TRUE(type_result.success);
    EXPECT_EQ(std::get<std::string>(type_result.value), "external");
    
    auto sendid_result = engine->executeScript(session_id, "_event.sendid;").get();
    EXPECT_TRUE(sendid_result.success);
    EXPECT_EQ(std::get<std::string>(sendid_result.value), "send123");
    
    auto origin_result = engine->executeScript(session_id, "_event.origin;").get();
    EXPECT_TRUE(origin_result.success);
    EXPECT_EQ(std::get<std::string>(origin_result.value), "http://example.com");
    
    auto origintype_result = engine->executeScript(session_id, "_event.origintype;").get();
    EXPECT_TRUE(origintype_result.success);
    EXPECT_EQ(std::get<std::string>(origintype_result.value), "http");
    
    auto invokeid_result = engine->executeScript(session_id, "_event.invokeid;").get();
    EXPECT_TRUE(invokeid_result.success);
    EXPECT_EQ(std::get<std::string>(invokeid_result.value), "invoke456");
}

// Test event data handling
TEST_F(EventSystemTest, EventDataHandling) {
    // Create event with JSON data
    auto event = std::make_shared<Event>("data.test", "external");
    event->setDataFromString(R"({"key": "value", "number": 42, "flag": true})");
    
    auto set_result = engine->setCurrentEvent(session_id, event).get();
    EXPECT_TRUE(set_result.success);
    
    // Access event data properties
    auto key_result = engine->executeScript(session_id, "_event.data.key;").get();
    EXPECT_TRUE(key_result.success);
    EXPECT_EQ(std::get<std::string>(key_result.value), "value");
    
    auto number_result = engine->executeScript(session_id, "_event.data.number;").get();
    EXPECT_TRUE(number_result.success);
    EXPECT_EQ(std::get<double>(number_result.value), 42);
    
    auto flag_result = engine->executeScript(session_id, "_event.data.flag;").get();
    EXPECT_TRUE(flag_result.success);
    EXPECT_TRUE(std::get<bool>(flag_result.value));
}

// Test event with complex nested data
TEST_F(EventSystemTest, ComplexEventData) {
    auto event = std::make_shared<Event>("complex.data", "external");
    event->setDataFromString(R"({
        "user": {
            "id": 123,
            "name": "John Doe",
            "preferences": {
                "theme": "dark",
                "notifications": true
            }
        },
        "actions": ["click", "scroll", "submit"]
    })");
    
    auto set_result = engine->setCurrentEvent(session_id, event).get();
    EXPECT_TRUE(set_result.success);
    
    // Access nested properties
    auto user_id = engine->executeScript(session_id, "_event.data.user.id;").get();
    EXPECT_TRUE(user_id.success);
    EXPECT_EQ(std::get<double>(user_id.value), 123);
    
    auto user_name = engine->executeScript(session_id, "_event.data.user.name;").get();
    EXPECT_TRUE(user_name.success);
    EXPECT_EQ(std::get<std::string>(user_name.value), "John Doe");
    
    auto theme = engine->executeScript(session_id, "_event.data.user.preferences.theme;").get();
    EXPECT_TRUE(theme.success);
    EXPECT_EQ(std::get<std::string>(theme.value), "dark");
    
    auto notifications = engine->executeScript(session_id, "_event.data.user.preferences.notifications;").get();
    EXPECT_TRUE(notifications.success);
    EXPECT_TRUE(std::get<bool>(notifications.value));
    
    // Access array elements
    auto first_action = engine->executeScript(session_id, "_event.data.actions[0];").get();
    EXPECT_TRUE(first_action.success);
    EXPECT_EQ(std::get<std::string>(first_action.value), "click");
    
    auto array_length = engine->executeScript(session_id, "_event.data.actions.length;").get();
    EXPECT_TRUE(array_length.success);
    EXPECT_EQ(std::get<double>(array_length.value), 3);
}

// Test event without data
TEST_F(EventSystemTest, EventWithoutData) {
    auto event = std::make_shared<Event>("simple.event", "internal");
    
    auto set_result = engine->setCurrentEvent(session_id, event).get();
    EXPECT_TRUE(set_result.success);
    
    // Check that data is undefined
    auto data_type = engine->executeScript(session_id, "typeof _event.data;").get();
    EXPECT_TRUE(data_type.success);
    EXPECT_EQ(std::get<std::string>(data_type.value), "undefined");
}

// Test clearing current event
TEST_F(EventSystemTest, ClearCurrentEvent) {
    // Set an event first
    auto event = std::make_shared<Event>("temp.event", "external");
    auto set_result = engine->setCurrentEvent(session_id, event).get();
    EXPECT_TRUE(set_result.success);
    
    // Verify event is set
    auto name_result = engine->executeScript(session_id, "_event.name;").get();
    EXPECT_TRUE(name_result.success);
    EXPECT_EQ(std::get<std::string>(name_result.value), "temp.event");
    
    // Clear event by passing nullptr
    auto clear_result = engine->setCurrentEvent(session_id, nullptr).get();
    EXPECT_TRUE(clear_result.success);
    
    // Verify event is cleared
    auto cleared_name = engine->executeScript(session_id, "_event.name;").get();
    EXPECT_TRUE(cleared_name.success);
    EXPECT_EQ(std::get<std::string>(cleared_name.value), "");
}

// Test event isolation between sessions
TEST_F(EventSystemTest, EventIsolationBetweenSessions) {
    // Create second session
    ASSERT_TRUE(engine->createSession("session2"));
    
    // Set different events in each session
    auto event1 = std::make_shared<Event>("event.session1", "external");
    auto event2 = std::make_shared<Event>("event.session2", "internal");
    
    auto set1_result = engine->setCurrentEvent("test_session", event1).get();
    auto set2_result = engine->setCurrentEvent("session2", event2).get();
    
    EXPECT_TRUE(set1_result.success);
    EXPECT_TRUE(set2_result.success);
    
    // Verify isolation
    auto name1 = engine->executeScript("test_session", "_event.name;").get();
    auto name2 = engine->executeScript("session2", "_event.name;").get();
    
    EXPECT_TRUE(name1.success);
    EXPECT_TRUE(name2.success);
    EXPECT_EQ(std::get<std::string>(name1.value), "event.session1");
    EXPECT_EQ(std::get<std::string>(name2.value), "event.session2");
    
    // Clean up
    engine->destroySession("session2");
}

// Test event name patterns (SCXML naming conventions)
TEST_F(EventSystemTest, EventNamePatterns) {
    // Test various SCXML event name patterns
    std::vector<std::string> event_names = {
        "done.state.state1",
        "done.invoke.id1",
        "error.execution",
        "error.communication",
        "user.click.button1",
        "timer.timeout",
        "http.success",
        "custom.my_event"
    };
    
    for (const auto& event_name : event_names) {
        auto event = std::make_shared<Event>(event_name, "external");
        auto set_result = engine->setCurrentEvent(session_id, event).get();
        EXPECT_TRUE(set_result.success) << "Failed to set event: " << event_name;
        
        auto name_result = engine->executeScript(session_id, "_event.name;").get();
        EXPECT_TRUE(name_result.success);
        EXPECT_EQ(std::get<std::string>(name_result.value), event_name);
    }
}

// Test event type validation
TEST_F(EventSystemTest, EventTypeValidation) {
    std::vector<std::string> event_types = {
        "internal", "external", "platform"
    };
    
    for (const auto& event_type : event_types) {
        auto event = std::make_shared<Event>("test.event", event_type);
        auto set_result = engine->setCurrentEvent(session_id, event).get();
        EXPECT_TRUE(set_result.success) << "Failed to set event type: " << event_type;
        
        auto type_result = engine->executeScript(session_id, "_event.type;").get();
        EXPECT_TRUE(type_result.success);
        EXPECT_EQ(std::get<std::string>(type_result.value), event_type);
    }
}

// Test invalid JSON data handling
TEST_F(EventSystemTest, InvalidJsonDataHandling) {
    auto event = std::make_shared<Event>("invalid.json", "external");
    event->setDataFromString("{ invalid json }");
    
    auto set_result = engine->setCurrentEvent(session_id, event).get();
    EXPECT_TRUE(set_result.success);
    
    // Should have undefined data for invalid JSON
    auto data_type = engine->executeScript(session_id, "typeof _event.data;").get();
    EXPECT_TRUE(data_type.success);
    EXPECT_EQ(std::get<std::string>(data_type.value), "undefined");
}

// Test event data modification from JavaScript
TEST_F(EventSystemTest, EventDataModification) {
    auto event = std::make_shared<Event>("modifiable.event", "external");
    event->setDataFromString(R"({"counter": 0, "items": []})");
    
    auto set_result = engine->setCurrentEvent(session_id, event).get();
    EXPECT_TRUE(set_result.success);
    
    // Modify event data through JavaScript
    auto modify_result = engine->executeScript(session_id, 
        "_event.data.counter = 5; _event.data.items.push('item1'); _event.data.counter;").get();
    EXPECT_TRUE(modify_result.success);
    EXPECT_EQ(std::get<double>(modify_result.value), 5);
    
    // Verify modifications
    auto counter_result = engine->executeScript(session_id, "_event.data.counter;").get();
    EXPECT_TRUE(counter_result.success);
    EXPECT_EQ(std::get<double>(counter_result.value), 5);
    
    auto items_length = engine->executeScript(session_id, "_event.data.items.length;").get();
    EXPECT_TRUE(items_length.success);
    EXPECT_EQ(std::get<double>(items_length.value), 1);
    
    auto first_item = engine->executeScript(session_id, "_event.data.items[0];").get();
    EXPECT_TRUE(first_item.success);
    EXPECT_EQ(std::get<std::string>(first_item.value), "item1");
}

// Test system variables setup with events
TEST_F(EventSystemTest, SystemVariablesWithEvents) {
    // Setup system variables
    std::vector<std::string> ioProcessors = {"scxml", "basichttp", "custom"};
    auto setup_result = engine->setupSystemVariables(session_id, "TestStateMachine", ioProcessors).get();
    EXPECT_TRUE(setup_result.success);
    
    // Set an event
    auto event = std::make_shared<Event>("system.test", "external");
    auto set_result = engine->setCurrentEvent(session_id, event).get();
    EXPECT_TRUE(set_result.success);
    
    // Verify both system variables and event are accessible
    auto sessionid_result = engine->executeScript(session_id, "_sessionid;").get();
    EXPECT_TRUE(sessionid_result.success);
    EXPECT_EQ(std::get<std::string>(sessionid_result.value), "test_session");
    
    auto name_result = engine->executeScript(session_id, "_name;").get();
    EXPECT_TRUE(name_result.success);
    EXPECT_EQ(std::get<std::string>(name_result.value), "TestStateMachine");
    
    auto event_name_result = engine->executeScript(session_id, "_event.name;").get();
    EXPECT_TRUE(event_name_result.success);
    EXPECT_EQ(std::get<std::string>(event_name_result.value), "system.test");
    
    auto ioprocessors_length = engine->executeScript(session_id, "_ioprocessors.length;").get();
    EXPECT_TRUE(ioprocessors_length.success);
    EXPECT_EQ(std::get<double>(ioprocessors_length.value), 3);
}