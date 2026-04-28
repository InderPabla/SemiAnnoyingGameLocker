#pragma once
#include <string>
#include <vector>
#include <optional>
#include <utility>
#include <cstdint>
#include "types.h"

class Config {
public:
    static std::string dataDir();
    static std::string locksDir();
    static std::string logsDir();
    static void        ensureDirectories();
    static std::string lockFilePath(const std::string& lockId);

    static bool                       save(const LockConfig& lc);
    static std::optional<LockConfig>  load(const std::string& lockId);
    static bool                       remove(const std::string& lockId);
    static std::vector<LockConfig>    loadAll();
    static std::vector<LockConfig>    findByGame(const std::string& gameName);

    static std::string                          isoNow();
    static std::string                          isoAdd(int64_t seconds);
    static int64_t                              secondsUntil(const std::string& iso);
    static std::string                          formatDisplay(const std::string& iso);
    static std::pair<std::string, std::string>  formatSchtasks(const std::string& iso);
    static std::string                          generateLockId(const std::vector<std::string>& games);
};
