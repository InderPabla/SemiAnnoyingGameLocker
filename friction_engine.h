#pragma once
#include <string>
#include <vector>
#include <cstdint>

class FrictionEngine {
public:
    struct Question {
        std::string question;
        std::string answer;  // lowercase + trimmed
    };

    static std::string         readPassword(const std::string& prompt);
    static std::string         setupPassword();
    static bool                verifyPassword(const std::string& storedHash);

    static std::vector<Question> loadTrivia(const std::string& path);
    static bool                  runTrivia(const std::string& path);
    static void                  runAnnoyanceLoop(int64_t remainingSeconds);
    static bool                  runUnlockFriction(bool passwordEnabled,
                                                    const std::string& passwordHash,
                                                    bool triviaEnabled,
                                                    const std::string& triviaFile,
                                                    int64_t remainingSeconds);
};
