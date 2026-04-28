#pragma once
#include <string>
#include <vector>
#include "shell.h"

class FirewallManager {
public:
    explicit FirewallManager(IShell& shell);

    // Pure function — usable without an instance (handy in tests).
    static std::string ruleName(const std::string& lockId, const std::string& exePath);

    std::string              addRule(const std::string& lockId, const std::string& exePath);
    bool                     deleteRule(const std::string& name);
    bool                     ruleExists(const std::string& name);
    std::vector<std::string> listSaglRules();
    void                     deleteRulesForLock(const std::vector<std::string>& ruleNames);

private:
    IShell& shell_;
};
