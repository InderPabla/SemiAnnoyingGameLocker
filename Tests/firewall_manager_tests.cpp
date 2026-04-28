#include <gtest/gtest.h>
#include "fake_shell.h"
#include "../firewall_manager.h"

// ---- ruleName -------------------------------------------------------------

TEST(FirewallManager_RuleName, CorrectFormat) {
    std::string name = FirewallManager::ruleName("20260427-Squad", "C:\\Games\\Squad.exe");
    EXPECT_EQ(name.substr(0, 6), "SAGL::");
    EXPECT_NE(name.find("20260427-Squad"), std::string::npos);
    // format: SAGL::<lockId>::<6-char-hash>
    auto parts = std::vector<std::string>{};
    std::string s = name;
    size_t pos;
    while ((pos = s.find("::")) != std::string::npos) { parts.push_back(s.substr(0, pos)); s = s.substr(pos + 2); }
    parts.push_back(s);
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "SAGL");
    EXPECT_EQ(parts[1], "20260427-Squad");
    EXPECT_EQ(parts[2].size(), 6u);  // short hash
}

TEST(FirewallManager_RuleName, CaseInsensitiveHash) {
    // Same exe, different case → same hash (paths are lowercased before hashing)
    std::string a = FirewallManager::ruleName("lock1", "C:\\Games\\Squad.exe");
    std::string b = FirewallManager::ruleName("lock1", "C:\\GAMES\\SQUAD.EXE");
    EXPECT_EQ(a, b);
}

TEST(FirewallManager_RuleName, DifferentExesDifferentHashes) {
    std::string a = FirewallManager::ruleName("lock1", "C:\\Games\\Squad.exe");
    std::string b = FirewallManager::ruleName("lock1", "C:\\Games\\CS2.exe");
    EXPECT_NE(a, b);
}

// ---- addRule --------------------------------------------------------------

TEST(FirewallManager_AddRule, SendsCorrectNetshCommand) {
    FakeShell shell;
    FirewallManager fw(shell);

    std::string result = fw.addRule("20260427-Squad", "C:\\Games\\Squad.exe");

    ASSERT_EQ(shell.executed.size(), 1u);
    EXPECT_FALSE(result.empty());

    const std::string& cmd = shell.executed[0];
    EXPECT_NE(cmd.find("netsh advfirewall firewall add rule"), std::string::npos);
    EXPECT_NE(cmd.find("dir=out"),                             std::string::npos);
    EXPECT_NE(cmd.find("action=block"),                        std::string::npos);
    EXPECT_NE(cmd.find("C:\\Games\\Squad.exe"),                std::string::npos);
    EXPECT_NE(cmd.find("SAGL::20260427-Squad::"),              std::string::npos);
}

TEST(FirewallManager_AddRule, ReturnsEmptyOnFailure) {
    FakeShell shell;
    shell.executeReturnCode = 1;
    FirewallManager fw(shell);

    EXPECT_TRUE(fw.addRule("lock1", "C:\\game.exe").empty());
}

// ---- deleteRule -----------------------------------------------------------

TEST(FirewallManager_DeleteRule, SendsCorrectNetshCommand) {
    FakeShell shell;
    FirewallManager fw(shell);

    fw.deleteRule("SAGL::20260427-Squad::abc123");

    ASSERT_EQ(shell.executed.size(), 1u);
    const std::string& cmd = shell.executed[0];
    EXPECT_NE(cmd.find("netsh advfirewall firewall delete rule"), std::string::npos);
    EXPECT_NE(cmd.find("SAGL::20260427-Squad::abc123"),           std::string::npos);
}

// ---- ruleExists -----------------------------------------------------------

TEST(FirewallManager_RuleExists, ReturnsTrueWhenOutputHasRuleInfo) {
    FakeShell shell;
    shell.captureResponses["show rule"] =
        "Rule Name:                            SAGL::20260427-Squad::abc123\r\n"
        "Enabled:                              Yes\r\n";
    FirewallManager fw(shell);

    EXPECT_TRUE(fw.ruleExists("SAGL::20260427-Squad::abc123"));
}

TEST(FirewallManager_RuleExists, ReturnsFalseWhenNotFound) {
    FakeShell shell;
    shell.captureResponses["show rule"] = "No rules match the specified criteria.\r\n";
    FirewallManager fw(shell);

    EXPECT_FALSE(fw.ruleExists("SAGL::20260427-Squad::abc123"));
}

// ---- listSaglRules --------------------------------------------------------

TEST(FirewallManager_ListRules, ParsesRuleNamesFromNetshOutput) {
    FakeShell shell;
    shell.captureResponses["show rule name=all"] =
        "Rule Name:                            SAGL::20260427-Squad::abc123\r\n"
        "------\r\n"
        "Rule Name:                            Allow Chrome\r\n"
        "------\r\n"
        "Rule Name:                            SAGL::20260427-CS2::def456\r\n";
    FirewallManager fw(shell);

    auto rules = fw.listSaglRules();

    ASSERT_EQ(rules.size(), 2u);
    EXPECT_EQ(rules[0], "SAGL::20260427-Squad::abc123");
    EXPECT_EQ(rules[1], "SAGL::20260427-CS2::def456");
}

TEST(FirewallManager_ListRules, ReturnsEmptyWhenNoSaglRules) {
    FakeShell shell;
    shell.captureResponses["show rule name=all"] = "Rule Name:   Allow Chrome\r\n";
    FirewallManager fw(shell);

    EXPECT_TRUE(fw.listSaglRules().empty());
}
