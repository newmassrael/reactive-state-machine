#include "Logger.h"

std::mutex Logger::logMutex_;

void Logger::log(Level level, const std::string &message)
{
    std::lock_guard<std::mutex> lock(logMutex_);

    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

    std::cout << std::put_time(std::localtime(&now_time_t), "%H:%M:%S")
              << '.' << std::setfill('0') << std::setw(3) << now_ms.count()
              << " [" << getLevelString(level) << "] "
              << message << std::endl;
}

void Logger::debug(const std::string &message)
{
    log(Level::DEBUG, message);
}

void Logger::info(const std::string &message)
{
    log(Level::INFO, message);
}

void Logger::warning(const std::string &message)
{
    log(Level::WARNING, message);
}

void Logger::error(const std::string &message)
{
    log(Level::ERROR, message);
}

std::string Logger::getLevelString(Level level)
{
    switch (level)
    {
    case Level::DEBUG:
        return "DEBUG";
    case Level::INFO:
        return "INFO ";
    case Level::WARNING:
        return "WARN ";
    case Level::ERROR:
        return "ERROR";
    default:
        return "?????";
    }
}
