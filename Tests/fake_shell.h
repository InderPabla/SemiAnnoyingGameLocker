#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include "../shell.h"

// Test double for IShell.  Records every command sent via execute() and returns
// canned responses for capture().  No real processes are spawned.
class FakeShell final : public IShell {
public:
    // Commands sent via execute(), in order.
    std::vector<std::string> executed;

    // Commands sent via capture(), in order.
    std::vector<std::string> captured;

    // Canned responses: if a capture() command contains a key, return the mapped value.
    std::map<std::string, std::string> captureResponses;

    // Return code for execute() — default 0 (success).
    int executeReturnCode = 0;

    int execute(std::string_view cmd) override {
        executed.emplace_back(cmd);
        return executeReturnCode;
    }

    std::string capture(std::string_view cmd) override {
        captured.emplace_back(cmd);
        for (const auto& [key, val] : captureResponses)
            if (std::string(cmd).find(key) != std::string::npos)
                return val;
        return {};
    }

    void reset() {
        executed.clear();
        captured.clear();
        captureResponses.clear();
        executeReturnCode = 0;
    }
};
