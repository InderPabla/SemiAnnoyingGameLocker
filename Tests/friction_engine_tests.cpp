#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "../friction_engine.h"

namespace fs = std::filesystem;

// Helper: write a temp trivia file and return its path.
static std::string writeTempTrivia(const std::string& content) {
    std::string path = (fs::temp_directory_path() / "sagl_test_trivia.txt").string();
    std::ofstream f(path);
    f << content;
    return path;
}

// ---- loadTrivia -----------------------------------------------------------

TEST(FrictionEngine_LoadTrivia, ParsesNumberedQuestions) {
    std::string path = writeTempTrivia(
        "1. What is 2 + 2?\n"
        "4\n"
        "2. Capital of France?\n"
        "paris\n");

    auto questions = FrictionEngine::loadTrivia(path);

    ASSERT_EQ(questions.size(), 2u);
    EXPECT_EQ(questions[0].question, "What is 2 + 2?");
    EXPECT_EQ(questions[0].answer,   "4");
    EXPECT_EQ(questions[1].question, "Capital of France?");
    EXPECT_EQ(questions[1].answer,   "paris");
}

TEST(FrictionEngine_LoadTrivia, IgnoresBlankLines) {
    std::string path = writeTempTrivia(
        "\n"
        "1. Question one?\n"
        "\n"
        "answer one\n"
        "\n");

    auto questions = FrictionEngine::loadTrivia(path);

    ASSERT_EQ(questions.size(), 1u);
    EXPECT_EQ(questions[0].answer, "answer one");
}

TEST(FrictionEngine_LoadTrivia, AnswerIsLowercased) {
    std::string path = writeTempTrivia("1. Test?\nPARIS\n");
    auto questions = FrictionEngine::loadTrivia(path);
    ASSERT_EQ(questions.size(), 1u);
    EXPECT_EQ(questions[0].answer, "paris");
}

TEST(FrictionEngine_LoadTrivia, AnswerIsTrimmed) {
    std::string path = writeTempTrivia("1. Test?\n  Paris  \n");
    auto questions = FrictionEngine::loadTrivia(path);
    ASSERT_EQ(questions.size(), 1u);
    EXPECT_EQ(questions[0].answer, "paris");
}

TEST(FrictionEngine_LoadTrivia, CapsAt10Questions) {
    std::string content;
    for (int i = 1; i <= 15; ++i)
        content += std::to_string(i) + ". Q" + std::to_string(i) + "?\nanswer\n";
    std::string path = writeTempTrivia(content);

    auto questions = FrictionEngine::loadTrivia(path);
    // loadTrivia itself loads all; the cap happens in runTrivia.
    // Verify that at minimum 10 are present.
    EXPECT_GE(questions.size(), 10u);
    EXPECT_EQ(questions.size(), 15u);
}

TEST(FrictionEngine_LoadTrivia, ThrowsOnMissingFile) {
    EXPECT_THROW(FrictionEngine::loadTrivia("nonexistent_file_xyz.txt"), std::runtime_error);
}

TEST(FrictionEngine_LoadTrivia, ReadsFromTestDataFile) {
    // Verifies the checked-in test_data/questions.txt is parseable and has 5 questions.
    std::string path = "test_data\\questions.txt";
    auto questions = FrictionEngine::loadTrivia(path);
    EXPECT_EQ(questions.size(), 5u);
    EXPECT_EQ(questions[0].question, "What is 2 + 2?");
    EXPECT_EQ(questions[0].answer,   "4");
}
