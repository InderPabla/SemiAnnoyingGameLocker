#include <gtest/gtest.h>
#include "../config.h"
#include <set>
#include <sstream>
#include <vector>

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, delim)) out.push_back(part);
    return out;
}

// ---- formatDisplay --------------------------------------------------------

TEST(Config_FormatDisplay, MorningAM) {
    EXPECT_EQ(Config::formatDisplay("2026-05-02T09:05:00"), "May 2, 2026 9:05 AM");
}
TEST(Config_FormatDisplay, AfternoonPM) {
    EXPECT_EQ(Config::formatDisplay("2026-05-02T13:30:00"), "May 2, 2026 1:30 PM");
}
TEST(Config_FormatDisplay, Midnight) {
    EXPECT_EQ(Config::formatDisplay("2026-01-01T00:00:00"), "Jan 1, 2026 12:00 AM");
}
TEST(Config_FormatDisplay, Noon) {
    EXPECT_EQ(Config::formatDisplay("2026-06-15T12:00:00"), "Jun 15, 2026 12:00 PM");
}

// ---- formatSchtasks -------------------------------------------------------

TEST(Config_FormatSchtasks, CorrectDateAndTime) {
    auto [date, time] = Config::formatSchtasks("2026-05-02T13:30:00");
    EXPECT_EQ(time, "13:30");
    auto parts = split(date, '/');
    ASSERT_EQ(parts.size(), 3u);
    std::set<std::string> expected{"2026", "05", "02"};
    std::set<std::string> actual{parts[0], parts[1], parts[2]};
    EXPECT_EQ(actual, expected);
}
TEST(Config_FormatSchtasks, PadsMonthAndDay) {
    auto [date, time] = Config::formatSchtasks("2026-01-05T09:05:00");
    EXPECT_EQ(time, "09:05");
    auto parts = split(date, '/');
    ASSERT_EQ(parts.size(), 3u);
    std::set<std::string> expected{"2026", "01", "05"};
    std::set<std::string> actual{parts[0], parts[1], parts[2]};
    EXPECT_EQ(actual, expected);
}

// ---- secondsUntil ---------------------------------------------------------

TEST(Config_SecondsUntil, PastDateReturnsZero) {
    // A date firmly in the past should return 0.
    EXPECT_EQ(Config::secondsUntil("2000-01-01T00:00:00"), 0);
}

TEST(Config_SecondsUntil, FutureDateReturnsPositive) {
    // A date far in the future should return a large positive number.
    EXPECT_GT(Config::secondsUntil("2099-12-31T23:59:59"), 0);
}

// ---- generateLockId -------------------------------------------------------

TEST(Config_GenerateLockId, ContainsGameName) {
    // The ID should embed the (sanitized) first game name.
    std::string id = Config::generateLockId({"Squad"});
    EXPECT_NE(id.find("Squad"), std::string::npos);
}

TEST(Config_GenerateLockId, FallsBackToLock) {
    std::string id = Config::generateLockId({});
    EXPECT_NE(id.find("Lock"), std::string::npos);
}

TEST(Config_GenerateLockId, SanitizesSpecialChars) {
    std::string id = Config::generateLockId({"My Game!"});
    // '!' should be replaced with '_', ID should not contain '!'
    EXPECT_EQ(id.find('!'), std::string::npos);
}
