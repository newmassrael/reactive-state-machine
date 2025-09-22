#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace RSM {

/**
 * @brief Simple embedded mock HTTP server for testing
 *
 * This class provides a lightweight HTTP server that can be started
 * and stopped within test cases. It automatically finds an available
 * port and responds to HTTP requests with predefined responses.
 */
class SimpleMockHttpServer {
public:
    SimpleMockHttpServer();
    ~SimpleMockHttpServer();

    /**
     * @brief Start the mock server
     * @return The server URL (e.g., "http://127.0.0.1:8080")
     */
    std::string start();

    /**
     * @brief Stop the mock server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const {
        return running_;
    }

    /**
     * @brief Get the server URL
     */
    const std::string &getUrl() const {
        return serverUrl_;
    }

private:
    void serverLoop();
    int findAvailablePort();
    void handleRequest(int clientSocket);
    std::string generateHttpResponse(const std::string &content);

    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> serverThread_;
    int serverSocket_{-1};
    int port_{0};
    std::string serverUrl_;
};

}  // namespace RSM