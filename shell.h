#pragma once
#include <string>
#include <string_view>
#include <cstdio>

// Seam for OS command execution.  Inject a FakeShell in tests; use SystemShell in production.
class IShell {
public:
    virtual ~IShell() = default;
    virtual int         execute(std::string_view cmd) = 0;   // returns process exit code
    virtual std::string capture(std::string_view cmd) = 0;   // returns stdout
};

class SystemShell final : public IShell {
public:
    int execute(std::string_view cmd) override {
        return system(std::string(cmd).c_str());
    }
    std::string capture(std::string_view cmd) override {
        std::string result;
        FILE* pipe = _popen(std::string(cmd).c_str(), "r");
        if (!pipe) return {};
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) result += buf;
        _pclose(pipe);
        return result;
    }
};
