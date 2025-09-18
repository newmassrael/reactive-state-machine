#include <gtest/gtest.h>
#include <iostream>
#include "../../scxml/include/SCXMLEngine.h"

using namespace SCXML;

TEST(DebugTest, BasicJavaScriptExecution) {
    // Create and initialize engine
    auto engine = createSCXMLEngine();
    ASSERT_NE(engine, nullptr);
    
    std::cout << "=== Debug: Initializing engine ===" << std::endl;
    bool initialized = engine->initialize();
    ASSERT_TRUE(initialized);
    std::cout << "=== Debug: Engine initialized successfully ===" << std::endl;
    
    // Create session
    std::cout << "=== Debug: Creating session ===" << std::endl;
    bool sessionCreated = engine->createSession("debug_session");
    ASSERT_TRUE(sessionCreated);
    std::cout << "=== Debug: Session created successfully ===" << std::endl;
    
    // Execute simple script
    std::cout << "=== Debug: Executing script ===" << std::endl;
    auto future = engine->executeScript("debug_session", "2 + 3;");
    auto result = future.get();
    
    std::cout << "=== Debug: Script execution result ===" << std::endl;
    std::cout << "Success: " << result.success << std::endl;
    std::cout << "Error: " << result.errorMessage << std::endl;
    std::cout << "Value type: " << result.value.index() << std::endl;
    
    if (result.success) {
        std::cout << "Value as string: " << result.getValueAsString() << std::endl;
    }
    
    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(std::get<double>(result.value), 5.0);
    }
    
    // Cleanup
    engine->destroySession("debug_session");
    engine->shutdown();
}