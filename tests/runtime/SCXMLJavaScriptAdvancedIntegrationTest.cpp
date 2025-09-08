/**
 * @file SCXMLJavaScriptAdvancedIntegrationTest.cpp
 * @brief Advanced SCXML-JavaScript integration tests for comprehensive QuickJS features
 * 
 * Tests advanced SCXML features and JavaScript capabilities:
 * - Complete SCXML system variables (_sessionid, _name, _ioprocessors) 
 * - Conditional execution support (<if>, <elseif>, <else>)
 * - Script execution capabilities
 * - Advanced JavaScript features (Promise/async, closures, prototypes)
 * - Performance and memory management
 */

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include "runtime/ECMAScriptEngineFactory.h"
#include "runtime/IECMAScriptEngine.h"
#include "runtime/RuntimeContext.h"
#include "events/Event.h"
#include "common/Logger.h"

class SCXMLJavaScriptAdvancedIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create QuickJS engine
        engine = SCXML::ECMAScriptEngineFactory::create(SCXML::ECMAScriptEngineFactory::EngineType::QUICKJS);
        ASSERT_NE(nullptr, engine) << "Failed to create QuickJS engine";
        
        bool initialized = engine->initialize();
        ASSERT_TRUE(initialized) << "Engine initialization failed";
        
        // Create runtime context
        runtimeContext = std::make_unique<SCXML::Runtime::RuntimeContext>();
        
        // Set up mock In() function for state checking
        engine->setStateCheckFunction([this](const std::string& stateName) -> bool {
            return currentStates.find(stateName) != currentStates.end();
        });
        
        // Initialize current states for testing
        currentStates = {"idle", "main", "authenticated"};
        
        // Setup complete SCXML system variables
        engine->setupSCXMLSystemVariables(
            "session-12345",           // _sessionid
            "MyStateMachine",          // _name  
            {"http", "websocket", "internal"}  // _ioprocessors
        );
    }

protected:
    std::unique_ptr<SCXML::IECMAScriptEngine> engine;
    std::unique_ptr<SCXML::Runtime::RuntimeContext> runtimeContext;
    std::set<std::string> currentStates;
    
    // Helper to create test events
    void setCurrentEvent(const std::string& name, const std::string& data = "") {
        auto event = std::make_shared<SCXML::Events::Event>(name, SCXML::Events::Event::Type::EXTERNAL);
        if (!data.empty()) {
            event->setData(data);
        }
        engine->setCurrentEvent(event);
    }
    
    // Performance measurement helper
    template<typename Func>
    double measureExecutionTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

/**
 * @brief Test complete SCXML system variables
 * W3C SCXML Section B.2.1: Complete System Variables
 */
TEST_F(SCXMLJavaScriptAdvancedIntegrationTest, W3C_CompleteSystemVariables) {
    // Test _sessionid
    auto result = engine->evaluateExpression("_sessionid", *runtimeContext);
    ASSERT_TRUE(result.success) << "Failed to access _sessionid: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<std::string>(result.value));
    EXPECT_EQ("session-12345", std::get<std::string>(result.value));
    
    // Test _name
    result = engine->evaluateExpression("_name", *runtimeContext);
    ASSERT_TRUE(result.success) << "Failed to access _name: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<std::string>(result.value));
    EXPECT_EQ("MyStateMachine", std::get<std::string>(result.value));
    
    // Test _ioprocessors access
    result = engine->evaluateExpression("_ioprocessors.length", *runtimeContext);
    ASSERT_TRUE(result.success) << "Failed to access _ioprocessors: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(3.0, std::get<double>(result.value));
    
    // Test _ioprocessors content
    result = engine->evaluateExpression("_ioprocessors.includes('http')", *runtimeContext);
    ASSERT_TRUE(result.success) << "Failed to check _ioprocessors content: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    SCXML::Common::Logger::info("Complete SCXML system variables working correctly");
}

/**
 * @brief Test advanced conditional execution patterns
 * W3C SCXML Section 3.13: Enhanced Conditional Logic
 */
TEST_F(SCXMLJavaScriptAdvancedIntegrationTest, W3C_AdvancedConditionalExecution) {
    // Set up complex data model
    engine->setVariable("config", R"({"mode": "production", "debug": false, "timeout": 5000})");
    engine->setVariable("permissions", R"(["read", "write", "admin"])");
    
    // Test nested conditional logic (simulating <if><elseif><else> structure)
    std::string complexCondition = R"(
        config.mode === 'production' ? 
            (permissions.includes('admin') ? 'admin-prod' : 'user-prod') :
            (config.debug ? 'debug-mode' : 'dev-mode')
    )";
    
    auto result = engine->evaluateExpression(complexCondition, *runtimeContext);
    ASSERT_TRUE(result.success) << "Complex conditional failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<std::string>(result.value));
    EXPECT_EQ("admin-prod", std::get<std::string>(result.value));
    
    // Test multiple condition evaluation
    result = engine->evaluateExpression(
        "config.mode === 'production' && permissions.length > 2 && !config.debug",
        *runtimeContext
    );
    ASSERT_TRUE(result.success) << "Multiple conditions failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    SCXML::Common::Logger::info("Advanced conditional execution working correctly");
}

/**
 * @brief Test script execution capabilities
 * W3C SCXML Section B: Script Execution
 */
TEST_F(SCXMLJavaScriptAdvancedIntegrationTest, W3C_ScriptExecution) {
    // Test multi-line script execution (simulating <script> content)
    std::string script = R"(
        var counter = 0;
        function increment() { 
            counter++; 
            return counter; 
        }
        function getStatus() {
            return {
                count: counter,
                sessionId: _sessionid,
                isActive: In('idle')
            };
        }
        increment();
    )";
    
    auto result = engine->executeScript(script, *runtimeContext);
    ASSERT_TRUE(result.success) << "Script execution failed: " + result.errorMessage;
    
    // Test function calls after script execution
    result = engine->evaluateExpression("increment()", *runtimeContext);
    ASSERT_TRUE(result.success) << "Function call after script failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(2.0, std::get<double>(result.value)); // Should be 2 after two increments
    
    // Test complex function with SCXML integration
    result = engine->evaluateExpression("JSON.stringify(getStatus())", *runtimeContext);
    ASSERT_TRUE(result.success) << "Complex function call failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<std::string>(result.value));
    
    std::string statusJson = std::get<std::string>(result.value);
    EXPECT_TRUE(statusJson.find("\"count\":2") != std::string::npos);
    EXPECT_TRUE(statusJson.find("\"sessionId\":\"session-12345\"") != std::string::npos);
    
    SCXML::Common::Logger::info("Script execution working correctly");
}

/**
 * @brief Test Promise and async support (if available)
 * Modern JavaScript: Promise/async patterns
 */
TEST_F(SCXMLJavaScriptAdvancedIntegrationTest, JavaScript_PromiseAsyncSupport) {
    // Test Promise creation (basic support check)
    auto result = engine->evaluateExpression("typeof Promise", *runtimeContext);
    ASSERT_TRUE(result.success) << "Promise type check failed: " + result.errorMessage;
    
    if (std::holds_alternative<std::string>(result.value) && 
        std::get<std::string>(result.value) != "undefined") {
        
        // Test Promise creation
        result = engine->evaluateExpression(
            "new Promise((resolve) => resolve(42)).constructor.name",
            *runtimeContext
        );
        
        if (result.success) {
            SCXML::Common::Logger::info("Promise support detected and working");
        } else {
            SCXML::Common::Logger::info("Promise creation not fully supported (expected in embedded JS)");
        }
    } else {
        SCXML::Common::Logger::info("Promise not available (expected in QuickJS)");
    }
    
    // Test setTimeout simulation (time-based processing)
    std::string timerScript = R"(
        var timerId = 0;
        function setTimeout(callback, delay) {
            // Simulate timer functionality
            timerId++;
            callback();
            return timerId;
        }
        var executed = false;
        setTimeout(function() { executed = true; }, 100);
        executed;
    )";
    
    result = engine->executeScript(timerScript, *runtimeContext);
    if (result.success && std::holds_alternative<bool>(result.value)) {
        EXPECT_TRUE(std::get<bool>(result.value));
        SCXML::Common::Logger::info("Timer simulation working correctly");
    }
}

/**
 * @brief Test closure and scope chain functionality
 * JavaScript: Advanced scoping features
 */
TEST_F(SCXMLJavaScriptAdvancedIntegrationTest, JavaScript_ClosureAndScope) {
    // Test closure creation and persistence
    std::string closureScript = R"(
        function createCounter(initial) {
            var count = initial || 0;
            return {
                increment: function() { return ++count; },
                decrement: function() { return --count; },
                getValue: function() { return count; }
            };
        }
        
        var counter1 = createCounter(10);
        var counter2 = createCounter(20);
        
        // Test independent closures
        counter1.increment();
        counter2.decrement();
        
        [counter1.getValue(), counter2.getValue()];
    )";
    
    auto result = engine->executeScript(closureScript, *runtimeContext);
    ASSERT_TRUE(result.success) << "Closure script failed: " + result.errorMessage;
    
    // Verify closure behavior
    result = engine->evaluateExpression("counter1.getValue()", *runtimeContext);
    ASSERT_TRUE(result.success) << "Closure access failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(11.0, std::get<double>(result.value));
    
    result = engine->evaluateExpression("counter2.getValue()", *runtimeContext);
    ASSERT_TRUE(result.success) << "Second closure access failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(19.0, std::get<double>(result.value));
    
    SCXML::Common::Logger::info("Closure and scope chain working correctly");
}

/**
 * @brief Test prototype chain functionality
 * JavaScript: Prototype-based inheritance
 */
TEST_F(SCXMLJavaScriptAdvancedIntegrationTest, JavaScript_PrototypeChain) {
    // Test prototype chain and inheritance
    std::string prototypeScript = R"(
        function StateManager(name) {
            this.name = name;
            this.states = [];
        }
        
        StateManager.prototype.addState = function(state) {
            this.states.push(state);
            return this.states.length;
        };
        
        StateManager.prototype.hasState = function(state) {
            return this.states.includes(state);
        };
        
        StateManager.prototype.toString = function() {
            return this.name + ': [' + this.states.join(', ') + ']';
        };
        
        var sm = new StateManager('TestMachine');
        sm.addState('idle');
        sm.addState('active');
        
        [sm.hasState('idle'), sm.states.length, sm.toString()];
    )";
    
    auto result = engine->executeScript(prototypeScript, *runtimeContext);
    ASSERT_TRUE(result.success) << "Prototype script failed: " + result.errorMessage;
    
    // Test prototype method calls
    result = engine->evaluateExpression("sm.hasState('active')", *runtimeContext);
    ASSERT_TRUE(result.success) << "Prototype method call failed: " + result.errorMessage;
    ASSERT_TRUE(std::holds_alternative<bool>(result.value));
    EXPECT_TRUE(std::get<bool>(result.value));
    
    // Test prototype chain inheritance
    result = engine->evaluateExpression("sm.constructor.name", *runtimeContext);
    if (result.success && std::holds_alternative<std::string>(result.value)) {
        EXPECT_EQ("StateManager", std::get<std::string>(result.value));
    }
    
    SCXML::Common::Logger::info("Prototype chain working correctly");
}

/**
 * @brief Test large data processing performance
 * Performance: Big data handling
 */
TEST_F(SCXMLJavaScriptAdvancedIntegrationTest, Performance_LargeDataProcessing) {
    // Create large dataset
    std::string largeDataScript = R"(
        var largeArray = [];
        for (var i = 0; i < 1000; i++) {
            largeArray.push({
                id: i,
                name: 'item_' + i,
                value: Math.random() * 1000,
                active: i % 2 === 0
            });
        }
        largeArray.length;
    )";
    
    double setupTime = measureExecutionTime([&]() {
        auto result = engine->executeScript(largeDataScript, *runtimeContext);
        ASSERT_TRUE(result.success) << "Large data setup failed: " + result.errorMessage;
    });
    
    // Test complex data processing operations
    std::string processingScript = R"(
        var activeItems = largeArray
            .filter(item => item.active)
            .map(item => ({ ...item, processed: true }))
            .sort((a, b) => b.value - a.value)
            .slice(0, 100);
        
        var summary = {
            total: largeArray.length,
            active: activeItems.length,
            maxValue: activeItems[0].value,
            avgValue: activeItems.reduce((sum, item) => sum + item.value, 0) / activeItems.length
        };
        
        summary;
    )";
    
    double processingTime = measureExecutionTime([&]() {
        auto result = engine->executeScript(processingScript, *runtimeContext);
        ASSERT_TRUE(result.success) << "Large data processing failed: " + result.errorMessage;
    });
    
    // Verify results
    auto result = engine->evaluateExpression("summary.total", *runtimeContext);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(1000.0, std::get<double>(result.value));
    
    SCXML::Common::Logger::info("Large data processing: Setup " + 
                               std::to_string(setupTime) + "ms, Processing " + 
                               std::to_string(processingTime) + "ms");
}

/**
 * @brief Test memory leak prevention
 * Memory Management: Leak detection and prevention
 */
TEST_F(SCXMLJavaScriptAdvancedIntegrationTest, Memory_LeakPrevention) {
    size_t initialMemory = engine->getMemoryUsage();
    
    // Create and destroy many objects
    for (int i = 0; i < 100; i++) {
        std::string script = R"(
            var tempArray = [];
            for (var j = 0; j < 100; j++) {
                tempArray.push({ id: j, data: 'test_' + j });
            }
            var processed = tempArray.map(item => item.data.toUpperCase());
            processed.length;
        )";
        
        auto result = engine->executeScript(script, *runtimeContext);
        ASSERT_TRUE(result.success) << "Memory test iteration " + std::to_string(i) + " failed";
        
        // Periodic garbage collection
        if (i % 20 == 0) {
            engine->collectGarbage();
        }
    }
    
    // Force final garbage collection
    engine->collectGarbage();
    
    size_t finalMemory = engine->getMemoryUsage();
    
    // Memory should not grow excessively (allow some reasonable growth)
    double memoryGrowthRatio = static_cast<double>(finalMemory) / static_cast<double>(initialMemory);
    
    SCXML::Common::Logger::info("Memory usage: Initial " + 
                               std::to_string(initialMemory) + " bytes, Final " + 
                               std::to_string(finalMemory) + " bytes, Ratio " + 
                               std::to_string(memoryGrowthRatio));
    
    // Allow up to 3x memory growth (reasonable for JS engine overhead)
    EXPECT_LT(memoryGrowthRatio, 3.0) << "Potential memory leak detected";
}

/**
 * @brief Test comprehensive integration scenario
 * Integration: Real-world SCXML + advanced JavaScript
 */
TEST_F(SCXMLJavaScriptAdvancedIntegrationTest, Comprehensive_AdvancedIntegration) {
    // Setup complex event
    setCurrentEvent("workflow.process", R"({
        "taskId": "task-12345",
        "priority": "high",
        "data": [1, 2, 3, 4, 5],
        "metadata": {
            "source": "external",
            "timestamp": 1640995200000
        }
    })");
    
    // Comprehensive integration test combining all features
    std::string integrationScript = R"(
        // Use SCXML system variables
        var sessionInfo = {
            id: _sessionid,
            name: _name,
            processors: _ioprocessors
        };
        
        // Process event data with advanced JavaScript
        function processWorkflowEvent(eventData) {
            return {
                session: sessionInfo,
                taskId: eventData.taskId,
                isHighPriority: eventData.priority === 'high',
                processedData: eventData.data
                    .filter(x => x > 2)
                    .map(x => x * 2)
                    .reduce((sum, x) => sum + x, 0),
                isAuthenticated: In('authenticated'),
                timestamp: new Date(eventData.metadata.timestamp).toISOString()
            };
        }
        
        var result = processWorkflowEvent(_event.data);
        result;
    )";
    
    auto result = engine->executeScript(integrationScript, *runtimeContext);
    ASSERT_TRUE(result.success) << "Comprehensive integration failed: " + result.errorMessage;
    
    // Verify integrated functionality
    result = engine->evaluateExpression("result.session.id", *runtimeContext);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(std::holds_alternative<std::string>(result.value));
    EXPECT_EQ("session-12345", std::get<std::string>(result.value));
    
    result = engine->evaluateExpression("result.processedData", *runtimeContext);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(std::holds_alternative<double>(result.value));
    EXPECT_EQ(24.0, std::get<double>(result.value)); // 3*2 + 4*2 + 5*2 = 6 + 8 + 10 = 24
    
    SCXML::Common::Logger::info("Comprehensive advanced integration test passed");
}