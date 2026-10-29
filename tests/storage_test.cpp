#include <doctest.h>

#include <cstdio>
#include <filesystem>

#include "core/storage/files.hpp"
#include "core/storage/paths.hpp"
#include "core/storage/profile_context.hpp"
#include "core/storage/store.hpp"
#include "test_util.hpp"

using namespace nuvio::core::storage;
namespace fs = std::filesystem;

namespace
{
void writeText(const std::string& path, const std::string& text)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    REQUIRE(f);
    std::fputs(text.c_str(), f);
    std::fclose(f);
}
} // namespace

TEST_CASE("NUVIOSWITCH_DATA_DIR overrides the data folder")
{
    TempDir tmp;
    ScopedEnv env("NUVIOSWITCH_DATA_DIR", tmp.path() + "data");
    CHECK(Paths::dataDir() == tmp.path() + "data/");
    CHECK(Paths::httpCacheDir() == tmp.path() + "data/cache/http/");
    CHECK(Paths::imageCacheDir() == tmp.path() + "data/cache/images/");
    CHECK(Paths::storeFile("settings", Scope::Global, 1) == tmp.path() + "data/settings.json");
    CHECK(Paths::storeFile("addons", Scope::Profile, 2) == tmp.path() + "data/profiles/2/addons.json");
}

TEST_CASE("the data folder is created on first use")
{
    TempDir tmp;
    ScopedEnv env("NUVIOSWITCH_DATA_DIR", tmp.path() + "fresh");
    CHECK(Paths::ensureCreated());
    CHECK(fs::is_directory(tmp.path() + "fresh/cache/http"));
    CHECK(fs::is_directory(tmp.path() + "fresh/cache/images"));
}

TEST_CASE("the active profile is 1 until profiles exist")
{
    CHECK(ProfileContext::current() == 1);
}

TEST_CASE("atomicWrite replaces the file and leaves no temp file")
{
    TempDir tmp;
    std::string path = tmp.path() + "file.json";
    REQUIRE(atomicWrite(path, "one"));
    REQUIRE(atomicWrite(path, "two"));
    CHECK(readFile(path) == std::optional<std::string>("two"));
    CHECK_FALSE(fileExists(path + ".tmp"));
}

TEST_CASE("a store round-trips through its file")
{
    TempDir tmp;
    ScopedEnv env("NUVIOSWITCH_DATA_DIR", tmp.path());
    std::string path = Paths::storeFile("addons", Scope::Profile, 1);
    {
        Store store(path);
        store.set("installed_addon_urls_ordered", {"https://v3-cinemeta.strem.io/manifest.json"});
        store.set("count", 3);
        store.save();
        Store::flush();
    }

    Store reopened(path); // as after a restart
    CHECK(reopened.get("installed_addon_urls_ordered") ==
          Store::Json::array({"https://v3-cinemeta.strem.io/manifest.json"}));
    CHECK(reopened.get<int>("count", 0) == 3);
    CHECK(reopened.get<std::string>("count", "fallback") == "fallback");
    CHECK(reopened.get("missing").is_null());
}

TEST_CASE("saves are written in order")
{
    TempDir tmp;
    Store store(tmp.path() + "ordered.json");
    for (int i = 0; i < 20; i++)
    {
        store.set("n", i);
        store.save();
    }
    Store::flush();
    CHECK(Store(tmp.path() + "ordered.json").get<int>("n", -1) == 19);
}

TEST_CASE("profiles keep separate values for the same key")
{
    TempDir tmp;
    ScopedEnv env("NUVIOSWITCH_DATA_DIR", tmp.path());
    {
        Store one(Paths::storeFile("addons", Scope::Profile, 1));
        Store two(Paths::storeFile("addons", Scope::Profile, 2));
        one.set("key", "profile one");
        two.set("key", "profile two");
        one.save();
        two.save();
        Store::flush();
    }
    CHECK(Store(Paths::storeFile("addons", Scope::Profile, 1)).get<std::string>("key", "") == "profile one");
    CHECK(Store(Paths::storeFile("addons", Scope::Profile, 2)).get<std::string>("key", "") == "profile two");
}

TEST_CASE("a corrupt store is moved aside and reads as empty")
{
    TempDir tmp;
    std::string path = tmp.path() + "addons.json";
    writeText(path, "{not json");

    Store store(path);
    CHECK(store.get("anything").is_null());
    CHECK_FALSE(fileExists(path));
    CHECK(readFile(path + ".corrupt") == std::optional<std::string>("{not json"));
}

TEST_CASE("a store left only as .tmp by an interrupted save is recovered")
{
    TempDir tmp;
    std::string path = tmp.path() + "addons.json";
    writeText(path + ".tmp", R"({"key": "saved"})");

    Store store(path);
    CHECK(store.get<std::string>("key", "") == "saved");
    CHECK(fileExists(path));
    CHECK_FALSE(fileExists(path + ".tmp"));
}

TEST_CASE("a half-written .tmp next to a good store is discarded")
{
    TempDir tmp;
    std::string path = tmp.path() + "addons.json";
    writeText(path, R"({"key": "old"})");
    writeText(path + ".tmp", R"({"key": "ne)");

    Store store(path);
    CHECK(store.get<std::string>("key", "") == "old");
    CHECK_FALSE(fileExists(path + ".tmp"));
}

TEST_CASE("Store::open shares one instance per file")
{
    TempDir tmp;
    ScopedEnv env("NUVIOSWITCH_DATA_DIR", tmp.path());
    auto a = Store::open("library");
    auto b = Store::open("library");
    auto g = Store::open("library", Scope::Global);
    CHECK(a == b);
    CHECK(a != g);
    CHECK(a->path() == tmp.path() + "profiles/1/library.json");
}
