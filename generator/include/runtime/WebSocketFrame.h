#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SCXML {
namespace Runtime {

/**
 * @brief WebSocket frame structure according to RFC 6455
 *
 * Implements WebSocket protocol frame format for data transmission
 * between client and server in real-time communication.
 */
class WebSocketFrame {
public:
    /**
     * @brief WebSocket frame opcodes
     */
    enum class OpCode : uint8_t { CONTINUATION = 0x0, TEXT = 0x1, BINARY = 0x2, CLOSE = 0x8, PING = 0x9, PONG = 0xA };

    /**
     * @brief WebSocket close status codes
     */
    enum class CloseCode : uint16_t {
        NORMAL_CLOSURE = 1000,
        GOING_AWAY = 1001,
        PROTOCOL_ERROR = 1002,
        UNSUPPORTED_DATA = 1003,
        INVALID_FRAME_PAYLOAD_DATA = 1007,
        POLICY_VIOLATION = 1008,
        MESSAGE_TOO_BIG = 1009,
        INTERNAL_ERROR = 1011
    };

private:
    bool fin_ = true;               // Final fragment flag
    bool rsv1_ = false;             // Reserved bit 1
    bool rsv2_ = false;             // Reserved bit 2
    bool rsv3_ = false;             // Reserved bit 3
    OpCode opcode_ = OpCode::TEXT;  // Frame opcode
    bool masked_ = false;           // Mask flag
    uint64_t payloadLength_ = 0;    // Payload length
    uint32_t maskingKey_ = 0;       // Masking key (if masked)
    std::vector<uint8_t> payload_;  // Frame payload data

public:
    /**
     * @brief Default constructor
     */
    WebSocketFrame() = default;

    /**
     * @brief Constructor with opcode and payload
     * @param opcode Frame opcode
     * @param payload Frame payload data
     */
    WebSocketFrame(OpCode opcode, const std::vector<uint8_t> &payload);

    /**
     * @brief Constructor for text frames
     * @param text Text content
     */
    explicit WebSocketFrame(const std::string &text);

    /**
     * @brief Constructor for close frames
     * @param closeCode Close status code
     * @param reason Close reason (optional)
     */
    WebSocketFrame(CloseCode closeCode, const std::string &reason = "");

    /**
     * @brief Parse WebSocket frame from binary data
     * @param data Binary frame data
     * @param length Data length
     * @return true if parsing was successful
     */
    bool parseFromData(const uint8_t *data, size_t length);

    /**
     * @brief Serialize frame to binary data
     * @param maskFrame Whether to mask the frame (client-side)
     * @return Binary frame data
     */
    std::vector<uint8_t> serialize(bool maskFrame = false) const;

    /**
     * @brief Create text frame
     * @param text Text content
     * @return WebSocket text frame
     */
    static WebSocketFrame createTextFrame(const std::string &text);

    /**
     * @brief Create binary frame
     * @param data Binary data
     * @return WebSocket binary frame
     */
    static WebSocketFrame createBinaryFrame(const std::vector<uint8_t> &data);

    /**
     * @brief Create ping frame
     * @param payload Optional ping payload
     * @return WebSocket ping frame
     */
    static WebSocketFrame createPingFrame(const std::vector<uint8_t> &payload = {});

    /**
     * @brief Create pong frame
     * @param payload Pong payload (should match ping)
     * @return WebSocket pong frame
     */
    static WebSocketFrame createPongFrame(const std::vector<uint8_t> &payload = {});

    /**
     * @brief Create close frame
     * @param closeCode Close status code
     * @param reason Close reason
     * @return WebSocket close frame
     */
    static WebSocketFrame createCloseFrame(CloseCode closeCode, const std::string &reason = "");

    // Getters
    bool isFin() const {
        return fin_;
    }

    OpCode getOpCode() const {
        return opcode_;
    }

    bool isMasked() const {
        return masked_;
    }

    uint64_t getPayloadLength() const {
        return payloadLength_;
    }

    const std::vector<uint8_t> &getPayload() const {
        return payload_;
    }

    /**
     * @brief Get payload as text string
     * @return Text representation of payload
     */
    std::string getPayloadAsText() const;

    /**
     * @brief Check if frame is a control frame
     * @return true if control frame (close, ping, pong)
     */
    bool isControlFrame() const;

    /**
     * @brief Check if frame is a data frame
     * @return true if data frame (text, binary, continuation)
     */
    bool isDataFrame() const;

    /**
     * @brief Get close code from close frame payload
     * @return Close status code (only valid for close frames)
     */
    CloseCode getCloseCode() const;

    /**
     * @brief Get close reason from close frame payload
     * @return Close reason string (only valid for close frames)
     */
    std::string getCloseReason() const;

private:
    /**
     * @brief Apply XOR masking to payload data
     * @param data Data to mask/unmask
     * @param maskingKey 4-byte masking key
     */
    void applyMask(std::vector<uint8_t> &data, uint32_t maskingKey) const;

    /**
     * @brief Generate random masking key
     * @return 32-bit random masking key
     */
    uint32_t generateMaskingKey() const;

    /**
     * @brief Validate frame according to RFC 6455
     * @return true if frame is valid
     */
    bool isValid() const;
};

}  // namespace Runtime
}  // namespace SCXML