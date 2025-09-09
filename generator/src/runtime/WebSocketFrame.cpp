#include "../../include/runtime/WebSocketFrame.h"
#include "common/Logger.h"
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

using namespace SCXML::Runtime;

WebSocketFrame::WebSocketFrame(OpCode opcode, const std::vector<uint8_t> &payload)
    : opcode_(opcode), payloadLength_(payload.size()), payload_(payload) {}

WebSocketFrame::WebSocketFrame(const std::string &text) : opcode_(OpCode::TEXT), payloadLength_(text.length()) {
    payload_.assign(text.begin(), text.end());
}

WebSocketFrame::WebSocketFrame(CloseCode closeCode, const std::string &reason) : opcode_(OpCode::CLOSE) {
    // Close frame payload: 2 bytes close code + reason string
    payload_.resize(2 + reason.length());

    // Close code in network byte order (big endian)
    uint16_t code = static_cast<uint16_t>(closeCode);
    payload_[0] = (code >> 8) & 0xFF;
    payload_[1] = code & 0xFF;

    // Reason string
    if (!reason.empty()) {
        std::copy(reason.begin(), reason.end(), payload_.begin() + 2);
    }

    payloadLength_ = payload_.size();
}

bool WebSocketFrame::parseFromData(const uint8_t *data, size_t length) {
    if (length < 2) {
        Logger::error("WebSocketFrame: Frame too short for basic header");
        return false;
    }

    size_t offset = 0;

    // First byte: FIN + RSV + Opcode
    uint8_t firstByte = data[offset++];
    fin_ = (firstByte & 0x80) != 0;
    rsv1_ = (firstByte & 0x40) != 0;
    rsv2_ = (firstByte & 0x20) != 0;
    rsv3_ = (firstByte & 0x10) != 0;
    opcode_ = static_cast<OpCode>(firstByte & 0x0F);

    // Second byte: MASK + Payload length
    uint8_t secondByte = data[offset++];
    masked_ = (secondByte & 0x80) != 0;
    uint8_t payloadLenIndicator = secondByte & 0x7F;

    // Determine actual payload length
    if (payloadLenIndicator < 126) {
        payloadLength_ = payloadLenIndicator;
    } else if (payloadLenIndicator == 126) {
        if (length < offset + 2) {
            Logger::error("WebSocketFrame: Not enough data for 16-bit length");
            return false;
        }
        payloadLength_ = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
    } else if (payloadLenIndicator == 127) {
        if (length < offset + 8) {
            Logger::error("WebSocketFrame: Not enough data for 64-bit length");
            return false;
        }
        payloadLength_ = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLength_ = (payloadLength_ << 8) | data[offset + i];
        }
        offset += 8;
    }

    // Masking key (if present)
    if (masked_) {
        if (length < offset + 4) {
            Logger::error("WebSocketFrame: Not enough data for masking key");
            return false;
        }
        maskingKey_ = (static_cast<uint32_t>(data[offset]) << 24) | (static_cast<uint32_t>(data[offset + 1]) << 16) |
                      (static_cast<uint32_t>(data[offset + 2]) << 8) | static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
    }

    // Payload data
    if (length < offset + payloadLength_) {
        Logger::error("WebSocketFrame: Not enough data for payload");
        return false;
    }

    payload_.resize(payloadLength_);
    if (payloadLength_ > 0) {
        std::copy(data + offset, data + offset + payloadLength_, payload_.begin());

        // Unmask payload if masked
        if (masked_) {
            applyMask(payload_, maskingKey_);
        }
    }

    return isValid();
}

std::vector<uint8_t> WebSocketFrame::serialize(bool maskFrame) const {
    std::vector<uint8_t> result;

    // First byte: FIN + RSV + Opcode
    uint8_t firstByte = (fin_ ? 0x80 : 0x00) | (rsv1_ ? 0x40 : 0x00) | (rsv2_ ? 0x20 : 0x00) | (rsv3_ ? 0x10 : 0x00) |
                        static_cast<uint8_t>(opcode_);
    result.push_back(firstByte);

    // Determine payload length encoding
    uint64_t payloadLen = payload_.size();
    uint8_t secondByte = maskFrame ? 0x80 : 0x00;

    if (payloadLen < 126) {
        secondByte |= static_cast<uint8_t>(payloadLen);
        result.push_back(secondByte);
    } else if (payloadLen <= 0xFFFF) {
        secondByte |= 126;
        result.push_back(secondByte);
        result.push_back((payloadLen >> 8) & 0xFF);
        result.push_back(payloadLen & 0xFF);
    } else {
        secondByte |= 127;
        result.push_back(secondByte);
        for (int i = 7; i >= 0; --i) {
            result.push_back((payloadLen >> (i * 8)) & 0xFF);
        }
    }

    // Masking key (if masking is enabled)
    uint32_t maskKey = 0;
    if (maskFrame) {
        maskKey = generateMaskingKey();
        result.push_back((maskKey >> 24) & 0xFF);
        result.push_back((maskKey >> 16) & 0xFF);
        result.push_back((maskKey >> 8) & 0xFF);
        result.push_back(maskKey & 0xFF);
    }

    // Payload data
    std::vector<uint8_t> payloadCopy = payload_;
    if (maskFrame && !payloadCopy.empty()) {
        applyMask(payloadCopy, maskKey);
    }

    result.insert(result.end(), payloadCopy.begin(), payloadCopy.end());

    return result;
}

WebSocketFrame WebSocketFrame::createTextFrame(const std::string &text) {
    return WebSocketFrame(text);
}

WebSocketFrame WebSocketFrame::createBinaryFrame(const std::vector<uint8_t> &data) {
    return WebSocketFrame(OpCode::BINARY, data);
}

WebSocketFrame WebSocketFrame::createPingFrame(const std::vector<uint8_t> &payload) {
    return WebSocketFrame(OpCode::PING, payload);
}

WebSocketFrame WebSocketFrame::createPongFrame(const std::vector<uint8_t> &payload) {
    return WebSocketFrame(OpCode::PONG, payload);
}

WebSocketFrame WebSocketFrame::createCloseFrame(CloseCode closeCode, const std::string &reason) {
    return WebSocketFrame(closeCode, reason);
}

std::string WebSocketFrame::getPayloadAsText() const {
    if (opcode_ != OpCode::TEXT && opcode_ != OpCode::CLOSE) {
        Logger::debug("WebSocketFrame: Getting text from non-text frame");
    }

    return std::string(payload_.begin(), payload_.end());
}

bool WebSocketFrame::isControlFrame() const {
    return opcode_ == OpCode::CLOSE || opcode_ == OpCode::PING || opcode_ == OpCode::PONG;
}

bool WebSocketFrame::isDataFrame() const {
    return opcode_ == OpCode::TEXT || opcode_ == OpCode::BINARY || opcode_ == OpCode::CONTINUATION;
}

WebSocketFrame::CloseCode WebSocketFrame::getCloseCode() const {
    if (opcode_ != OpCode::CLOSE || payload_.size() < 2) {
        return CloseCode::NORMAL_CLOSURE;
    }

    uint16_t code = (static_cast<uint16_t>(payload_[0]) << 8) | payload_[1];
    return static_cast<CloseCode>(code);
}

std::string WebSocketFrame::getCloseReason() const {
    if (opcode_ != OpCode::CLOSE || payload_.size() <= 2) {
        return "";
    }

    return std::string(payload_.begin() + 2, payload_.end());
}

void WebSocketFrame::applyMask(std::vector<uint8_t> &data, uint32_t maskingKey) const {
    uint8_t maskBytes[4] = {static_cast<uint8_t>((maskingKey >> 24) & 0xFF),
                            static_cast<uint8_t>((maskingKey >> 16) & 0xFF),
                            static_cast<uint8_t>((maskingKey >> 8) & 0xFF), static_cast<uint8_t>(maskingKey & 0xFF)};

    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= maskBytes[i % 4];
    }
}

uint32_t WebSocketFrame::generateMaskingKey() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

bool WebSocketFrame::isValid() const {
    // Check reserved bits (RSV1-3 must be 0 unless extensions are negotiated)
    if (rsv1_ || rsv2_ || rsv3_) {
        Logger::error("WebSocketFrame: Reserved bits must be 0");
        return false;
    }

    // Check opcode validity
    uint8_t opcodeValue = static_cast<uint8_t>(opcode_);
    if (opcodeValue > 2 && opcodeValue < 8) {
        Logger::error("WebSocketFrame: Invalid opcode: " + std::to_string(opcodeValue));
        return false;
    }
    if (opcodeValue > 0xA) {
        Logger::error("WebSocketFrame: Invalid opcode: " + std::to_string(opcodeValue));
        return false;
    }

    // Control frames must have FIN=1 and payload <= 125 bytes
    if (isControlFrame()) {
        if (!fin_) {
            Logger::error("WebSocketFrame: Control frames must have FIN=1");
            return false;
        }
        if (payloadLength_ > 125) {
            Logger::error("WebSocketFrame: Control frame payload too large: " + std::to_string(payloadLength_));
            return false;
        }
    }

    // Close frame payload validation
    if (opcode_ == OpCode::CLOSE && payloadLength_ > 0) {
        if (payloadLength_ < 2) {
            Logger::error("WebSocketFrame: Close frame payload must be at least 2 bytes");
            return false;
        }

        uint16_t closeCode = static_cast<uint16_t>(getCloseCode());
        if (closeCode < 1000 || (closeCode >= 1004 && closeCode <= 1006) || (closeCode >= 1015 && closeCode < 3000)) {
            Logger::error("WebSocketFrame: Invalid close code: " + std::to_string(closeCode));
            return false;
        }
    }

    return true;
}