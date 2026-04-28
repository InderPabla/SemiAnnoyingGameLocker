#pragma once
#include "types.h"
#include "firewall_manager.h"
#include "scheduler_manager.h"

class LockManager {
public:
    LockManager(FirewallManager& fw, SchedulerManager& sched);

    int lock(const CliArgs& args);
    int unlock(const CliArgs& args);
    int list(const CliArgs& args);
    int status(const CliArgs& args);
    int cleanup(const CliArgs& args);

private:
    FirewallManager&  fw_;
    SchedulerManager& sched_;

    int doUnlock(const LockConfig& lc);
    static std::string joinVec(const std::vector<std::string>& v,
                               const std::string& sep = ", ");
};
