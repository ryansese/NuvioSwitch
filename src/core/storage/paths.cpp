#include "core/storage/paths.hpp"

#include <cstdlib>

#include "core/storage/files.hpp"

namespace nuvio::core::storage
{

namespace
{
std::string withSlash(std::string dir)
{
    if (!dir.empty() && dir.back() != '/')
        dir += '/';
    return dir;
}

std::string platformDataDir()
{
#if defined(__SWITCH__)
    return "sdmc:/switch/NuvioSwitch/";
#else
    const char* home = std::getenv("HOME");
    std::string base = home ? home : ".";
#if defined(__APPLE__)
    return base + "/Library/Application Support/NuvioSwitch/";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
        return withSlash(xdg) + "NuvioSwitch/";
    return base + "/.local/share/NuvioSwitch/";
#endif
#endif
}
} // namespace

std::string Paths::dataDir()
{
    if (const char* overrideDir = std::getenv("NUVIOSWITCH_DATA_DIR"); overrideDir && *overrideDir)
        return withSlash(overrideDir);
    return platformDataDir();
}

std::string Paths::cacheDir()
{
    return dataDir() + "cache/";
}

std::string Paths::httpCacheDir()
{
    return cacheDir() + "http/";
}

std::string Paths::imageCacheDir()
{
    return cacheDir() + "images/";
}

std::string Paths::storeFile(const std::string& name, Scope scope, int profileId)
{
    if (scope == Scope::Global)
        return dataDir() + name + ".json";
    return dataDir() + "profiles/" + std::to_string(profileId) + "/" + name + ".json";
}

bool Paths::ensureCreated()
{
    return ensureDir(dataDir()) && ensureDir(httpCacheDir()) && ensureDir(imageCacheDir());
}

} // namespace nuvio::core::storage
