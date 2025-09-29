#include <chrono>
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>

#include "SimpleMockHttpServer.h"
#include "actions/SendAction.h"
#include "common/Logger.h"
#include "events/EventTargetFactoryImpl.h"
#include "events/HttpEventTarget.h"
#include "mocks/MockEventRaiser.h"
#include "runtime/ActionExecutorImpl.h"
#include "runtime/ExecutionContextImpl.h"

namespace RSM {

/**
 * @brief Test fixture for HTTP event target functionality
 */
class HttpEventTargetTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start embedded mock HTTP server
        mockServer_ = std::make_unique<SimpleMockHttpServer>();
        mockServerUrl_ = mockServer_->start();
        ASSERT_FALSE(mockServerUrl_.empty()) << "Failed to start mock HTTP server";

        LOG_INFO("HttpEventTargetTest: Mock server started at {}", mockServerUrl_);

        // Create basic infrastructure with MockEventRaiser
        auto mockEventRaiser =
            std::make_shared<RSM::Test::MockEventRaiser>([](const std::string &, const std::string &) -> bool {
                return true;  // Always succeed for HTTP tests
            });

        actionExecutor_ = std::make_shared<ActionExecutorImpl>("test_session");
        actionExecutor_->setEventRaiser(mockEventRaiser);
        targetFactory_ = std::make_shared<EventTargetFactoryImpl>(mockEventRaiser);
    }

    void TearDown() override {
        // Stop mock server
        if (mockServer_) {
            mockServer_->stop();
            Logger::info("HttpEventTargetTest: Mock server stopped");
        }
    }

protected:
    std::shared_ptr<ActionExecutorImpl> actionExecutor_;
    std::shared_ptr<EventTargetFactoryImpl> targetFactory_;
    std::unique_ptr<SimpleMockHttpServer> mockServer_;
    std::string mockServerUrl_;
};

/**
 * @brief Test HTTP target creation and validation
 */
TEST_F(HttpEventTargetTest, HttpTargetCreation) {
    // Test HTTP target creation with mock server
    auto httpTarget = std::make_shared<HttpEventTarget>(mockServerUrl_ + "/post");

    EXPECT_EQ(httpTarget->getTargetType(), "http");
    EXPECT_TRUE(httpTarget->canHandle("http://example.com"));
    EXPECT_FALSE(httpTarget->canHandle("https://example.com"));  // Different scheme
    EXPECT_FALSE(httpTarget->canHandle("ftp://example.com"));
    EXPECT_FALSE(httpTarget->canHandle("invalid-url"));

    // Validate target
    auto errors = httpTarget->validate();
    EXPECT_TRUE(errors.empty()) << "HttpEventTarget should be valid";

    // Test debug info
    std::string debugInfo = httpTarget->getDebugInfo();
    EXPECT_FALSE(debugInfo.empty());
    EXPECT_NE(debugInfo.find("HttpEventTarget"), std::string::npos);
    EXPECT_NE(debugInfo.find("127.0.0.1"), std::string::npos);
}

/**
 * @brief Test HTTPS target creation
 */
TEST_F(HttpEventTargetTest, HttpsTargetCreation) {
    // Test HTTPS target creation (mock server only supports HTTP)
    // This test only validates the target creation, not actual HTTPS communication
    auto httpsTarget = std::make_shared<HttpEventTarget>("https://example.com/post");

    EXPECT_EQ(httpsTarget->getTargetType(), "https");

    // Validate target
    auto errors = httpsTarget->validate();
    EXPECT_TRUE(errors.empty()) << "HttpsEventTarget should be valid";
}

/**
 * @brief Test invalid URL handling
 */
TEST_F(HttpEventTargetTest, InvalidUrlHandling) {
    // Test invalid URLs
    std::vector<std::string> invalidUrls = {"",        "not-a-url", "ftp://example.com",
                                            "http://", "https://",  "http:///path"};

    for (const auto &invalidUrl : invalidUrls) {
        auto target = std::make_shared<HttpEventTarget>(invalidUrl);

        // Should have validation errors
        auto errors = target->validate();
        EXPECT_FALSE(errors.empty()) << "URL '" << invalidUrl << "' should be invalid";
    }
}

/**
 * @brief Test factory integration
 */
TEST_F(HttpEventTargetTest, FactoryIntegration) {
    // Test HTTP target creation via factory with mock server
    auto httpTarget = targetFactory_->createTarget(mockServerUrl_ + "/post", "");
    ASSERT_NE(httpTarget, nullptr);
    EXPECT_EQ(httpTarget->getTargetType(), "http");

    // Test HTTPS target creation via factory (validation only)
    auto httpsTarget = targetFactory_->createTarget("https://example.com/post", "");
    ASSERT_NE(httpsTarget, nullptr);
    EXPECT_EQ(httpsTarget->getTargetType(), "https");

    // Test unsupported scheme
    auto ftpTarget = targetFactory_->createTarget("ftp://example.com", "");
    EXPECT_EQ(ftpTarget, nullptr);

    // Check supported schemes
    auto schemes = targetFactory_->getSupportedSchemes();
    EXPECT_TRUE(std::find(schemes.begin(), schemes.end(), "http") != schemes.end());
    EXPECT_TRUE(std::find(schemes.begin(), schemes.end(), "https") != schemes.end());
    EXPECT_TRUE(std::find(schemes.begin(), schemes.end(), "internal") != schemes.end());

    // Check scheme support
    EXPECT_TRUE(targetFactory_->isSchemeSupported("http"));
    EXPECT_TRUE(targetFactory_->isSchemeSupported("https"));
    EXPECT_TRUE(targetFactory_->isSchemeSupported("internal"));
    EXPECT_FALSE(targetFactory_->isSchemeSupported("ftp"));
}

/**
 * @brief Test HTTP event sending with embedded mock server
 *
 * This test uses an embedded mock HTTP server for reliable testing
 * without external network dependencies.
 */
TEST_F(HttpEventTargetTest, BasicHttpEventSending) {
    // Create HTTP target with mock server URL
    auto httpTarget = std::make_shared<HttpEventTarget>(mockServerUrl_ + "/post");

    // Create test event
    EventDescriptor event;
    event.eventName = "test.event";
    event.data = R"({"message": "hello world", "timestamp": 12345})";
    event.sendId = "test_001";
    event.target = mockServerUrl_ + "/post";

    // Send event (async)
    auto resultFuture = httpTarget->send(event);

    // Wait for result with shorter timeout (local server should be fast)
    auto status = resultFuture.wait_for(std::chrono::seconds(5));

    ASSERT_EQ(status, std::future_status::ready) << "HTTP request should complete quickly with mock server";

    auto result = resultFuture.get();

    // Should always succeed with mock server
    EXPECT_TRUE(result.isSuccess) << "Mock server should always respond successfully: " << result.errorMessage;
    EXPECT_EQ(result.sendId, "test_001");
    EXPECT_TRUE(result.errorMessage.empty());

    LOG_INFO("HTTP test successful - sent event to mock server at {}", mockServerUrl_);
}

/**
 * @brief Test HTTPS target functionality (validation only)
 *
 * Note: Our embedded mock server only supports HTTP, so this test
 * focuses on HTTPS target creation and validation rather than actual communication.
 */
TEST_F(HttpEventTargetTest, BasicHttpsTargetValidation) {
    // Create HTTPS target (for validation testing)
    auto httpsTarget = std::make_shared<HttpEventTarget>("https://example.com/post");

    // Validate target properties
    EXPECT_EQ(httpsTarget->getTargetType(), "https");
    EXPECT_TRUE(httpsTarget->canHandle("https://example.com"));
    EXPECT_FALSE(httpsTarget->canHandle("http://example.com"));  // Different scheme

    // Validate target
    auto errors = httpsTarget->validate();
    EXPECT_TRUE(errors.empty()) << "HTTPS target should be valid";

    Logger::info("HTTPS target validation successful");
}

/**
 * @brief Test HTTP error handling
 */
TEST_F(HttpEventTargetTest, HttpErrorHandling) {
    // Create target pointing to non-existent server
    auto httpTarget = std::make_shared<HttpEventTarget>("http://non-existent-server-12345.com/");

    // Create test event
    EventDescriptor event;
    event.eventName = "test.error";
    event.data = "test";
    event.sendId = "error_test_001";

    // Send event (should fail)
    auto resultFuture = httpTarget->send(event);

    // Wait for result with timeout
    auto status = resultFuture.wait_for(std::chrono::seconds(5));

    ASSERT_EQ(status, std::future_status::ready);

    auto result = resultFuture.get();

    // Should fail
    EXPECT_FALSE(result.isSuccess);
    EXPECT_FALSE(result.errorMessage.empty());
    EXPECT_EQ(result.errorType, SendResult::ErrorType::NETWORK_ERROR);

    LOG_DEBUG("Expected error for non-existent server: {}", result.errorMessage);
}

/**
 * @brief Test SendAction integration with HTTP targets
 */
TEST_F(HttpEventTargetTest, SendActionIntegration) {
    // Create action executor with HTTP dispatcher
    // Note: For this test, we'll just test the action creation and validation

    SendAction sendAction("http.test.event");
    sendAction.setTarget("http://httpbin.org/post");
    sendAction.setData("'integration test data'");
    sendAction.setSendId("integration_001");

    // Validate the action
    auto errors = sendAction.validate();
    EXPECT_TRUE(errors.empty()) << "SendAction should be valid";

    // Check properties
    EXPECT_EQ(sendAction.getEvent(), "http.test.event");
    EXPECT_EQ(sendAction.getTarget(), "http://httpbin.org/post");
    EXPECT_EQ(sendAction.getData(), "'integration test data'");
    EXPECT_EQ(sendAction.getSendId(), "integration_001");
}

/**
 * @brief Test custom headers and timeout settings
 */
TEST_F(HttpEventTargetTest, CustomConfiguration) {
    auto httpTarget = std::make_shared<HttpEventTarget>("http://httpbin.org/post");

    // Set custom timeout
    httpTarget->setTimeout(std::chrono::milliseconds(2000));

    // Set custom headers
    std::map<std::string, std::string> headers = {{"X-Custom-Header", "test-value"}, {"X-API-Key", "secret-key"}};
    httpTarget->setCustomHeaders(headers);

    // Set max retries
    httpTarget->setMaxRetries(3);

    // Set SSL verification
    httpTarget->setSSLVerification(false);

    // Validate configuration
    auto errors = httpTarget->validate();
    EXPECT_TRUE(errors.empty());

    // Check debug info includes configuration
    std::string debugInfo = httpTarget->getDebugInfo();
    EXPECT_NE(debugInfo.find("timeout=2000"), std::string::npos);
    EXPECT_NE(debugInfo.find("retries=3"), std::string::npos);
    EXPECT_NE(debugInfo.find("ssl_verify=false"), std::string::npos);
}

}  // namespace RSM