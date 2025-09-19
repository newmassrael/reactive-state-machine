#include "common/Logger.h"
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace RSM {

namespace {
std::shared_ptr<spdlog::logger> logger_;

void initializeLogger() {
    if (!logger_) {
        logger_ = spdlog::stdout_color_mt("RSM");
        logger_->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
        logger_->set_level(spdlog::level::debug);
    }
}
}  // namespace

// Detail namespace implementation
namespace detail {
std::shared_ptr<spdlog::logger> getLogger() {
    initializeLogger();
    return logger_;
}
}  // namespace detail

// === Logger Implementation ===

// Helper function to format message with function name
std::string RSM::Logger::formatWithFunction(const std::string &message, const std::source_location &location) {
    return std::string(location.function_name()) + "() - " + message;
}

void RSM::Logger::log(Level level, const std::string &message, const std::source_location &location) {
    initializeLogger();

    std::string formattedMessage = formatWithFunction(message, location);

    switch (level) {
    case Level::DEBUG:
        logger_->debug(formattedMessage);
        break;
    case Level::INFO:
        logger_->info(formattedMessage);
        break;
    case Level::WARNING:
        logger_->warn(formattedMessage);
        break;
    case Level::ERROR:
        logger_->error(formattedMessage);
        break;
    }
}

void RSM::Logger::debug(const std::string &message, const std::source_location &location) {
    log(Level::DEBUG, message, location);
}

void RSM::Logger::info(const std::string &message, const std::source_location &location) {
    log(Level::INFO, message, location);
}

void RSM::Logger::warn(const std::string &message, const std::source_location &location) {
    log(Level::WARNING, message, location);
}

void RSM::Logger::error(const std::string &message, const std::source_location &location) {
    log(Level::ERROR, message, location);
}

// Stream-based methods
RSM::LoggerStream RSM::Logger::debug(const std::source_location &location) {
    return LoggerStream(Level::DEBUG, location);
}

RSM::LoggerStream RSM::Logger::info(const std::source_location &location) {
    return LoggerStream(Level::INFO, location);
}

RSM::LoggerStream RSM::Logger::warn(const std::source_location &location) {
    return LoggerStream(Level::WARNING, location);
}

RSM::LoggerStream RSM::Logger::error(const std::source_location &location) {
    return LoggerStream(Level::ERROR, location);
}

// === LoggerStream Implementation ===

RSM::LoggerStream::LoggerStream(RSM::Logger::Level level, const std::source_location &location)
    : level_(level), location_(location) {}

RSM::LoggerStream::~LoggerStream() {
    // When LoggerStream is destroyed, flush the message to the logger
    std::string message = buffer_;
    if (!message.empty()) {
        RSM::Logger::log(level_, message, location_);
    }
}

RSM::LoggerStream::LoggerStream(RSM::LoggerStream &&other) noexcept
    : buffer_(std::move(other.buffer_)), level_(other.level_), location_(other.location_) {}

RSM::LoggerStream &RSM::LoggerStream::operator=(RSM::LoggerStream &&other) noexcept {
    if (this != &other) {
        buffer_ = std::move(other.buffer_);
        level_ = other.level_;
        location_ = other.location_;
    }
    return *this;
}

// Stream operators implementation
RSM::LoggerStream &RSM::LoggerStream::operator<<(const std::string &value) {
    buffer_ += value;
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(const char *value) {
    buffer_ += value;
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(int value) {
    buffer_ += std::to_string(value);
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(long value) {
    buffer_ += std::to_string(value);
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(long long value) {
    buffer_ += std::to_string(value);
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(unsigned int value) {
    buffer_ += std::to_string(value);
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(unsigned long value) {
    buffer_ += std::to_string(value);
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(unsigned long long value) {
    buffer_ += std::to_string(value);
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(float value) {
    buffer_ += std::to_string(value);
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(double value) {
    buffer_ += std::to_string(value);
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(bool value) {
    buffer_ += (value ? "true" : "false");
    return *this;
}

RSM::LoggerStream &RSM::LoggerStream::operator<<(char value) {
    buffer_ += value;
    return *this;
}

}  // namespace RSM
