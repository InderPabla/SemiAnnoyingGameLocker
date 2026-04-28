#include "config.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include "logger.h"

namespace fs = std::filesystem;
using json   = nlohmann::json;

// ---- Serialization helpers ------------------------------------------------

static json toJson(const LockConfig& lc) {
    return json{
        {"lockId",        lc.lockId},
        {"games",         lc.games},
        {"executables",   lc.executables},
        {"createdAt",     lc.createdAt},
        {"unlockAt",      lc.unlockAt},
        {"firewallRules", lc.firewallRules},
        {"scheduledTask", lc.scheduledTask},
        {"friction", {
            {"passwordEnabled", lc.friction.passwordEnabled},
            {"passwordHash",    lc.friction.passwordHash},
            {"triviaEnabled",   lc.friction.triviaEnabled},
            {"triviaFile",      lc.friction.triviaFile}
        }}
    };
}

static LockConfig fromJson(const json& j) {
    LockConfig lc;
    lc.lockId        = j.at("lockId").get<std::string>();
    lc.createdAt     = j.at("createdAt").get<std::string>();
    lc.unlockAt      = j.at("unlockAt").get<std::string>();
    lc.scheduledTask = j.at("scheduledTask").get<std::string>();
    lc.games         = j.at("games").get<std::vector<std::string>>();
    lc.executables   = j.at("executables").get<std::vector<std::string>>();
    lc.firewallRules = j.at("firewallRules").get<std::vector<std::string>>();
    const auto& fr          = j.at("friction");
    lc.friction.passwordEnabled = fr.at("passwordEnabled").get<bool>();
    lc.friction.passwordHash    = fr.at("passwordHash").get<std::string>();
    lc.friction.triviaEnabled   = fr.at("triviaEnabled").get<bool>();
    lc.friction.triviaFile      = fr.at("triviaFile").get<std::string>();
    return lc;
}

// ---- Windows time helpers -------------------------------------------------

static SYSTEMTIME currentST() { SYSTEMTIME st; GetLocalTime(&st); return st; }

static std::string isoFrom(const SYSTEMTIME& st) {
    std::ostringstream o;
    o << std::setfill('0')
      << std::setw(4) << st.wYear   << "-" << std::setw(2) << st.wMonth  << "-"
      << std::setw(2) << st.wDay    << "T" << std::setw(2) << st.wHour   << ":"
      << std::setw(2) << st.wMinute << ":" << std::setw(2) << st.wSecond;
    return o.str();
}

static SYSTEMTIME parseIso(const std::string& iso) {
    SYSTEMTIME st{};
    if (iso.size() < 19) return st;
    st.wYear   = static_cast<WORD>(std::stoi(iso.substr(0,  4)));
    st.wMonth  = static_cast<WORD>(std::stoi(iso.substr(5,  2)));
    st.wDay    = static_cast<WORD>(std::stoi(iso.substr(8,  2)));
    st.wHour   = static_cast<WORD>(std::stoi(iso.substr(11, 2)));
    st.wMinute = static_cast<WORD>(std::stoi(iso.substr(14, 2)));
    st.wSecond = static_cast<WORD>(std::stoi(iso.substr(17, 2)));
    return st;
}

static FILETIME toFILETIME(const SYSTEMTIME& st) {
    FILETIME ft; SystemTimeToFileTime(&st, &ft); return ft;
}

static ULARGE_INTEGER toULI(const FILETIME& ft) {
    return { ft.dwLowDateTime, ft.dwHighDateTime };
}

static std::string sanitize(const std::string& s) {
    std::string r;
    for (char c : s)
        r += (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') ? c : '_';
    return r.empty() ? "Lock" : r;
}

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

// ---- Public API -----------------------------------------------------------

std::string Config::dataDir() {
    const char* pd = getenv("PROGRAMDATA");
    return (pd ? std::string(pd) : "C:\\ProgramData") + "\\SemiAnnoyingGameLocker";
}
std::string Config::locksDir() { return dataDir() + "\\locks"; }
std::string Config::logsDir()  { return dataDir() + "\\logs";  }

void Config::ensureDirectories() {
    for (const auto& d : {dataDir(), locksDir(), logsDir()}) {
        std::error_code ec;
        fs::create_directories(d, ec);
    }
}

std::string Config::lockFilePath(const std::string& lockId) {
    return locksDir() + "\\" + lockId + ".json";
}

bool Config::save(const LockConfig& lc) {
    ensureDirectories();
    std::string path = lockFilePath(lc.lockId);
    std::ofstream f(path);
    if (!f) { Logger::error("Cannot write lock file: " + path); return false; }
    f << toJson(lc).dump(2);
    Logger::log("LOCK CONFIG SAVED " + path);
    return true;
}

std::optional<LockConfig> Config::load(const std::string& lockId) {
    try {
        std::ifstream f(lockFilePath(lockId));
        if (!f) return std::nullopt;
        return fromJson(json::parse(f));
    } catch (const std::exception& ex) {
        Logger::error("Failed to load lock " + lockId + ": " + ex.what());
        return std::nullopt;
    }
}

bool Config::remove(const std::string& lockId) {
    std::error_code ec;
    fs::remove(lockFilePath(lockId), ec);
    if (ec) { Logger::error("Failed to remove lock file: " + lockId); return false; }
    Logger::log("LOCK CONFIG REMOVED " + lockId);
    return true;
}

std::vector<LockConfig> Config::loadAll() {
    std::vector<LockConfig> result;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(locksDir(), ec)) {
        if (entry.path().extension() != ".json") continue;
        auto lc = load(entry.path().stem().string());
        if (lc) result.push_back(*lc);
    }
    return result;
}

std::vector<LockConfig> Config::findByGame(const std::string& gameName) {
    std::string lower = toLower(gameName);
    std::vector<LockConfig> result;
    for (const auto& lc : loadAll())
        for (const auto& g : lc.games)
            if (toLower(g) == lower) { result.push_back(lc); break; }
    return result;
}

std::string Config::isoNow() { return isoFrom(currentST()); }

std::string Config::isoAdd(int64_t seconds) {
    SYSTEMTIME st = currentST();
    FILETIME ft   = toFILETIME(st);
    ULARGE_INTEGER uli = toULI(ft);
    uli.QuadPart += static_cast<ULONGLONG>(seconds) * 10'000'000ULL;
    FILETIME ft2{ uli.LowPart, uli.HighPart };
    SYSTEMTIME st2;
    FileTimeToSystemTime(&ft2, &st2);
    return isoFrom(st2);
}

int64_t Config::secondsUntil(const std::string& iso) {
    SYSTEMTIME stNow = currentST();
    FILETIME ftTarget = toFILETIME(parseIso(iso));
    FILETIME ftNow    = toFILETIME(stNow);
    ULARGE_INTEGER t  = toULI(ftTarget);
    ULARGE_INTEGER n  = toULI(ftNow);
    return t.QuadPart > n.QuadPart
        ? static_cast<int64_t>((t.QuadPart - n.QuadPart) / 10'000'000ULL) : 0;
}

std::string Config::formatDisplay(const std::string& iso) {
    static constexpr const char* months[] = {
        "","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
    };
    SYSTEMTIME st = parseIso(iso);
    int h = st.wHour, h12 = h % 12;
    if (h12 == 0) h12 = 12;
    std::ostringstream o;
    o << months[st.wMonth] << " " << st.wDay << ", " << st.wYear << " "
      << h12 << ":" << std::setw(2) << std::setfill('0') << st.wMinute
      << (h >= 12 ? " PM" : " AM");
    return o.str();
}

std::pair<std::string, std::string> Config::formatSchtasks(const std::string& iso) {
    SYSTEMTIME st = parseIso(iso);
    std::ostringstream date, time;
    date << std::setfill('0') << std::setw(2) << st.wMonth << "/"
         << std::setw(2) << st.wDay   << "/" << st.wYear;
    time << std::setfill('0') << std::setw(2) << st.wHour  << ":"
         << std::setw(2) << st.wMinute;
    return {date.str(), time.str()};
}

std::string Config::generateLockId(const std::vector<std::string>& games) {
    SYSTEMTIME st = currentST();
    std::ostringstream date;
    date << st.wYear
         << std::setfill('0') << std::setw(2) << st.wMonth
         << std::setw(2) << st.wDay;
    std::string name = games.empty() ? "Lock" : sanitize(games[0]);
    std::string base = date.str() + "-" + name;
    std::string id   = base;
    for (int n = 2; fs::exists(lockFilePath(id)); ++n)
        id = base + "-" + std::to_string(n);
    return id;
}
