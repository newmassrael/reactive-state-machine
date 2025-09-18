#include "../include/SCXMLEngine.h"
#include "../include/SCXMLTypes.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace SCXML;

/**
 * @brief Example demonstrating session-based JavaScript engine usage
 */
void demonstrateSessionIsolation() {
    std::cout << "\n=== SCXML Engine Session Isolation Demo ===" << std::endl;

    auto engine = createSCXMLEngine();

    // Initialize engine
    if (!engine->initialize()) {
        std::cerr << "Failed to initialize SCXML Engine" << std::endl;
        return;
    }

    // Create two independent sessions
    std::cout << "\n1. Creating sessions..." << std::endl;
    engine->createSession("main");
    engine->createSession("child1");

    // Set different variables in each session
    std::cout << "\n2. Setting session-specific variables..." << std::endl;
    engine->setVariable("main", "temperature", 25.0).get();
    engine->setVariable("main", "location", std::string("room")).get();

    engine->setVariable("child1", "temperature", 30.0).get();
    engine->setVariable("child1", "location", std::string("kitchen")).get();

    // Setup system variables for each session
    std::cout << "\n3. Setting up SCXML system variables..." << std::endl;
    engine->setupSystemVariables("main", "MainStateMachine", {"http", "websocket"}).get();
    engine->setupSystemVariables("child1", "ChildStateMachine", {"http"}).get();

    // Test variable isolation
    std::cout << "\n4. Testing variable isolation..." << std::endl;

    auto mainTemp = engine->getVariable("main", "temperature").get();
    auto child1Temp = engine->getVariable("child1", "temperature").get();

    std::cout << "Main session temperature: " << mainTemp.getValueAsString() << std::endl;
    std::cout << "Child1 session temperature: " << child1Temp.getValueAsString() << std::endl;

    // Test script execution isolation
    std::cout << "\n5. Testing script execution isolation..." << std::endl;

    engine->executeScript("main", "var result = temperature + 5; var status = 'warm';").get();
    engine->executeScript("child1", "var result = temperature * 2; var status = 'hot';").get();

    auto mainResult = engine->getVariable("main", "result").get();
    auto child1Result = engine->getVariable("child1", "result").get();
    auto mainStatus = engine->getVariable("main", "status").get();
    auto child1Status = engine->getVariable("child1", "status").get();

    std::cout << "Main session - result: " << mainResult.getValueAsString()
              << ", status: " << mainStatus.getValueAsString() << std::endl;
    std::cout << "Child1 session - result: " << child1Result.getValueAsString()
              << ", status: " << child1Status.getValueAsString() << std::endl;

    // Test SCXML event handling
    std::cout << "\n6. Testing SCXML event handling..." << std::endl;

    // Create different events for each session
    auto mainEvent = std::make_shared<Event>("temperature.changed", "internal");
    mainEvent->setDataFromString("\"25.5\"");
    
    auto child1Event = std::make_shared<Event>("timer.expired", "platform");
    child1Event->setDataFromString("\"timeout\"");  // JSON string format

    engine->setCurrentEvent("main", mainEvent).get();
    engine->setCurrentEvent("child1", child1Event).get();

    // Test event access from JavaScript
    auto mainEventName = engine->evaluateExpression("main", "_event.name").get();
    auto mainEventData = engine->evaluateExpression("main", "_event.data").get();
    auto child1EventName = engine->evaluateExpression("child1", "_event.name").get();
    auto child1EventData = engine->evaluateExpression("child1", "_event.data").get();

    std::cout << "Main session event: " << mainEventName.getValueAsString()
              << " with data: " << mainEventData.getValueAsString() << std::endl;
    std::cout << "Child1 session event: " << child1EventName.getValueAsString()
              << " with data: " << child1EventData.getValueAsString() << std::endl;

    // Test system variables
    std::cout << "\n7. Testing system variables..." << std::endl;

    auto mainSessionId = engine->evaluateExpression("main", "_sessionid").get();
    auto mainSessionName = engine->evaluateExpression("main", "_name").get();
    auto child1SessionId = engine->evaluateExpression("child1", "_sessionid").get();
    auto child1SessionName = engine->evaluateExpression("child1", "_name").get();

    std::cout << "Main session: id=" << mainSessionId.getValueAsString()
              << ", name=" << mainSessionName.getValueAsString() << std::endl;
    std::cout << "Child1 session: id=" << child1SessionId.getValueAsString()
              << ", name=" << child1SessionName.getValueAsString() << std::endl;

    // Test conditional expressions (typical SCXML usage)
    std::cout << "\n8. Testing conditional expressions..." << std::endl;

    auto mainCondition = engine->evaluateExpression("main", "temperature > 20 && _event.name === 'temperature.changed'").get();
    auto child1Condition = engine->evaluateExpression("child1", "temperature > 25 && _event.name === 'timer.expired'").get();

    std::cout << "Main condition result: " << mainCondition.getValueAsString() << std::endl;
    std::cout << "Child1 condition result: " << child1Condition.getValueAsString() << std::endl;

    // Cleanup
    std::cout << "\n9. Cleaning up..." << std::endl;
    engine->destroySession("child1");
    engine->destroySession("main");

    auto activeSessions = engine->getActiveSessions();
    std::cout << "Active sessions remaining: " << activeSessions.size() << std::endl;

    engine->shutdown();
    std::cout << "\nDemo completed successfully!" << std::endl;
}

/**
 * @brief Example demonstrating thread safety
 */
void demonstrateThreadSafety() {
    std::cout << "\n=== SCXML Engine Thread Safety Demo ===" << std::endl;

    auto engine = createSCXMLEngine();
    engine->initialize();

    engine->createSession("thread_test");
    engine->setVariable("thread_test", "counter", 0).get();

    std::cout << "Starting 5 threads to increment counter concurrently..." << std::endl;

    std::vector<std::thread> threads;
    const int numThreads = 5;
    const int incrementsPerThread = 10;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&engine, i, incrementsPerThread]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                // Each thread increments the counter
                auto script = "counter = counter + 1; counter;";
                auto result = engine->executeScript("thread_test", script).get();

                std::cout << "Thread " << i << " increment " << j
                          << " -> counter = " << result.getValueAsString() << std::endl;

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    auto finalCounter = engine->getVariable("thread_test", "counter").get();
    std::cout << "Final counter value: " << finalCounter.getValueAsString()
              << " (expected: " << (numThreads * incrementsPerThread) << ")" << std::endl;

    engine->destroySession("thread_test");
    engine->shutdown();
}

int main() {
    std::cout << "SCXML Engine Example Application" << std::endl;
    
    auto engine = createSCXMLEngine();
    std::cout << "Engine Info: " << engine->getEngineInfo() << std::endl;

    try {
        demonstrateSessionIsolation();

        std::cout << "\n" << std::string(50, '=') << std::endl;

        demonstrateThreadSafety();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}