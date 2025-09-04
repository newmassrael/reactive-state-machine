#include "../../include/runtime/WebSocketIOProcessor.h"
#include "../../include/Logger.h"
#include "../../include/common/GracefulJoin.h"
#include "../../include/runtime/IOProcessor.h"
#include <chrono>
#include <fcntl.h>
#include <netdb.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <random>
#include <regex>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
// OpenSSL replaced with built-in implementations

using namespace SCXML::Runtime;

WebSocketIOProcessor::WebSocketIOProcessor()
    : IOProcessor(ProcessorType::CUSTOM, "http://www.w3.org/TR/scxml/#WebSocketIOProcessor") {
    config_ = Config{};
}

WebSocketIOProcessor::WebSocketIOProcessor(const Config &config)
    : IOProcessor(ProcessorType::CUSTOM, "http://www.w3.org/TR/scxml/#WebSocketIOProcessor"), config_(config) {}

WebSocketIOProcessor::~WebSocketIOProcessor() {
    stop();
}

bool WebSocketIOProcessor::start() {
    if (isRunning()) {
        return true;
    }

    shutdownRequested_.store(false);

    // Start delayed send processing thread
    delayedSendThread_ = std::thread([this]() { processDelayedSends(); });

    Logger::info("WebSocketIOProcessor started successfully");
    return IOProcessor::start();
}

bool WebSocketIOProcessor::stop() {
    if (!isRunning()) {
        return true;
    }

    shutdownRequested_.store(true);

    // Close all connections
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (auto &pair : connections_) {
            auto connection = pair.second;
            connection->shouldClose.store(true);
            if (connection->connectionThread.joinable()) {
                SCXML::GracefulJoin::joinWithTimeout(connection->connectionThread, 2, "WebSocket_Connection");
            }
            if (connection->socket >= 0) {
                close(connection->socket);
            }
        }
        connections_.clear();
    }

    // Stop delayed send thread
    if (delayedSendThread_.joinable()) {
        delayedSendCondition_.notify_all();
        SCXML::GracefulJoin::joinWithTimeout(delayedSendThread_, 3, "WebSocket_DelayedSend");
    }

    Logger::info("WebSocketIOProcessor stopped");
    return IOProcessor::stop();
}

bool WebSocketIOProcessor::send(const SendParameters &params) {
    std::string connectionId, messageType, message;
    extractWebSocketParameters(params, connectionId, messageType, message);

    if (connectionId.empty()) {
        Logger::error("WebSocketIOProcessor: Missing connection ID in send parameters");
        return false;
    }

    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = connections_.find(connectionId);
    if (it == connections_.end()) {
        Logger::error("WebSocketIOProcessor: Connection not found: " + connectionId);
        return false;
    }

    auto connection = it->second;
    if (connection->state != ConnectionState::CONNECTED) {
        Logger::error("WebSocketIOProcessor: Connection not in connected state: " + connectionId);
        return false;
    }

    WebSocketFrame frame;
    if (messageType == "text" || messageType.empty()) {
        frame = WebSocketFrame::createTextFrame(message);
    } else if (messageType == "binary") {
        std::vector<uint8_t> binaryData(message.begin(), message.end());
        frame = WebSocketFrame::createBinaryFrame(binaryData);
    } else if (messageType == "ping") {
        std::vector<uint8_t> pingData(message.begin(), message.end());
        frame = WebSocketFrame::createPingFrame(pingData);
    } else {
        Logger::error("WebSocketIOProcessor: Unsupported message type: " + messageType);
        return false;
    }

    // Queue frame for sending
    {
        std::lock_guard<std::mutex> frameLock(connection->outgoingMutex);
        connection->outgoingFrames.push(frame);
        connection->outgoingCondition.notify_one();
    }

    return true;
}

bool WebSocketIOProcessor::cancelSend(const std::string &sendId) {
    std::lock_guard<std::mutex> lock(delayedSendsMutex_);

    // Mark as cancelled in delayed sends queue
    std::queue<PendingWebSocketSend> tempQueue;
    while (!delayedSends_.empty()) {
        auto pendingSend = delayedSends_.front();
        delayedSends_.pop();

        if (pendingSend.sendId == sendId) {
            pendingSend.cancelled = true;
        }

        tempQueue.push(pendingSend);
    }

    delayedSends_ = tempQueue;
    return true;
}

bool WebSocketIOProcessor::canHandle(const std::string &typeURI) const {
    return typeURI == "http://www.w3.org/TR/scxml/#WebSocketIOProcessor" || typeURI == "websocket" ||
           typeURI.find("ws://") == 0 || typeURI.find("wss://") == 0;
}

std::string WebSocketIOProcessor::connectToServer(const std::string &url) {
    std::string host, path;
    int port;
    bool isSecure;

    if (!parseWebSocketURL(url, host, port, path, isSecure)) {
        Logger::error("WebSocketIOProcessor: Invalid WebSocket URL: " + url);
        return "";
    }

    if (isSecure) {
        Logger::error("WebSocketIOProcessor: WSS (secure WebSocket) not yet supported");
        return "";
    }

    auto connection = std::make_shared<WebSocketConnection>();
    connection->connectionId = generateConnectionId();
    connection->url = url;
    connection->state = ConnectionState::CONNECTING;

    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_[connection->connectionId] = connection;
    }

    // Start connection thread
    connection->connectionThread =
        std::thread([this, connection, host, port, path]() { connectionThreadMain(connection); });

    Logger::info("WebSocketIOProcessor: Initiated connection to " + url + " (ID: " + connection->connectionId + ")");
    return connection->connectionId;
}

bool WebSocketIOProcessor::disconnect(const std::string &connectionId) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    auto it = connections_.find(connectionId);
    if (it == connections_.end()) {
        return false;
    }

    auto connection = it->second;
    connection->shouldClose.store(true);

    // Send close frame
    if (connection->state == ConnectionState::CONNECTED) {
        WebSocketFrame closeFrame =
            WebSocketFrame::createCloseFrame(WebSocketFrame::CloseCode::NORMAL_CLOSURE, "Client disconnect");

        std::lock_guard<std::mutex> frameLock(connection->outgoingMutex);
        connection->outgoingFrames.push(closeFrame);
        connection->outgoingCondition.notify_one();
    }

    return true;
}

bool WebSocketIOProcessor::sendTextMessage(const std::string &connectionId, const std::string &message) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    auto it = connections_.find(connectionId);
    if (it == connections_.end() || it->second->state != ConnectionState::CONNECTED) {
        return false;
    }

    auto connection = it->second;
    WebSocketFrame frame = WebSocketFrame::createTextFrame(message);

    std::lock_guard<std::mutex> frameLock(connection->outgoingMutex);
    connection->outgoingFrames.push(frame);
    connection->outgoingCondition.notify_one();

    return true;
}

bool WebSocketIOProcessor::sendBinaryMessage(const std::string &connectionId, const std::vector<uint8_t> &data) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    auto it = connections_.find(connectionId);
    if (it == connections_.end() || it->second->state != ConnectionState::CONNECTED) {
        return false;
    }

    auto connection = it->second;
    WebSocketFrame frame = WebSocketFrame::createBinaryFrame(data);

    std::lock_guard<std::mutex> frameLock(connection->outgoingMutex);
    connection->outgoingFrames.push(frame);
    connection->outgoingCondition.notify_one();

    return true;
}

bool WebSocketIOProcessor::sendPing(const std::string &connectionId, const std::vector<uint8_t> &payload) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    auto it = connections_.find(connectionId);
    if (it == connections_.end() || it->second->state != ConnectionState::CONNECTED) {
        return false;
    }

    auto connection = it->second;
    WebSocketFrame frame = WebSocketFrame::createPingFrame(payload);

    std::lock_guard<std::mutex> frameLock(connection->outgoingMutex);
    connection->outgoingFrames.push(frame);
    connection->outgoingCondition.notify_one();

    return true;
}

WebSocketIOProcessor::ConnectionState WebSocketIOProcessor::getConnectionState(const std::string &connectionId) const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    auto it = connections_.find(connectionId);
    if (it == connections_.end()) {
        return ConnectionState::DISCONNECTED;
    }

    return it->second->state;
}

std::vector<std::string> WebSocketIOProcessor::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    std::vector<std::string> activeConnections;
    for (const auto &pair : connections_) {
        if (pair.second->state == ConnectionState::CONNECTED) {
            activeConnections.push_back(pair.first);
        }
    }

    return activeConnections;
}

bool WebSocketIOProcessor::updateConfig(const Config &config) {
    if (isRunning()) {
        Logger::error("WebSocketIOProcessor: Cannot update config while running");
        return false;
    }

    config_ = config;
    return true;
}

bool WebSocketIOProcessor::parseWebSocketURL(const std::string &url, std::string &host, int &port, std::string &path,
                                             bool &isSecure) const {
    std::regex wsRegex(R"(^(ws|wss)://([^:/]+)(?::(\d+))?(.*)$)");
    std::smatch matches;

    if (!std::regex_match(url, matches, wsRegex)) {
        return false;
    }

    std::string protocol = matches[1].str();
    host = matches[2].str();
    std::string portStr = matches[3].str();
    path = matches[4].str();

    isSecure = (protocol == "wss");
    port = portStr.empty() ? (isSecure ? 443 : 80) : std::stoi(portStr);

    if (path.empty()) {
        path = "/";
    }

    return true;
}

bool WebSocketIOProcessor::performHandshake(int socket, const std::string &host, int port, const std::string &path) {
    // Generate WebSocket key
    std::string wsKey = generateWebSocketKey();

    // Build HTTP upgrade request
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << host;
    if (port != 80 && port != 443) {
        request << ":" << port;
    }
    request << "\r\n";
    request << "Upgrade: websocket\r\n";
    request << "Connection: Upgrade\r\n";
    request << "Sec-WebSocket-Key: " << wsKey << "\r\n";
    request << "Sec-WebSocket-Version: 13\r\n";

    // Add custom headers
    for (const auto &header : config_.headers) {
        request << header.first << ": " << header.second << "\r\n";
    }

    // Add subprotocols if specified
    if (!config_.subprotocols.empty()) {
        request << "Sec-WebSocket-Protocol: ";
        for (size_t i = 0; i < config_.subprotocols.size(); ++i) {
            if (i > 0) {
                request << ", ";
            }
            request << config_.subprotocols[i];
        }
        request << "\r\n";
    }

    request << "\r\n";

    std::string requestStr = request.str();

    // Send handshake request
    ssize_t bytesSent = ::send(socket, requestStr.c_str(), requestStr.length(), 0);
    if (bytesSent != static_cast<ssize_t>(requestStr.length())) {
        Logger::error("WebSocketIOProcessor: Failed to send handshake request");
        return false;
    }

    // Receive handshake response
    char buffer[4096];
    ssize_t bytesReceived = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        Logger::error("WebSocketIOProcessor: Failed to receive handshake response");
        return false;
    }

    buffer[bytesReceived] = '\0';
    std::string response(buffer);

    // Validate handshake response
    if (response.find("HTTP/1.1 101") != 0) {
        Logger::error("WebSocketIOProcessor: Invalid handshake response status");
        return false;
    }

    // Validate Sec-WebSocket-Accept
    std::string expectedAccept = calculateWebSocketAccept(wsKey);
    if (response.find("Sec-WebSocket-Accept: " + expectedAccept) == std::string::npos) {
        Logger::error("WebSocketIOProcessor: Invalid Sec-WebSocket-Accept header");
        return false;
    }

    Logger::debug("WebSocketIOProcessor: WebSocket handshake completed successfully");
    return true;
}

std::string WebSocketIOProcessor::generateWebSocketKey() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::vector<uint8_t> keyBytes(16);
    for (auto &byte : keyBytes) {
        byte = dis(gen);
    }

    return base64Encode(keyBytes);
}

std::string WebSocketIOProcessor::calculateWebSocketAccept(const std::string &key) const {
    std::string concat = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::vector<uint8_t> hash = sha1Hash(concat);
    return base64Encode(hash);
}

std::string WebSocketIOProcessor::base64Encode(const std::vector<uint8_t> &data) const {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data.data(), data.size());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return result;
}

std::vector<uint8_t> WebSocketIOProcessor::sha1Hash(const std::string &data) const {
    std::vector<uint8_t> hash(SHA_DIGEST_LENGTH);
    SHA1(reinterpret_cast<const unsigned char *>(data.c_str()), data.length(), hash.data());
    return hash;
}

void WebSocketIOProcessor::connectionThreadMain(std::shared_ptr<WebSocketConnection> connection) {
    Logger::debug("WebSocketIOProcessor: Connection thread started for " + connection->connectionId);

    std::string host, path;
    int port;
    bool isSecure;

    if (!parseWebSocketURL(connection->url, host, port, path, isSecure)) {
        Logger::error("WebSocketIOProcessor: Failed to parse URL in connection thread");
        connection->state = ConnectionState::ERROR;
        return;
    }

    // Create TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        Logger::error("WebSocketIOProcessor: Failed to create socket");
        connection->state = ConnectionState::ERROR;
        return;
    }

    connection->socket = sock;

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = config_.connectTimeoutMs / 1000;
    timeout.tv_usec = (config_.connectTimeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Resolve host
    struct hostent *hostEntry = gethostbyname(host.c_str());
    if (!hostEntry) {
        Logger::error("WebSocketIOProcessor: Failed to resolve host: " + host);
        connection->state = ConnectionState::ERROR;
        close(sock);
        return;
    }

    // Connect to server
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        Logger::error("WebSocketIOProcessor: Failed to connect to " + host + ":" + std::to_string(port));
        connection->state = ConnectionState::ERROR;
        close(sock);
        return;
    }

    // Perform WebSocket handshake
    if (!performHandshake(sock, host, port, path)) {
        Logger::error("WebSocketIOProcessor: WebSocket handshake failed");
        connection->state = ConnectionState::ERROR;
        close(sock);
        return;
    }

    // Connection established
    connection->state = ConnectionState::CONNECTED;
    connection->lastPingTime = std::chrono::steady_clock::now();
    connection->lastPongTime = std::chrono::steady_clock::now();

    // Notify about connection established
    auto connectionEvent = createConnectionEvent(connection->connectionId, "websocket.connected");
    if (eventCallback_) {
        eventCallback_(connectionEvent);
    }

    Logger::info("WebSocketIOProcessor: WebSocket connection established: " + connection->connectionId);

    // Main connection loop
    while (!connection->shouldClose.load() && !shutdownRequested_.load()) {
        // Handle keepalive
        handleKeepalive(connection);

        // Send queued frames
        sendQueuedFrames(connection);

        // Receive frames (with timeout)
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        struct timeval selectTimeout;
        selectTimeout.tv_sec = 1;
        selectTimeout.tv_usec = 0;

        int result = select(sock + 1, &readfds, nullptr, nullptr, &selectTimeout);
        if (result > 0 && FD_ISSET(sock, &readfds)) {
            // Data available to read
            uint8_t buffer[4096];
            ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer), 0);

            if (bytesReceived <= 0) {
                Logger::info("WebSocketIOProcessor: Connection closed by peer");
                break;
            }

            // Parse received frame
            WebSocketFrame frame;
            if (frame.parseFromData(buffer, bytesReceived)) {
                handleIncomingFrame(connection, frame);
            } else {
                Logger::error("WebSocketIOProcessor: Failed to parse incoming frame");
            }
        } else if (result < 0) {
            Logger::error("WebSocketIOProcessor: Select error in connection thread");
            break;
        }
    }

    // Cleanup
    connection->state = ConnectionState::CLOSING;
    close(sock);
    connection->socket = -1;
    connection->state = ConnectionState::CLOSED;

    // Notify about connection closed
    auto closedEvent = createConnectionEvent(connection->connectionId, "websocket.closed");
    if (eventCallback_) {
        eventCallback_(closedEvent);
    }

    Logger::info("WebSocketIOProcessor: Connection thread ended for " + connection->connectionId);
}

void WebSocketIOProcessor::handleIncomingFrame(std::shared_ptr<WebSocketConnection> connection,
                                               const WebSocketFrame &frame) {
    switch (frame.getOpCode()) {
    case WebSocketFrame::OpCode::TEXT: {
        std::string text = frame.getPayloadAsText();
        Logger::debug("WebSocketIOProcessor: Received text message: " + text.substr(0, 100) +
                      (text.length() > 100 ? "..." : ""));

        auto messageEvent = createMessageEvent(connection->connectionId, frame);
        if (eventCallback_) {
            eventCallback_(messageEvent);
        }
        break;
    }

    case WebSocketFrame::OpCode::BINARY: {
        Logger::debug("WebSocketIOProcessor: Received binary message (" + std::to_string(frame.getPayloadLength()) +
                      " bytes)");

        auto messageEvent = createMessageEvent(connection->connectionId, frame);
        if (eventCallback_) {
            eventCallback_(messageEvent);
        }
        break;
    }

    case WebSocketFrame::OpCode::PING: {
        Logger::debug("WebSocketIOProcessor: Received ping, sending pong");

        // Send pong with same payload
        WebSocketFrame pongFrame = WebSocketFrame::createPongFrame(frame.getPayload());

        std::lock_guard<std::mutex> lock(connection->outgoingMutex);
        connection->outgoingFrames.push(pongFrame);
        connection->outgoingCondition.notify_one();
        break;
    }

    case WebSocketFrame::OpCode::PONG: {
        Logger::debug("WebSocketIOProcessor: Received pong");
        connection->lastPongTime = std::chrono::steady_clock::now();
        connection->waitingForPong = false;
        break;
    }

    case WebSocketFrame::OpCode::CLOSE: {
        Logger::info("WebSocketIOProcessor: Received close frame");
        connection->shouldClose.store(true);

        // Send close acknowledgment if not already closing
        if (connection->state == ConnectionState::CONNECTED) {
            WebSocketFrame closeFrame = WebSocketFrame::createCloseFrame(WebSocketFrame::CloseCode::NORMAL_CLOSURE, "");

            std::lock_guard<std::mutex> lock(connection->outgoingMutex);
            connection->outgoingFrames.push(closeFrame);
            connection->outgoingCondition.notify_one();
        }
        break;
    }

    default:
        Logger::warning("WebSocketIOProcessor: Received unsupported frame type");
        break;
    }
}

void WebSocketIOProcessor::sendQueuedFrames(std::shared_ptr<WebSocketConnection> connection) {
    std::unique_lock<std::mutex> lock(connection->outgoingMutex);

    while (!connection->outgoingFrames.empty()) {
        WebSocketFrame frame = connection->outgoingFrames.front();
        connection->outgoingFrames.pop();

        lock.unlock();

        // Serialize and send frame (client-side frames should be masked)
        std::vector<uint8_t> frameData = frame.serialize(true);
        ssize_t bytesSent = ::send(connection->socket, frameData.data(), frameData.size(), 0);

        if (bytesSent != static_cast<ssize_t>(frameData.size())) {
            Logger::error("WebSocketIOProcessor: Failed to send frame");
            connection->shouldClose.store(true);
            break;
        }

        lock.lock();
    }
}

void WebSocketIOProcessor::handleKeepalive(std::shared_ptr<WebSocketConnection> connection) {
    auto now = std::chrono::steady_clock::now();
    auto pingInterval = std::chrono::milliseconds(config_.pingIntervalMs);
    auto pongTimeout = std::chrono::milliseconds(config_.pongTimeoutMs);

    // Check if we need to send a ping
    if (now - connection->lastPingTime >= pingInterval && !connection->waitingForPong) {
        Logger::debug("WebSocketIOProcessor: Sending keepalive ping");

        WebSocketFrame pingFrame = WebSocketFrame::createPingFrame();

        std::lock_guard<std::mutex> lock(connection->outgoingMutex);
        connection->outgoingFrames.push(pingFrame);
        connection->outgoingCondition.notify_one();

        connection->lastPingTime = now;
        connection->waitingForPong = true;
    }

    // Check for pong timeout
    if (connection->waitingForPong && (now - connection->lastPingTime >= pongTimeout)) {
        Logger::error("WebSocketIOProcessor: Pong timeout, closing connection");
        connection->shouldClose.store(true);
    }
}

void WebSocketIOProcessor::processDelayedSends() {
    while (!shutdownRequested_.load()) {
        std::unique_lock<std::mutex> lock(delayedSendsMutex_);

        delayedSendCondition_.wait_for(lock, std::chrono::milliseconds(1000));

        auto now = std::chrono::steady_clock::now();

        while (!delayedSends_.empty()) {
            auto pendingSend = delayedSends_.front();

            if (pendingSend.cancelled) {
                delayedSends_.pop();
                continue;
            }

            if (now >= pendingSend.scheduledTime) {
                delayedSends_.pop();
                lock.unlock();

                // Execute the delayed send
                sendTextMessage(pendingSend.connectionId, pendingSend.frame.getPayloadAsText());

                lock.lock();
            } else {
                break;  // Queue is ordered by time, so we can stop here
            }
        }
    }
}

void WebSocketIOProcessor::extractWebSocketParameters(const SendParameters &params, std::string &connectionId,
                                                      std::string &messageType, std::string &message) const {
    // Extract connection ID from target or params
    if (!params.target.empty() && params.target.find("websocket://") == 0) {
        connectionId = params.target.substr(12);  // Remove "websocket://" prefix
    }

    // Check params for connection ID
    auto connIt = params.params.find("connectionid");
    if (connIt != params.params.end()) {
        connectionId = connIt->second;
    }

    // Extract message type
    auto typeIt = params.params.find("type");
    if (typeIt != params.params.end()) {
        messageType = typeIt->second;
    } else {
        messageType = "text";  // Default to text
    }

    // Extract message content
    if (!params.content.empty()) {
        message = params.content;
    } else {
        auto msgIt = params.params.find("message");
        if (msgIt != params.params.end()) {
            message = msgIt->second;
        }
    }
}

std::string WebSocketIOProcessor::generateConnectionId() {
    std::lock_guard<std::mutex> lock(connectionIdMutex_);
    return "ws_" + std::to_string(nextConnectionId_.fetch_add(1));
}

SCXML::Events::EventPtr WebSocketIOProcessor::createConnectionEvent(const std::string &connectionId,
                                                                    const std::string &eventType) {
    auto event = std::make_shared<SCXML::Events::Event>(eventType, SCXML::Events::EventType::EXTERNAL);
    // We need to add connection info differently since Event doesn't have data["key"] = value interface
    return event;
}

SCXML::Events::EventPtr WebSocketIOProcessor::createMessageEvent(const std::string &connectionId,
                                                                 const WebSocketFrame &frame) {
    std::string eventData;

    if (frame.getOpCode() == WebSocketFrame::OpCode::TEXT) {
        eventData = frame.getPayloadAsText();
    } else if (frame.getOpCode() == WebSocketFrame::OpCode::BINARY) {
        eventData = "Binary data (" + std::to_string(frame.getPayloadLength()) + " bytes)";
    }

    auto event =
        std::make_shared<SCXML::Events::Event>("websocket.message", SCXML::Events::EventType::EXTERNAL, eventData);
    return event;
}