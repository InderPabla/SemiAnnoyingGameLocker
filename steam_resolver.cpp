#include "steam_resolver.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include "logger.h"

namespace fs = std::filesystem;

// ---- VDF helpers ----------------------------------------------------------

static std::string readVdfString(const std::string& s, size_t& pos) {
    if (pos >= s.size() || s[pos] != '"') return {};
    ++pos;
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            ++pos;
            switch (s[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                default:  result += s[pos]; break;
            }
        } else {
            result += s[pos];
        }
        ++pos;
    }
    if (pos < s.size()) ++pos;
    return result;
}

// Returns the next "key" "value" pair at the current VDF nesting level.
// Returns {"",""} on block end or EOF.
static std::pair<std::string,std::string> nextKV(const std::string& s, size_t& pos) {
    while (pos < s.size()) {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
        if (pos >= s.size()) break;
        if (s[pos] == '}') { ++pos; return {}; }
        if (s[pos] == '{') { ++pos; continue; }
        if (s[pos] != '"') { ++pos; continue; }
        std::string key = readVdfString(s, pos);
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
        if (pos >= s.size()) break;
        if (s[pos] == '{') { ++pos; continue; }
        if (s[pos] != '"') continue;
        std::string val = readVdfString(s, pos);
        return {key, val};
    }
    return {};
}

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

// ---- Steam path -----------------------------------------------------------

std::string SteamResolver::getSteamPath() {
    const wchar_t* keys[] = {
        L"SOFTWARE\\WOW6432Node\\Valve\\Steam",
        L"SOFTWARE\\Valve\\Steam"
    };
    for (auto* key : keys) {
        HKEY hk;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hk) != ERROR_SUCCESS) continue;
        wchar_t buf[MAX_PATH];
        DWORD sz = sizeof(buf);
        bool ok = RegQueryValueExW(hk, L"InstallPath", nullptr, nullptr,
                                   reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS;
        RegCloseKey(hk);
        if (!ok) continue;
        int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
        std::string s(n - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, s.data(), n, nullptr, nullptr);
        return s;
    }
    const char* def = "C:\\Program Files (x86)\\Steam";
    return fs::exists(def) ? def : std::string{};
}

std::vector<std::string> SteamResolver::getLibraryFolders() {
    std::string steamPath = getSteamPath();
    if (steamPath.empty()) return {};
    std::vector<std::string> folders{steamPath};

    std::ifstream f(steamPath + "\\steamapps\\libraryfolders.vdf");
    if (!f) return folders;
    std::string content((std::istreambuf_iterator<char>(f)), {});

    size_t pos = 0;
    while (pos < content.size()) {
        auto [k, v] = nextKV(content, pos);
        if (k.empty()) break;
        std::string path;
        if (k == "path") {
            path = v;
        } else if (!v.empty() && std::all_of(k.begin(), k.end(), ::isdigit)) {
            path = v;
        }
        if (!path.empty()) {
            // Unescape double-backslashes from VDF
            std::string unescaped;
            for (size_t i = 0; i < path.size(); ++i)
                if (path[i] == '\\' && i + 1 < path.size() && path[i+1] == '\\')
                    { unescaped += '\\'; ++i; }
                else
                    unescaped += path[i];
            if (fs::exists(unescaped)) folders.push_back(unescaped);
        }
    }
    return folders;
}

// ---- Manifest parsing -----------------------------------------------------

static SteamResolver::GameInfo parseManifest(const std::string& acfPath,
                                              const std::string& commonDir) {
    SteamResolver::GameInfo info;
    std::ifstream f(acfPath);
    if (!f) return info;
    std::string content((std::istreambuf_iterator<char>(f)), {});

    size_t pos = 0;
    std::string installDirName;
    while (pos < content.size()) {
        auto [k, v] = nextKV(content, pos);
        if (k.empty()) break;
        if (k == "name")       info.name = v;
        if (k == "appid")      info.appId = v;
        if (k == "installdir") installDirName = v;
    }
    if (!installDirName.empty())
        info.installDir = commonDir + "\\" + installDirName;
    return info;
}

std::map<std::string, SteamResolver::GameInfo>
SteamResolver::parseManifestsFromDir(const std::string& appsDir) {
    std::map<std::string, GameInfo> games;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(appsDir, ec)) {
        const std::string fname = entry.path().filename().string();
        if (fname.rfind("appmanifest_", 0) != 0 || entry.path().extension() != ".acf")
            continue;
        auto info = parseManifest(entry.path().string(), appsDir + "\\common");
        if (!info.name.empty())
            games[toLower(info.name)] = info;
    }
    return games;
}

std::map<std::string, SteamResolver::GameInfo> SteamResolver::getInstalledGames() {
    std::map<std::string, GameInfo> games;
    for (const auto& lib : getLibraryFolders()) {
        auto more = parseManifestsFromDir(lib + "\\steamapps");
        games.insert(more.begin(), more.end());
    }
    return games;
}

std::vector<std::string> SteamResolver::findExes(const std::string& installDir) {
    static constexpr const char* skipWords[] = {
        "unins","setup","redist","crash","report","vcredist","dxsetup"
    };
    std::vector<std::string> exes;
    std::error_code ec;
    for (fs::recursive_directory_iterator rdi(installDir, ec);
         !ec && rdi != fs::recursive_directory_iterator(); ++rdi) {
        if (rdi.depth() > 4) { rdi.disable_recursion_pending(); continue; }
        const auto& entry = *rdi;
        if (entry.path().extension() != ".exe") continue;
        std::string nameLower = toLower(entry.path().filename().string());
        bool skip = false;
        for (const char* w : skipWords)
            if (nameLower.find(w) != std::string::npos) { skip = true; break; }
        if (!skip) exes.push_back(entry.path().string());
    }
    return exes;
}

std::vector<std::string> SteamResolver::resolve(const std::string& gameName) {
    auto games = getInstalledGames();
    std::string lower = toLower(gameName);

    auto it = games.find(lower);
    if (it != games.end()) {
        Logger::log("STEAM RESOLVED \"" + gameName + "\" -> " + it->second.installDir);
        return findExes(it->second.installDir);
    }
    for (const auto& [key, info] : games) {
        if (key.find(lower) != std::string::npos || lower.find(key) != std::string::npos) {
            Logger::log("STEAM RESOLVED (partial) \"" + gameName + "\" -> " + info.installDir);
            return findExes(info.installDir);
        }
    }
    Logger::log("STEAM NOT FOUND: \"" + gameName + "\"");
    return {};
}
