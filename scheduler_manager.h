#pragma once
#include <string>
#include <vector>
#include "shell.h"

class SchedulerManager {
public:
    explicit SchedulerManager(IShell& shell);

    static std::string taskName(const std::string& lockId);
    static std::string selfExePath();

    std::string              createTask(const std::string& lockId,
                                        const std::string& exePath,
                                        const std::string& unlockAtIso,
                                        const std::string& schedDate,
                                        const std::string& schedTime);
    bool                     deleteTask(const std::string& name);
    bool                     taskExists(const std::string& name);
    std::vector<std::string> listSaglTasks();

private:
    IShell& shell_;
};
