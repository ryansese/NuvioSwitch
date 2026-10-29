#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace nuvio::core::cache
{

// FNV-1a 64: cache file names only, no security need.
inline uint64_t fnv1a64(const std::string& data)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned char c : data)
    {
        hash ^= c;
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

// fnv1a64 as 16 lowercase hex digits.
inline std::string hashKey(const std::string& data)
{
    char buffer[17];
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(fnv1a64(data)));
    return buffer;
}

} // namespace nuvio::core::cache
