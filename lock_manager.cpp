#include "lock_manager.h"
#include "config.h"
#include "logger.h"
#include "duration_parser.h"
#include "steam_resolver.h"
#include "friction_engine.h"
#include <iostream>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

LockManager::LockManager(FirewallManager& fw, SchedulerManager& sched)
    : fw_(fw), sched_(sched) {}

// ---- LOCK -----------------------------------------------------------------

int LockManager::lock(const CliArgs& args) {
    Logger::log("LOCK START games=" + joinVec(args.games) + " duration=" + args.durationStr);

    int64_t durationSecs = 0;
    try {
        durationSecs = ParseDuration(args.durationStr);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    // Resolve executables
    std::vector<std::string> exePaths;

    for (const auto& game : args.games) {
        std::cout << "Searching for game: " << game << "...\n";
        auto candidates = SteamResolver::resolve(game);

        if (candidates.empty()) {
            std::cout << "  Not found automatically.\n"
                      << "  Provide full path (blank to skip): ";
            std::string manual;
            std::getline(std::cin, manual);
            while (!manual.empty() && (manual.back() == '\r' || manual.back() == ' '))
                manual.pop_back();
            if (!manual.empty()) {
                if (!fs::exists(manual)) { std::cerr << "File not found: " << manual << "\n"; return 1; }
                exePaths.push_back(manual);
            }
        } else if (candidates.size() == 1) {
            std::cout << "  Found: " << candidates[0] << "\n";
            exePaths.push_back(candidates[0]);
        } else {
            std::cout << "  Multiple executables found:\n";
            size_t show = std::min(candidates.size(), size_t{10});
            for (size_t i = 0; i < show; ++i)
                std::cout << "    [" << (i + 1) << "] " << candidates[i] << "\n";
            std::cout << "  Select [1-" << show << "]: ";
            std::string sel;
            std::getline(std::cin, sel);
            int idx = 0;
            try { idx = std::stoi(sel) - 1; } catch (...) {}
            if (idx < 0 || idx >= static_cast<int>(candidates.size())) idx = 0;
            exePaths.push_back(candidates[idx]);
        }
    }

    for (const auto& e : args.exes) {
        if (!fs::exists(e)) { std::cerr << "Error: not found: " << e << "\n"; return 1; }
        exePaths.push_back(e);
    }

    if (exePaths.empty()) { std::cerr << "Error: no valid executables.\n"; return 1; }

    std::string createdAt = Config::isoNow();
    std::string unlockAt  = Config::isoAdd(durationSecs);
    auto [schedDate, schedTime] = Config::formatSchtasks(unlockAt);
    std::string lockId    = Config::generateLockId(args.games);

    // Preview
    std::cout << "\n=== LOCK PLAN ===\n"
              << "Lock ID:    " << lockId << "\n";
    if (!args.games.empty())
        std::cout << "Games:      " << joinVec(args.games) << "\n";
    std::cout << "Unlocks:    " << Config::formatDisplay(unlockAt) << "\n"
              << "Duration:   " << FormatRemaining(durationSecs) << "\n"
              << "Executables:\n";
    for (const auto& e : exePaths) std::cout << "  " << e << "\n";
    std::cout << "Rules to create:\n";
    for (const auto& e : exePaths)
        std::cout << "  " << FirewallManager::ruleName(lockId, e) << "\n";
    std::cout << "=================\n\n";

    if (args.dryRun) { std::cout << "[DRY RUN] No changes made.\n"; return 0; }

    if (!args.yes) {
        std::cout << "Proceed with lock? [y/N]: ";
        std::string confirm;
        std::getline(std::cin, confirm);
        while (!confirm.empty() && (confirm.back() == '\r' || confirm.back() == ' '))
            confirm.pop_back();
        if (confirm != "y" && confirm != "Y" && confirm != "yes") {
            std::cout << "Aborted.\n"; return 0;
        }
    }

    FrictionConfig friction;
    if (args.askPassword) {
        std::cout << "\nSetting up password...\n";
        friction.passwordEnabled = true;
        friction.passwordHash    = FrictionEngine::setupPassword();
        std::cout << "Password set.\n";
    }
    if (!args.triviaConfig.empty()) {
        friction.triviaEnabled = true;
        friction.triviaFile    = args.triviaConfig;
    }

    std::cout << "\nCreating firewall rules...\n";
    std::vector<std::string> createdRules;
    for (const auto& exe : exePaths) {
        std::string rule = fw_.addRule(lockId, exe);
        if (rule.empty()) {
            std::cerr << "Error creating rule for: " << exe << "\n";
            for (const auto& r : createdRules) fw_.deleteRule(r);
            return 1;
        }
        createdRules.push_back(rule);
        std::cout << "  Created: " << rule << "\n";
    }

    LockConfig lc{ lockId, args.games, exePaths, createdAt, unlockAt,
                   createdRules, {}, friction };
    if (!Config::save(lc)) {
        for (const auto& r : createdRules) fw_.deleteRule(r);
        return 1;
    }

    std::cout << "Creating scheduled task...\n";
    std::string taskN = sched_.createTask(lockId, SchedulerManager::selfExePath(),
                                          unlockAt, schedDate, schedTime);
    if (!taskN.empty()) {
        lc.scheduledTask = taskN;
        Config::save(lc);
        std::cout << "  Created: " << taskN << "\n";
    } else {
        std::cerr << "Warning: scheduled task not created. Lock is active but won't auto-unlock.\n";
    }

    std::cout << "\nLock active! " << lockId << " until "
              << Config::formatDisplay(unlockAt) << "\n";
    Logger::log("LOCK COMPLETE " + lockId);
    return 0;
}

// ---- UNLOCK ---------------------------------------------------------------

int LockManager::unlock(const CliArgs& args) {
    std::optional<LockConfig> lc;

    if (args.lockId.has_value()) {
        lc = Config::load(*args.lockId);
        if (!lc) { std::cerr << "Lock not found: " << *args.lockId << "\n"; return 1; }
    } else if (!args.games.empty()) {
        auto matches = Config::findByGame(args.games[0]);
        if (matches.empty()) { std::cerr << "No lock for game: " << args.games[0] << "\n"; return 1; }
        lc = matches[0];
    } else {
        std::cerr << "No lock specified.\n"; return 1;
    }

    Logger::log("UNLOCK START " + lc->lockId + (args.scheduled ? " [scheduled]" : " [manual]"));

    if (!args.scheduled) {
        FrictionEngine::runUnlockFriction(
            lc->friction.passwordEnabled, lc->friction.passwordHash,
            lc->friction.triviaEnabled,   lc->friction.triviaFile,
            Config::secondsUntil(lc->unlockAt));
    }
    return doUnlock(*lc);
}

// ---- LIST -----------------------------------------------------------------

int LockManager::list(const CliArgs&) {
    auto locks = Config::loadAll();
    if (locks.empty()) { std::cout << "No active locks.\n"; return 0; }

    std::cout << "[ACTIVE LOCKS]\n\n";
    for (const auto& lc : locks) {
        int64_t rem = Config::secondsUntil(lc.unlockAt);
        std::cout << "ID:          " << lc.lockId << "\n";
        if (!lc.games.empty()) std::cout << "Games:       " << joinVec(lc.games) << "\n";
        std::cout << "Ends:        " << Config::formatDisplay(lc.unlockAt) << "\n"
                  << "Remaining:   " << (rem > 0 ? FormatRemaining(rem) : "EXPIRED") << "\n"
                  << "Executables: " << lc.executables.size() << "\n";
        if (lc.friction.passwordEnabled) std::cout << "Protection:  Password\n";
        if (lc.friction.triviaEnabled)   std::cout << "             Trivia\n";
        std::cout << "\n";
    }
    return 0;
}

// ---- STATUS ---------------------------------------------------------------

int LockManager::status(const CliArgs& args) {
    std::optional<LockConfig> lc;
    if (args.lockId) lc = Config::load(*args.lockId);
    else if (!args.games.empty()) {
        auto m = Config::findByGame(args.games[0]);
        if (!m.empty()) lc = m[0];
    }
    if (!lc) { std::cerr << "Lock not found.\n"; return 1; }

    int64_t rem = Config::secondsUntil(lc->unlockAt);
    std::cout << "=== LOCK STATUS ===\n"
              << "ID:         " << lc->lockId << "\n";
    if (!lc->games.empty()) std::cout << "Games:      " << joinVec(lc->games) << "\n";
    std::cout << "Created:    " << Config::formatDisplay(lc->createdAt) << "\n"
              << "Unlocks:    " << Config::formatDisplay(lc->unlockAt) << "\n"
              << "Remaining:  " << (rem > 0 ? FormatRemaining(rem) : "EXPIRED") << "\n"
              << "Executables (" << lc->executables.size() << "):\n";
    for (const auto& e : lc->executables) std::cout << "  " << e << "\n";
    std::cout << "Firewall rules:\n";
    for (const auto& r : lc->firewallRules)
        std::cout << "  " << r << (fw_.ruleExists(r) ? " [ACTIVE]" : " [MISSING]") << "\n";
    if (!lc->scheduledTask.empty())
        std::cout << "Task:       " << lc->scheduledTask
                  << (sched_.taskExists(lc->scheduledTask) ? " [ACTIVE]" : " [GONE]") << "\n";
    std::cout << "Password:   " << (lc->friction.passwordEnabled ? "Yes" : "No") << "\n"
              << "Trivia:     " << (lc->friction.triviaEnabled ? lc->friction.triviaFile : "No") << "\n"
              << "===================\n";
    return 0;
}

// ---- CLEANUP --------------------------------------------------------------

int LockManager::cleanup(const CliArgs&) {
    std::cout << "Running cleanup...\n";
    Logger::log("CLEANUP START");

    auto locks = Config::loadAll();
    std::set<std::string> knownRules, knownTasks;
    for (const auto& lc : locks) {
        for (const auto& r : lc.firewallRules) knownRules.insert(r);
        if (!lc.scheduledTask.empty()) knownTasks.insert(lc.scheduledTask);
    }

    int removedRules = 0, removedTasks = 0, removedLocks = 0;

    for (const auto& r : fw_.listSaglRules())
        if (!knownRules.count(r)) { std::cout << "  Orphaned rule: " << r << "\n"; fw_.deleteRule(r); ++removedRules; }

    for (const auto& t : sched_.listSaglTasks())
        if (!knownTasks.count(t)) { std::cout << "  Orphaned task: " << t << "\n"; sched_.deleteTask(t); ++removedTasks; }

    for (const auto& lc : locks)
        if (Config::secondsUntil(lc.unlockAt) == 0) {
            std::cout << "  Expired lock: " << lc.lockId << "\n";
            doUnlock(lc); ++removedLocks;
        }

    std::cout << "Done. Removed: " << removedRules << " rules, "
              << removedTasks << " tasks, " << removedLocks << " expired locks.\n";
    Logger::log("CLEANUP DONE");
    return 0;
}

// ---- Private helpers ------------------------------------------------------

int LockManager::doUnlock(const LockConfig& lc) {
    std::cout << "Removing firewall rules...\n";
    for (const auto& r : lc.firewallRules) fw_.deleteRule(r);
    if (!lc.scheduledTask.empty()) sched_.deleteTask(lc.scheduledTask);
    Config::remove(lc.lockId);
    std::cout << "Lock " << lc.lockId << " removed.\n";
    Logger::log("UNLOCK COMPLETE " + lc.lockId);
    return 0;
}

std::string LockManager::joinVec(const std::vector<std::string>& v, const std::string& sep) {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) { if (i) r += sep; r += v[i]; }
    return r;
}
