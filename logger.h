#pragma once
#include <string>

class Logger {
public:
    static void init(const std::string& logFilePath);
    static void log(const std::string& msg);
    static void info(const std::string& msg);
    static void error(const std::string& msg);
};
