#pragma once

#include <string>

namespace nuvio::core::storage
{

enum class Scope
{
    Global,  // <data>/<name>.json
    Profile, // <data>/profiles/<id>/<name>.json
};

// Where persistent data lives. Every directory returned ends with '/'.
class Paths
{
  public:
    // sdmc:/switch/NuvioSwitch/ on Switch, ~/Library/Application Support/NuvioSwitch/
    // on macOS. NUVIOSWITCH_DATA_DIR overrides it (tests, development).
    static std::string dataDir();

    static std::string cacheDir();      // <data>/cache/
    static std::string httpCacheDir();  // <data>/cache/http/
    static std::string imageCacheDir(); // <data>/cache/images/

    // The file of a named store.
    static std::string storeFile(const std::string& name, Scope scope, int profileId);

    // Creates the data folder and its fixed subfolders.
    static bool ensureCreated();
};

} // namespace nuvio::core::storage
