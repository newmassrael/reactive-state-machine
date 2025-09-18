#include <iostream>
#include <gtest/gtest.h>
#include "scripting/JSEngine.h"

using namespace RSM;

TEST(DebugTest, BasicJavaScriptExecution) {
    // Create and initialize engine
    auto& engine = JSEngine::instance();
    ASSERT_TRUE(engine.initialize());
    
    // Create test session
    std::string sessionId = "debug_session";
    bool createResult = engine.createSession(sessionId, "");
    ASSERT_TRUE(createResult) << "Failed to create session";
    
    // Test basic arithmetic
    auto result = engine.evaluateExpression(sessionId, "2 + 3").get();
    ASSERT_TRUE(result.isSuccess()) << "Failed to evaluate expression: " << result.errorMessage;
    EXPECT_EQ(result.getValue<double>(), 5.0);
    
    // Cleanup
    engine.destroySession(sessionId);
    engine.shutdown();
}

TEST(DebugTest, VariableAssignmentAndRetrieval) {
    auto& engine = JSEngine::instance();
    ASSERT_TRUE(engine.initialize());
    
    std::string sessionId = "debug_var_session";
    bool createResult = engine.createSession(sessionId, "");
    ASSERT_TRUE(createResult);
    
    // Assign variable
    auto assignResult = engine.executeScript(sessionId, "var testVar = 'Hello World'; testVar").get();
    ASSERT_TRUE(assignResult.isSuccess());
    EXPECT_EQ(assignResult.getValue<std::string>(), "Hello World");
    
    // Retrieve variable
    auto retrieveResult = engine.evaluateExpression(sessionId, "testVar").get();
    ASSERT_TRUE(retrieveResult.isSuccess());
    EXPECT_EQ(retrieveResult.getValue<std::string>(), "Hello World");
    
    // Cleanup
    engine.destroySession(sessionId);
    engine.shutdown();
}

TEST(DebugTest, SCXMLBuiltinFunctions) {
    auto& engine = JSEngine::instance();
    ASSERT_TRUE(engine.initialize());
    
    std::string sessionId = "debug_builtin_session";
    bool createResult = engine.createSession(sessionId, "");
    ASSERT_TRUE(createResult);
    
    // Test In() function exists
    auto inTypeResult = engine.evaluateExpression(sessionId, "typeof In").get();
    ASSERT_TRUE(inTypeResult.isSuccess());
    EXPECT_EQ(inTypeResult.getValue<std::string>(), "function");
    
    // Test console exists
    auto consoleTypeResult = engine.evaluateExpression(sessionId, "typeof console").get();
    ASSERT_TRUE(consoleTypeResult.isSuccess());
    EXPECT_EQ(consoleTypeResult.getValue<std::string>(), "object");
    
    // Test console.log exists
    auto logTypeResult = engine.evaluateExpression(sessionId, "typeof console.log").get();
    ASSERT_TRUE(logTypeResult.isSuccess());
    EXPECT_EQ(logTypeResult.getValue<std::string>(), "function");
    
    // Test Math exists
    auto mathTypeResult = engine.evaluateExpression(sessionId, "typeof Math").get();
    ASSERT_TRUE(mathTypeResult.isSuccess());
    EXPECT_EQ(mathTypeResult.getValue<std::string>(), "object");
    
    // Cleanup
    engine.destroySession(sessionId);
    engine.shutdown();
}

TEST(DebugTest, SystemVariables) {
    auto& engine = JSEngine::instance();
    ASSERT_TRUE(engine.initialize());
    
    std::string sessionId = "debug_sysvar_session";
    bool createResult = engine.createSession(sessionId, "");
    ASSERT_TRUE(createResult);
    
    // Test _sessionid exists and is string
    auto sessionIdTypeResult = engine.evaluateExpression(sessionId, "typeof _sessionid").get();
    ASSERT_TRUE(sessionIdTypeResult.isSuccess());
    EXPECT_EQ(sessionIdTypeResult.getValue<std::string>(), "string");
    
    // Test _name exists and is string
    auto nameTypeResult = engine.evaluateExpression(sessionId, "typeof _name").get();
    ASSERT_TRUE(nameTypeResult.isSuccess());
    EXPECT_EQ(nameTypeResult.getValue<std::string>(), "string");
    
    // Test _ioprocessors exists and is object
    auto ioTypeResult = engine.evaluateExpression(sessionId, "typeof _ioprocessors").get();
    ASSERT_TRUE(ioTypeResult.isSuccess());
    EXPECT_EQ(ioTypeResult.getValue<std::string>(), "object");
    
    // Test _event exists and is object
    auto eventTypeResult = engine.evaluateExpression(sessionId, "typeof _event").get();
    ASSERT_TRUE(eventTypeResult.isSuccess());
    EXPECT_EQ(eventTypeResult.getValue<std::string>(), "object");
    
    // Cleanup
    engine.destroySession(sessionId);
    engine.shutdown();
}

TEST(DebugTest, ErrorHandling) {
    auto& engine = JSEngine::instance();
    ASSERT_TRUE(engine.initialize());
    
    std::string sessionId = "debug_error_session";
    bool createResult = engine.createSession(sessionId, "");
    ASSERT_TRUE(createResult);
    
    // Test syntax error handling
    auto syntaxErrorResult = engine.evaluateExpression(sessionId, "var x = ;").get();
    EXPECT_FALSE(syntaxErrorResult.isSuccess()) << "Syntax error should be caught";
    
    // Test reference error handling
    auto refErrorResult = engine.evaluateExpression(sessionId, "undefinedVariable").get();
    EXPECT_FALSE(refErrorResult.isSuccess()) << "Reference error should be caught";
    
    // Test that engine continues to work after errors
    auto workingResult = engine.evaluateExpression(sessionId, "1 + 1").get();
    ASSERT_TRUE(workingResult.isSuccess()) << "Engine should continue working after errors";
    EXPECT_EQ(workingResult.getValue<double>(), 2.0);
    
    // Cleanup
    engine.destroySession(sessionId);
    engine.shutdown();
}

TEST(DebugTest, ComplexExpressions) {
    auto& engine = JSEngine::instance();
    ASSERT_TRUE(engine.initialize());
    
    std::string sessionId = "debug_complex_session";
    bool createResult = engine.createSession(sessionId, "");
    ASSERT_TRUE(createResult);
    
    // Test complex expression with system variables
    auto complexResult = engine.evaluateExpression(sessionId, 
        "_name.length > 0 && typeof _sessionid === 'string' && Math.max(1, 2) === 2").get();
    ASSERT_TRUE(complexResult.isSuccess());
    EXPECT_TRUE(complexResult.getValue<bool>());
    
    // Test function definition and execution
    auto functionResult = engine.executeScript(sessionId, 
        "function factorial(n) { return n <= 1 ? 1 : n * factorial(n - 1); } factorial(5)").get();
    ASSERT_TRUE(functionResult.isSuccess());
    EXPECT_EQ(functionResult.getValue<double>(), 120.0);
    
    // Test object manipulation
    auto objectResult = engine.executeScript(sessionId, 
        "var obj = {a: 1, b: {c: 2}}; obj.b.c + obj.a").get();
    ASSERT_TRUE(objectResult.isSuccess());
    EXPECT_EQ(objectResult.getValue<double>(), 3.0);
    
    // Cleanup
    engine.destroySession(sessionId);
    engine.shutdown();
}

TEST(DebugTest, ConsoleLogging) {
    auto& engine = JSEngine::instance();
    ASSERT_TRUE(engine.initialize());
    
    std::string sessionId = "debug_console_session";
    bool createResult = engine.createSession(sessionId, "");
    ASSERT_TRUE(createResult);
    
    // Test console.log doesn't crash and returns undefined
    auto logResult = engine.executeScript(sessionId, "console.log('Debug test message'); 'completed'").get();
    ASSERT_TRUE(logResult.isSuccess()) << "console.log should not crash";
    EXPECT_EQ(logResult.getValue<std::string>(), "completed");
    
    // Test console.log with multiple arguments
    auto multiLogResult = engine.executeScript(sessionId, 
        "console.log('Multiple', 'arguments', 123, true); 'multi_completed'").get();
    ASSERT_TRUE(multiLogResult.isSuccess());
    EXPECT_EQ(multiLogResult.getValue<std::string>(), "multi_completed");
    
    // Cleanup
    engine.destroySession(sessionId);
    engine.shutdown();
}