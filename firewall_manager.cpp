#include "firewall_manager.h"
#include "crypto_utils.h"
#include "logger.h"
#include <sstream>
#include <algorithm>
#include <cctype>

FirewallManager::FirewallManager(IShell& shell) : shell_(shell) {}

std::string FirewallManager::ruleName(const std::string& lockId, const std::string& exePath) {
    std::string lower = exePath;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return "SAGL::" + lockId + "::" + CryptoUtils::sha256short(lower, 6);
}

std::string FirewallManager::addRule(const std::string& lockId, const std::string& exePath) {
    std::string name = ruleName(lockId, exePath);
    std::ostringstream cmd;
    cmd << "netsh advfirewall firewall add rule"
        << " name=\""    << name    << "\""
        << " dir=out"
        << " program=\"" << exePath << "\""
        << " action=block enable=yes profile=any >nul 2>&1";

    if (shell_.execute(cmd.str()) != 0) {
        Logger::error("Failed to add firewall rule: " + name);
        return {};
    }
    Logger::log("FIREWALL RULE CREATED " + name);
    return name;
}

bool FirewallManager::deleteRule(const std::string& name) {
    std::string cmd = "netsh advfirewall firewall delete rule name=\"" + name + "\" >nul 2>&1";
    if (shell_.execute(cmd) != 0) {
        Logger::error("Failed to delete firewall rule: " + name);
        return false;
    }
    Logger::log("FIREWALL RULE DELETED " + name);
    return true;
}

bool FirewallManager::ruleExists(const std::string& name) {
    std::string cmd    = "netsh advfirewall firewall show rule name=\"" + name + "\" 2>&1";
    std::string output = shell_.capture(cmd);
    return output.find("No rules match") == std::string::npos && !output.empty();
}

std::vector<std::string> FirewallManager::listSaglRules() {
    std::vector<std::string> rules;
    std::string output = shell_.capture("netsh advfirewall firewall show rule name=all 2>&1");

    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        const std::string prefix = "Rule Name:";
        if (line.rfind(prefix, 0) != 0) continue;
        size_t start = prefix.size();
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start])))
            ++start;
        std::string name = line.substr(start);
        while (!name.empty() && (name.back() == '\r' || name.back() == ' '))
            name.pop_back();
        if (name.rfind("SAGL::", 0) == 0) rules.push_back(name);
    }
    return rules;
}

void FirewallManager::deleteRulesForLock(const std::vector<std::string>& ruleNames) {
    for (const auto& r : ruleNames) deleteRule(r);
}
