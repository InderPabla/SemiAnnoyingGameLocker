#include "crypto_utils.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#pragma comment(lib, "bcrypt.lib")

std::string CryptoUtils::sha256hex(const std::string& input) {
    BCRYPT_ALG_HANDLE  hAlg  = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("BCryptOpenAlgorithmProvider failed");

    DWORD hashLen = 0, cbData = 0;
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
                      reinterpret_cast<PUCHAR>(&hashLen), sizeof(hashLen), &cbData, 0);
    BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hHash,
                   reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
                   static_cast<ULONG>(input.size()), 0);

    std::vector<BYTE> digest(hashLen);
    BCryptFinishHash(hHash, digest.data(), hashLen, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    std::ostringstream oss;
    for (BYTE b : digest)
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

std::string CryptoUtils::sha256short(const std::string& s, size_t len) {
    return sha256hex(s).substr(0, len);
}
