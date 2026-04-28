#pragma once
#include <string>
#include <vector>
#include <optional>

struct FrictionConfig {
    bool passwordEnabled = false;
    std::string passwordHash;
    bool triviaEnabled = false;
    std::string triviaFile;
};

struct LockConfig {
    std::string lockId;
    std::vector<std::string> games;
    std::vector<std::string> executables;
    std::string createdAt;
    std::string unlockAt;
    std::vector<std::string> firewallRules;
    std::string scheduledTask;
    FrictionConfig friction;
};

struct CliArgs {
    std::string command;
    std::vector<std::string> games;
    std::vector<std::string> exes;
    std::string durationStr;
    std::optional<std::string> lockId;
    bool dryRun    = false;
    bool askPassword = false;
    std::string triviaConfig;
    bool yes       = false;
    bool scheduled = false;
};
