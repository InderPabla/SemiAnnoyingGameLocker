#include "friction_engine.h"
#include "crypto_utils.h"
#include "duration_parser.h"
#include "logger.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <conio.h>

// ---- String helpers -------------------------------------------------------

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

// ---- Password -------------------------------------------------------------

std::string FrictionEngine::readPassword(const std::string& prompt) {
    std::cout << prompt;
    std::string pw;
    int ch;
    while ((ch = _getch()) != '\r' && ch != '\n') {
        if (ch == '\b' || ch == 127) {
            if (!pw.empty()) { pw.pop_back(); std::cout << "\b \b"; }
        } else if (ch >= 32 && ch < 127) {
            pw += static_cast<char>(ch);
            std::cout << '*';
        }
    }
    std::cout << '\n';
    return pw;
}

std::string FrictionEngine::setupPassword() {
    while (true) {
        std::string pw  = readPassword("Enter password: ");
        std::string pw2 = readPassword("Confirm password: ");
        if (pw.empty())  { std::cout << "Password cannot be empty.\n";      continue; }
        if (pw != pw2)   { std::cout << "Passwords do not match. Retry.\n"; continue; }
        return CryptoUtils::sha256hex(pw);
    }
}

bool FrictionEngine::verifyPassword(const std::string& storedHash) {
    return CryptoUtils::sha256hex(readPassword("Enter password to unlock: ")) == storedHash;
}

// ---- Trivia ---------------------------------------------------------------

std::vector<FrictionEngine::Question> FrictionEngine::loadTrivia(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open trivia file: " + path);

    std::vector<Question> questions;
    std::string line;

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        size_t dot = line.find(". ");
        if (dot == std::string::npos || dot == 0) continue;

        bool allDigits = true;
        for (size_t i = 0; i < dot; ++i)
            if (!std::isdigit(static_cast<unsigned char>(line[i]))) { allDigits = false; break; }
        if (!allDigits) continue;

        Question q;
        q.question = line.substr(dot + 2);
        while (std::getline(f, line)) {
            line = trim(line);
            if (!line.empty()) { q.answer = toLower(line); break; }
        }
        if (!q.answer.empty()) questions.push_back(std::move(q));
    }
    return questions;
}

bool FrictionEngine::runTrivia(const std::string& path) {
    std::vector<Question> questions;
    try {
        questions = loadTrivia(path);
    } catch (const std::exception& ex) {
        Logger::error(ex.what());
        std::cout << "[Trivia unavailable — skipping]\n";
        return true;
    }
    if (questions.empty()) {
        std::cout << "[No questions found — skipping]\n";
        return true;
    }

    size_t limit = std::min(questions.size(), size_t{10});
    std::cout << "\n--- TRIVIA CHALLENGE (" << limit << " questions, all must be correct) ---\n\n";

    for (size_t i = 0; i < limit; ++i) {
        std::cout << "Q" << (i + 1) << ": " << questions[i].question << "\nAnswer: ";
        std::string input;
        std::getline(std::cin, input);
        if (toLower(trim(input)) == questions[i].answer) {
            std::cout << "Correct!\n\n";
        } else {
            std::cout << "\nWrong. Restarting from Q1.\n\n";
            Logger::log("TRIVIA FAILED at Q" + std::to_string(i + 1));
            return false;
        }
    }
    std::cout << "--- All correct! ---\n\n";
    Logger::log("TRIVIA PASSED");
    return true;
}

// ---- Annoyance loop -------------------------------------------------------

void FrictionEngine::runAnnoyanceLoop(int64_t remainingSeconds) {
    static constexpr struct { const char* prompt; const char* expected; } steps[] = {
        {"You still have %s left. Type CONTINUE to proceed: ",           "CONTINUE" },
        {"Are you sure you want to unlock early? Type YES: ",            "YES"      },
        {"This is intentional friction. Type PROCEED: ",                 "PROCEED"  },
        {"You set this lock for a reason. Type IGNORE to override: ",    "IGNORE"   },
        {"Last chance to back out. Type OVERRIDE to continue: ",         "OVERRIDE" },
        {"Really? Type UNLOCK to show commitment: ",                     "UNLOCK"   },
        {"Think about why you set this. Type STILLHERE: ",               "STILLHERE"},
        {"No judgment — the friction is working. Type FORWARD: ",        "FORWARD"  },
        {"Almost done. Type COMMITTED: ",                                "COMMITTED"},
        {"Final check. Type CONFIRM to continue: ",                      "CONFIRM"  },
    };

    std::string remaining = FormatRemaining(remainingSeconds);
    std::cout << "\n=== EARLY UNLOCK FRICTION === (remaining: " << remaining << ")\n\n";

    for (const auto& step : steps) {
        std::string prompt = step.prompt;
        auto pos = prompt.find("%s");
        if (pos != std::string::npos) prompt.replace(pos, 2, remaining);

        while (true) {
            std::cout << prompt;
            std::string input;
            std::getline(std::cin, input);
            if (trim(input) == step.expected) break;
            std::cout << "  Type exactly: " << step.expected << "\n";
        }
        std::cout << '\n';
    }
    Logger::log("ANNOYANCE LOOP COMPLETED");
}

// ---- Full unlock friction flow --------------------------------------------

bool FrictionEngine::runUnlockFriction(bool passwordEnabled,
                                        const std::string& passwordHash,
                                        bool triviaEnabled,
                                        const std::string& triviaFile,
                                        int64_t remainingSeconds) {
    if (passwordEnabled) {
        std::cout << "\nThis lock is password-protected.\n";
        if (verifyPassword(passwordHash)) {
            std::cout << "Password correct. Proceeding...\n";
            Logger::log("UNLOCK: password accepted");
            return true;
        }
        std::cout << "Incorrect password — falling back to friction mode.\n\n";
        Logger::log("UNLOCK: wrong password, fallback to friction");
    }

    runAnnoyanceLoop(remainingSeconds);

    if (triviaEnabled && !triviaFile.empty()) {
        bool passed = false;
        while (!passed) passed = runTrivia(triviaFile);
    }

    std::cout << "=== FINAL CONFIRMATION ===\nType UNLOCK to remove the lock: ";
    while (true) {
        std::string input;
        std::getline(std::cin, input);
        if (trim(input) == "UNLOCK") break;
        std::cout << "  Type exactly: UNLOCK\nType UNLOCK to remove the lock: ";
    }
    std::cout << "\nProceeding with unlock...\n";
    Logger::log("UNLOCK: friction completed");
    return true;
}
