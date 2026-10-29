#include "core/storage/files.hpp"

#include <cstdio>
#include <filesystem>
#include <system_error>

#if !defined(__SWITCH__)
#include <unistd.h>
#endif

namespace nuvio::core::storage
{

namespace fs = std::filesystem;

bool ensureDir(const std::string& path)
{
    std::error_code ec;
    fs::create_directories(path, ec);
    return fs::is_directory(path, ec);
}

std::optional<std::string> readFile(const std::string& path)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return std::nullopt;

    std::string data;
    char buffer[16384];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        data.append(buffer, n);
    bool failed = std::ferror(file) != 0;
    std::fclose(file);
    if (failed)
        return std::nullopt;
    return data;
}

bool fileExists(const std::string& path)
{
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool atomicWrite(const std::string& path, const std::string& data)
{
    std::string tmp = path + ".tmp";
    FILE* file      = std::fopen(tmp.c_str(), "wb");
    if (!file)
        return false;

    bool ok = std::fwrite(data.data(), 1, data.size(), file) == data.size();
    ok      = std::fflush(file) == 0 && ok;
#if !defined(__SWITCH__)
    ok = fsync(fileno(file)) == 0 && ok;
#endif
    ok = std::fclose(file) == 0 && ok;
    if (!ok)
    {
        std::remove(tmp.c_str());
        return false;
    }

    if (std::rename(tmp.c_str(), path.c_str()) == 0)
        return true;

    // FAT fallback: rename won't replace an existing file
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

} // namespace nuvio::core::storage
