#include "logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <mutex>

// Anonymous namespace keeps these strictly internal to this translation unit.
namespace {
    std::string g_path;
    std::mutex  g_mu;

    std::string timestamp() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        std::ostringstream o;
        o << std::setfill('0')
          << std::setw(4) << st.wYear   << "-"
          << std::setw(2) << st.wMonth  << "-"
          << std::setw(2) << st.wDay    << " "
          << std::setw(2) << st.wHour   << ":"
          << std::setw(2) << st.wMinute << ":"
          << std::setw(2) << st.wSecond;
        return o.str();
    }

    void append(const std::string& msg) {
        if (g_path.empty()) return;
        std::lock_guard lk(g_mu);
        if (std::ofstream f{g_path, std::ios::app})
            f << "[" << timestamp() << "] " << msg << "\n";
    }
}

void Logger::init(const std::string& path) { g_path = path; }

void Logger::log(const std::string& msg)   { append(msg); }

void Logger::info(const std::string& msg) {
    std::cout << msg << "\n";
    append(msg);
}

void Logger::error(const std::string& msg) {
    std::cerr << "ERROR: " << msg << "\n";
    append("ERROR: " + msg);
}
