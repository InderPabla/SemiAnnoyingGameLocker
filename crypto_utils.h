#pragma once
#include <string>

class CryptoUtils {
public:
    static std::string sha256hex(const std::string& input);
    static std::string sha256short(const std::string& s, size_t len = 6);
};
