#include <gtest/gtest.h>
#include "../duration_parser.h"

// ---- ParseDuration --------------------------------------------------------

TEST(ParseDuration, Days) {
    EXPECT_EQ(ParseDuration("5d"), 5 * 86400);
}
TEST(ParseDuration, Hours) {
    EXPECT_EQ(ParseDuration("48h"), 48 * 3600);
}
TEST(ParseDuration, Minutes) {
    EXPECT_EQ(ParseDuration("90m"), 90 * 60);
}
TEST(ParseDuration, CompoundDayHour) {
    EXPECT_EQ(ParseDuration("1d12h"), 86400 + 12 * 3600);
}
TEST(ParseDuration, CompoundDayHourMinute) {
    EXPECT_EQ(ParseDuration("1d12h30m"), 86400 + 12 * 3600 + 30 * 60);
}
TEST(ParseDuration, SingleMinute) {
    EXPECT_EQ(ParseDuration("1m"), 60);
}
TEST(ParseDuration, ThrowsOnEmpty) {
    EXPECT_THROW(ParseDuration(""), std::runtime_error);
}
TEST(ParseDuration, ThrowsOnUnknownUnit) {
    EXPECT_THROW(ParseDuration("5s"), std::runtime_error);
}
TEST(ParseDuration, ThrowsOnNoUnit) {
    EXPECT_THROW(ParseDuration("42"), std::runtime_error);
}
TEST(ParseDuration, ThrowsOnLeadingGarbage) {
    EXPECT_THROW(ParseDuration("xd"), std::runtime_error);
}

// ---- FormatRemaining ------------------------------------------------------

TEST(FormatRemaining, ZeroReturnsZeroMinutes) {
    EXPECT_EQ(FormatRemaining(0), "0m");
}
TEST(FormatRemaining, JustMinutes) {
    EXPECT_EQ(FormatRemaining(90 * 60), "1h 30m");
}
TEST(FormatRemaining, JustDays) {
    EXPECT_EQ(FormatRemaining(3 * 86400), "3d");
}
TEST(FormatRemaining, DaysAndHours) {
    EXPECT_EQ(FormatRemaining(86400 + 2 * 3600), "1d 2h");
}
TEST(FormatRemaining, FullCompound) {
    EXPECT_EQ(FormatRemaining(86400 + 12 * 3600 + 30 * 60), "1d 12h 30m");
}
TEST(FormatRemaining, NegativeReturnsZeroMinutes) {
    EXPECT_EQ(FormatRemaining(-100), "0m");
}
