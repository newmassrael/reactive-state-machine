#include "SimpleMockHttpServer.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>

#include "common/TestUtils.h"
#include <sstream>
#include <thread>

#include "common/Logger.h"

namespace RSM {

SimpleMockHttpServer::SimpleMockHttpServer() = default;

SimpleMockHttpServer::~SimpleMockHttpServer() {
    stop();
}

std::string SimpleMockHttpServer::start() {
    if (running_) {
        return serverUrl_;
    }

    // Find available port
    port_ = findAvailablePort();
    if (port_ <= 0) {
        Logger::error("SimpleMockHttpServer: Failed to find available port");
        return "";
    }

    // Create socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        Logger::error("SimpleMockHttpServer: Failed to create socket");
        return "";
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        Logger::warn("SimpleMockHttpServer: Failed to set SO_REUSEADDR");
    }

    // Set socket to non-blocking mode
    int flags = fcntl(serverSocket_, F_GETFL, 0);
    if (flags < 0 || fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK) < 0) {
        Logger::warn("SimpleMockHttpServer: Failed to set non-blocking mode");
    }

    // Bind to address
    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(serverSocket_, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG_ERROR("SimpleMockHttpServer: Failed to bind to port {}", port_);
        close(serverSocket_);
        serverSocket_ = -1;
        return "";
    }

    // Start listening
    if (listen(serverSocket_, 5) < 0) {
        LOG_ERROR("SimpleMockHttpServer: Failed to listen on port {}", port_);
        close(serverSocket_);
        serverSocket_ = -1;
        return "";
    }

    // Generate server URL
    serverUrl_ = "http://127.0.0.1:" + std::to_string(port_);

    // Start server thread
    running_ = true;
    serverThread_ = std::make_unique<std::thread>(&SimpleMockHttpServer::serverLoop, this);

    LOG_DEBUG("SimpleMockHttpServer: Started on {}", serverUrl_);
    return serverUrl_;
}

void SimpleMockHttpServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Close server socket to break accept() call
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }

    // Wait for server thread to finish
    if (serverThread_ && serverThread_->joinable()) {
        serverThread_->join();
    }
    serverThread_.reset();

    Logger::debug("SimpleMockHttpServer: Stopped");
}

int SimpleMockHttpServer::findAvailablePort() {
    // Try ports from 8000 to 8999
    for (int port = 8000; port < 9000; ++port) {
        int testSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (testSocket < 0) {
            continue;
        }

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        int result = bind(testSocket, (struct sockaddr *)&addr, sizeof(addr));
        close(testSocket);

        if (result == 0) {
            return port;
        }
    }
    return -1;  // No available port found
}

void SimpleMockHttpServer::serverLoop() {
    while (running_) {
        struct sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket_, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No connection available, wait a bit and try again
                std::this_thread::sleep_for(RSM::Test::Utils::POLL_INTERVAL_MS);
                continue;
            } else if (running_) {
                LOG_WARN("SimpleMockHttpServer: Accept failed: {}", strerror(errno));
                break;
            } else {
                // Server is shutting down
                break;
            }
        }

        // Handle request in the same thread (simple implementation)
        handleRequest(clientSocket);
        close(clientSocket);
    }
}

void SimpleMockHttpServer::handleRequest(int clientSocket) {
    char buffer[4096];
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0) {
        return;
    }

    buffer[bytesRead] = '\0';
    std::string request(buffer);

    LOG_DEBUG("SimpleMockHttpServer: Received request: {}", request.substr(0, request.find('\n')));

    // Generate mock response (simulates successful HTTP POST)
    std::string jsonResponse = R"({
  "args": {},
  "data": "",
  "files": {},
  "form": {},
  "headers": {
    "Content-Type": "application/json",
    "Host": "127.0.0.1:)" + std::to_string(port_) +
                               R"("
  },
  "json": null,
  "origin": "127.0.0.1",
  "url": ")" + serverUrl_ + R"(/post"
})";

    std::string response = generateHttpResponse(jsonResponse);
    send(clientSocket, response.c_str(), response.length(), 0);
}

std::string SimpleMockHttpServer::generateHttpResponse(const std::string &content) {
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Connection: close\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "\r\n";
    response << content;
    return response.str();
}

}  // namespace RSM