#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>

namespace SCXML {
namespace Common {

class Logger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

private:
    static std::mutex logMutex_;
    static Level minLevel_;

public:
    static void setMinLevel(Level level);
    static void log(Level level, const std::string &message);
    static void debug(const std::string &message);
    static void info(const std::string &message);
    static void warning(const std::string &message);
    static void error(const std::string &message);

private:
    static std::string getLevelString(Level level);
};

}  // namespace Common
}  // namespace SCXML
