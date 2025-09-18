#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <mutex>


namespace RSM {

class Logger
{
private:
    static std::mutex logMutex_;

public:
    enum class Level
    {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    static void log(Level level, const std::string &message);
    static void debug(const std::string &message);
    static void info(const std::string &message);
    static void warning(const std::string &message);
    static void error(const std::string &message);

private:
    static std::string getLevelString(Level level);
};


}  // namespace RSM