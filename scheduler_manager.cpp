#include "scheduler_manager.h"
#include "logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sstream>
#include <algorithm>

SchedulerManager::SchedulerManager(IShell& shell) : shell_(shell) {}

std::string SchedulerManager::taskName(const std::string& lockId) {
    return "SAGL_Unlock_" + lockId;
}

std::string SchedulerManager::selfExePath() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n),
                                     nullptr, 0, nullptr, nullptr);
    std::string s(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n),
                        s.data(), needed, nullptr, nullptr);
    return s;
}

std::string SchedulerManager::createTask(const std::string& lockId,
                                          const std::string& exePath,
                                          const std::string& unlockAtIso,
                                          const std::string& schedDate,
                                          const std::string& schedTime) {
    std::string name = taskName(lockId);
    std::string run  = "\"" + exePath + "\" unlock --id " + lockId + " --scheduled";

    std::ostringstream cmd;
    cmd << "schtasks /Create"
        << " /TN \""  << name      << "\""
        << " /TR \""  << run       << "\""
        << " /SC ONCE /SD " << schedDate
        << " /ST "          << schedTime
        << " /RL HIGHEST /Z /F >nul 2>&1";

    if (shell_.execute(cmd.str()) != 0) {
        Logger::error("Failed to create scheduled task: " + name);
        return {};
    }
    Logger::log("TASK CREATED " + name + " at " + unlockAtIso);
    return name;
}

bool SchedulerManager::deleteTask(const std::string& name) {
    std::string cmd = "schtasks /Delete /TN \"" + name + "\" /F >nul 2>&1";
    shell_.execute(cmd);   // task may already be gone via /Z — not an error either way
    Logger::log("TASK DELETED (or already gone) " + name);
    return true;
}

bool SchedulerManager::taskExists(const std::string& name) {
    return shell_.execute("schtasks /Query /TN \"" + name + "\" >nul 2>&1") == 0;
}

std::vector<std::string> SchedulerManager::listSaglTasks() {
    std::vector<std::string> tasks;
    std::string output = shell_.capture("schtasks /Query /FO CSV /NH 2>&1");

    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] != '"') continue;
        size_t end = line.find('"', 1);
        if (end == std::string::npos) continue;
        std::string name = line.substr(1, end - 1);
        if (!name.empty() && name[0] == '\\') name = name.substr(1);
        if (name.rfind("SAGL_Unlock_", 0) == 0) tasks.push_back(name);
    }
    return tasks;
}
