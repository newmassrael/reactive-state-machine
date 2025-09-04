#pragma once

#include "runtime/IOProcessor.h"
#include "NativeHttpClient.h"
#include "WebSocketFrame.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <queue>
#include <thread>
#include <unordered_set>

namespace SCXML {
namespace Runtime {

/**
 * @brief WebSocket I/O Processor implementing real-time bidirectional communication
 *
 * Supports WebSocket protocol (RFC 6455) for real-time communication between
 * SCXML state machines and WebSocket servers/clients.
 *
 * Features:
 * - WebSocket handshake (HTTP upgrade)
 * - Frame-based messaging (text, binary, ping, pong, close)
 * - Automatic connection management
 * - Heartbeat/keepalive support
 * - Connection reconnection
 * - Multiple concurrent connections
 */
class WebSocketIOProcessor : public IOProcessor {
public:
    /**
     * @brief WebSocket connection state
     */
    enum class ConnectionState { DISCONNECTED, CONNECTING, CONNECTED, CLOSING, CLOSED, ERROR };

    /**
     * @brief Configuration for WebSocket I/O Processor
     */
    struct Config {
        std::string serverHost = "localhost";     // Server host for WebSocket server
        int serverPort = 8080;                    // Server port for WebSocket server
        std::string serverPath = "/websocket";    // WebSocket endpoint path
        bool enableServer = false;                // Enable WebSocket server
        uint32_t connectTimeoutMs = 10000;        // Connection timeout (10 seconds)
        uint32_t pingIntervalMs = 30000;          // Ping interval (30 seconds)
        uint32_t pongTimeoutMs = 5000;            // Pong timeout (5 seconds)
        uint32_t maxMessageSize = 1048576;        // Max message size (1MB)
        uint32_t maxConcurrentConnections = 100;  // Max concurrent connections
        bool autoReconnect = true;                // Auto-reconnect on disconnect
        uint32_t reconnectDelayMs = 5000;         // Reconnect delay (5 seconds)
        uint32_t maxReconnectAttempts = 3;        // Max reconnection attempts

        // Headers to include in WebSocket handshake
        std::unordered_map<std::string, std::string> headers;

        // Supported WebSocket subprotocols
        std::vector<std::string> subprotocols;
    };

private:
    /**
     * @brief WebSocket connection information
     */
    struct WebSocketConnection {
        std::string connectionId;
        std::string url;
        int socket = -1;
        ConnectionState state = ConnectionState::DISCONNECTED;
        std::thread connectionThread;
        std::atomic<bool> shouldClose{false};
        std::chrono::steady_clock::time_point lastPingTime;
        std::chrono::steady_clock::time_point lastPongTime;
        bool waitingForPong = false;
        uint32_t reconnectAttempts = 0;

        // Message queues
        std::queue<WebSocketFrame> outgoingFrames;
        std::mutex outgoingMutex;
        std::condition_variable outgoingCondition;

        WebSocketConnection() = default;
        WebSocketConnection(const WebSocketConnection &) = delete;
        WebSocketConnection &operator=(const WebSocketConnection &) = delete;
        WebSocketConnection(WebSocketConnection &&) = default;
        WebSocketConnection &operator=(WebSocketConnection &&) = default;
    };

    /**
     * @brief Pending WebSocket send operation
     */
    struct PendingWebSocketSend {
        std::string sendId;
        std::string connectionId;
        WebSocketFrame frame;
        std::chrono::steady_clock::time_point scheduledTime;
        bool cancelled = false;

        // Make it copyable and moveable
        PendingWebSocketSend() = default;

        PendingWebSocketSend(const PendingWebSocketSend &other)
            : sendId(other.sendId), connectionId(other.connectionId), frame(other.frame),
              scheduledTime(other.scheduledTime), cancelled(other.cancelled) {}

        PendingWebSocketSend &operator=(const PendingWebSocketSend &other) {
            if (this != &other) {
                sendId = other.sendId;
                connectionId = other.connectionId;
                frame = other.frame;
                scheduledTime = other.scheduledTime;
                cancelled = other.cancelled;
            }
            return *this;
        }

        PendingWebSocketSend(PendingWebSocketSend &&other) noexcept = default;
        PendingWebSocketSend &operator=(PendingWebSocketSend &&other) noexcept = default;
    };

public:
    /**
     * @brief Constructor with default configuration
     */
    WebSocketIOProcessor();

    /**
     * @brief Constructor with custom configuration
     * @param config WebSocket processor configuration
     */
    explicit WebSocketIOProcessor(const Config &config);

    /**
     * @brief Destructor
     */
    ~WebSocketIOProcessor() override;

    /**
     * @brief Start the WebSocket I/O Processor
     * @return true if started successfully
     */
    bool start() override;

    /**
     * @brief Stop the WebSocket I/O Processor
     * @return true if stopped successfully
     */
    bool stop() override;

    /**
     * @brief Send WebSocket message
     * @param params Send parameters from SCXML <send> element
     * @return true if send was initiated successfully
     */
    bool send(const SendParameters &params) override;

    /**
     * @brief Cancel a WebSocket send operation
     * @param sendId The send identifier to cancel
     * @return true if cancellation was successful
     */
    bool cancelSend(const std::string &sendId) override;

    /**
     * @brief Check if this processor can handle the given type URI
     * @param typeURI The type URI to check
     * @return true if this processor can handle the type
     */
    bool canHandle(const std::string &typeURI) const override;

    /**
     * @brief Connect to WebSocket server
     * @param url WebSocket URL (ws:// or wss://)
     * @return Connection ID for future operations
     */
    std::string connectToServer(const std::string &url);

    /**
     * @brief Disconnect from WebSocket server
     * @param connectionId Connection identifier
     * @return true if disconnection was initiated
     */
    bool disconnect(const std::string &connectionId);

    /**
     * @brief Send text message to specific connection
     * @param connectionId Connection identifier
     * @param message Text message
     * @return true if message was queued for sending
     */
    bool sendTextMessage(const std::string &connectionId, const std::string &message);

    /**
     * @brief Send binary message to specific connection
     * @param connectionId Connection identifier
     * @param data Binary data
     * @return true if message was queued for sending
     */
    bool sendBinaryMessage(const std::string &connectionId, const std::vector<uint8_t> &data);

    /**
     * @brief Send ping frame to specific connection
     * @param connectionId Connection identifier
     * @param payload Optional ping payload
     * @return true if ping was queued for sending
     */
    bool sendPing(const std::string &connectionId, const std::vector<uint8_t> &payload = {});

    /**
     * @brief Get connection state
     * @param connectionId Connection identifier
     * @return Current connection state
     */
    ConnectionState getConnectionState(const std::string &connectionId) const;

    /**
     * @brief Get list of active connections
     * @return Vector of connection IDs
     */
    std::vector<std::string> getActiveConnections() const;

    /**
     * @brief Update processor configuration (processor must be stopped)
     * @param config New configuration
     * @return true if configuration was updated
     */
    bool updateConfig(const Config &config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const Config &getConfig() const {
        return config_;
    }

private:

public:  // Make public for testing
    /**
     * @brief Parse WebSocket URL
     * @param url WebSocket URL
     * @param host Output host
     * @param port Output port
     * @param path Output path
     * @param isSecure Output secure flag
     * @return true if parsing was successful
     */
    bool parseWebSocketURL(const std::string &url, std::string &host, int &port, std::string &path,
                           bool &isSecure) const;

    /**
     * @brief Generate WebSocket key for handshake
     * @return Base64-encoded WebSocket key
     */
    std::string generateWebSocketKey() const;

    /**
     * @brief Calculate WebSocket accept value
     * @param key WebSocket key
     * @return Base64-encoded accept value
     */
    std::string calculateWebSocketAccept(const std::string &key) const;

private:
    /**
     * @brief Perform WebSocket handshake
     * @param socket TCP socket
     * @param host Server host
     * @param port Server port
     * @param path WebSocket path
     * @return true if handshake was successful
     */
    bool performHandshake(int socket, const std::string &host, int port, const std::string &path);

    /**
     * @brief Base64 encode data
     * @param data Data to encode
     * @return Base64-encoded string
     */
    std::string base64Encode(const std::vector<uint8_t> &data) const;

    /**
     * @brief SHA1 hash calculation
     * @param data Data to hash
     * @return SHA1 hash
     */
    std::vector<uint8_t> sha1Hash(const std::string &data) const;

    /**
     * @brief Connection thread main function
     * @param connection Connection to handle
     */
    void connectionThreadMain(std::shared_ptr<WebSocketConnection> connection);

    /**
     * @brief Handle incoming WebSocket frame
     * @param connection Connection that received the frame
     * @param frame Received frame
     */
    void handleIncomingFrame(std::shared_ptr<WebSocketConnection> connection, const WebSocketFrame &frame);

    /**
     * @brief Send queued frames for connection
     * @param connection Connection to send frames for
     */
    void sendQueuedFrames(std::shared_ptr<WebSocketConnection> connection);

    /**
     * @brief Handle connection timeout and keepalive
     * @param connection Connection to check
     */
    void handleKeepalive(std::shared_ptr<WebSocketConnection> connection);

    /**
     * @brief Process delayed sends
     */
    void processDelayedSends();

    /**
     * @brief Extract WebSocket parameters from SCXML params
     * @param params SCXML send parameters
     * @param connectionId Connection ID (output)
     * @param messageType Message type (output)
     * @param message Message content (output)
     */
    void extractWebSocketParameters(const SendParameters &params, std::string &connectionId, std::string &messageType,
                                    std::string &message) const;

    /**
     * @brief Generate unique connection ID
     * @return Unique connection identifier
     */
    std::string generateConnectionId();

    /**
     * @brief Create connection established event
     * @param connectionId Connection identifier
     * @return WebSocket connection event
     */
    Events::EventPtr createConnectionEvent(const std::string &connectionId, const std::string &eventType);

    /**
     * @brief Create message received event
     * @param connectionId Connection identifier
     * @param frame Received frame
     * @return WebSocket message event
     */
    Events::EventPtr createMessageEvent(const std::string &connectionId, const WebSocketFrame &frame);

    Config config_;

    // Connection management
    mutable std::mutex connectionsMutex_;
    std::unordered_map<std::string, std::shared_ptr<WebSocketConnection>> connections_;

    // Delayed sends
    mutable std::mutex delayedSendsMutex_;
    std::condition_variable delayedSendCondition_;
    std::queue<PendingWebSocketSend> delayedSends_;
    std::thread delayedSendThread_;

    // Connection ID generation
    mutable std::mutex connectionIdMutex_;
    std::atomic<uint64_t> nextConnectionId_{1};

    // Shutdown management
    std::atomic<bool> shutdownRequested_{false};
};

}  // namespace Runtime
}  // namespace SCXML