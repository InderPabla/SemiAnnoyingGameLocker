#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include "cli_parser.h"
#include "lock_manager.h"
#include "config.h"
#include "logger.h"
#include "shell.h"

static bool isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

int main(int argc, char* argv[]) {
    Config::ensureDirectories();
    Logger::init(Config::logsDir() + "\\sagl.log");

    if (!isRunningAsAdmin()) {
        std::cerr << "Error: must be run as Administrator (required for netsh and schtasks).\n";
        return 1;
    }

    CliArgs args;
    try {
        args = CliParser::parse(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    SystemShell    shell;
    FirewallManager  fw(shell);
    SchedulerManager sched(shell);
    LockManager      lm(fw, sched);

    if (args.command == "lock")    return lm.lock(args);
    if (args.command == "unlock")  return lm.unlock(args);
    if (args.command == "list")    return lm.list(args);
    if (args.command == "status")  return lm.status(args);
    if (args.command == "cleanup") return lm.cleanup(args);

    return 1;
}
