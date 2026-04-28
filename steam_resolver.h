#pragma once
#include <string>
#include <vector>
#include <map>

class SteamResolver {
public:
    struct GameInfo {
        std::string name;
        std::string appId;
        std::string installDir;
    };

    static std::string                   getSteamPath();
    static std::vector<std::string>      getLibraryFolders();
    static std::map<std::string,GameInfo> getInstalledGames();
    static std::vector<std::string>      findExes(const std::string& installDir);
    static std::vector<std::string>      resolve(const std::string& gameName);

    // Exposed for unit tests: parse manifests from an arbitrary steamapps directory.
    static std::map<std::string, GameInfo> parseManifestsFromDir(const std::string& appsDir);
};
