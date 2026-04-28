#pragma once
#include <string>
#include <stdexcept>
#include <cstdint>

// Parses compound duration strings like "5d", "48h", "90m", "1d12h", "1d12h30m".
// Returns total seconds.
inline int64_t ParseDuration(const std::string& s) {
    if (s.empty()) throw std::runtime_error("Empty duration string");

    int64_t total = 0;
    size_t i = 0;
    bool parsed = false;

    while (i < s.size()) {
        if (!isdigit((unsigned char)s[i]))
            throw std::runtime_error("Expected digit in duration: " + s);

        int64_t num = 0;
        while (i < s.size() && isdigit((unsigned char)s[i])) {
            num = num * 10 + (s[i] - '0');
            i++;
        }

        if (i >= s.size())
            throw std::runtime_error("Expected unit (d/h/m) after number in: " + s);

        char unit = (char)tolower((unsigned char)s[i]);
        i++;
        parsed = true;

        switch (unit) {
            case 'd': total += num * 86400; break;
            case 'h': total += num * 3600;  break;
            case 'm': total += num * 60;    break;
            default:
                throw std::runtime_error(std::string("Unknown duration unit '") + unit + "' in: " + s);
        }
    }

    if (!parsed) throw std::runtime_error("No duration components found in: " + s);
    return total;
}

// Formats a number of seconds back to a human-readable remaining string like "4d 2h 15m".
inline std::string FormatRemaining(int64_t seconds) {
    if (seconds <= 0) return "0m";

    int64_t d = seconds / 86400;
    int64_t h = (seconds % 86400) / 3600;
    int64_t m = (seconds % 3600) / 60;

    std::string result;
    if (d > 0) result += std::to_string(d) + "d ";
    if (h > 0) result += std::to_string(h) + "h ";
    if (m > 0 || result.empty()) result += std::to_string(m) + "m";

    while (!result.empty() && result.back() == ' ')
        result.pop_back();
    return result;
}
