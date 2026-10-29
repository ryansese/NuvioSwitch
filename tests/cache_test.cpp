#include <doctest.h>

#include <memory>

#include "core/cache/disk_cache.hpp"
#include "core/cache/hash.hpp"
#include "core/cache/lru_index.hpp"
#include "core/image/image_cache.hpp"
#include "core/net/cache_control.hpp"
#include "core/net/http_cache.hpp"
#include "test_util.hpp"

using namespace nuvio::core;

TEST_CASE("Cache-Control parsing")
{
    auto cc = net::parseCacheControl("public, max-age=600");
    CHECK(cc.maxAge == std::optional<long>(600));
    CHECK(cc.freshnessSeconds() == 600);
    CHECK_FALSE(cc.noStore);

    cc = net::parseCacheControl("max-age=60, S-MAXAGE=\"300\"");
    CHECK(cc.freshnessSeconds() == 300);

    cc = net::parseCacheControl("no-store");
    CHECK(cc.noStore);
    CHECK(net::parseCacheControl(" No-Cache ").noCache);
    CHECK(net::parseCacheControl("private, max-age=10").isPrivate);

    cc = net::parseCacheControl("max-age=abc");
    CHECK_FALSE(cc.maxAge.has_value());
    CHECK(cc.freshnessSeconds() == 0);
    CHECK(net::parseCacheControl("").freshnessSeconds() == 0);
}

TEST_CASE("only 2xx GET responses without no-store/no-cache/private are cacheable")
{
    net::CacheControl plain;
    CHECK(net::isCacheable("GET", 200, plain));
    CHECK(net::isCacheable("GET", 204, plain));
    CHECK_FALSE(net::isCacheable("POST", 200, plain));
    CHECK_FALSE(net::isCacheable("GET", 404, plain));
    CHECK_FALSE(net::isCacheable("GET", 500, plain));
    CHECK_FALSE(net::isCacheable("GET", 200, net::parseCacheControl("no-store")));
    CHECK_FALSE(net::isCacheable("GET", 200, net::parseCacheControl("no-cache")));
    CHECK_FALSE(net::isCacheable("GET", 200, net::parseCacheControl("private")));
}

TEST_CASE("cache keys are stable 16-digit hex and differ by method")
{
    CHECK(cache::hashKey("") == "cbf29ce484222325");
    CHECK(cache::hashKey("GET https://a/").size() == 16);
    CHECK(net::HttpCache::key("GET", "https://a/") == net::HttpCache::key("GET", "https://a/"));
    CHECK(net::HttpCache::key("GET", "https://a/") != net::HttpCache::key("POST", "https://a/"));
}

TEST_CASE("LruIndex evicts least recently used entries over the cap")
{
    cache::LruIndex lru(100);
    CHECK(lru.put("a", 40).empty());
    CHECK(lru.put("b", 40).empty());
    lru.touch("a"); // b is now oldest
    auto evicted = lru.put("c", 40);
    CHECK(evicted == std::vector<std::string>{"b"});
    CHECK(lru.totalBytes() == 80);
    CHECK(lru.keysOldestFirst() == std::vector<std::string>{"a", "c"});

    // replacing a key updates its size instead of counting it twice
    CHECK(lru.put("a", 10).empty());
    CHECK(lru.totalBytes() == 50);

    // an entry larger than the cap evicts everything, itself included
    evicted = lru.put("huge", 200);
    CHECK(lru.count() == 0);
    CHECK(lru.totalBytes() == 0);
}

TEST_CASE("MemoryLru keeps bytes until the cap and drops the oldest")
{
    image::MemoryLru memory(10);
    auto bytes = [](const char* s) { return std::make_shared<const std::string>(s); };
    memory.put("a", bytes("1234"));
    memory.put("b", bytes("1234"));
    CHECK(memory.get("a")); // a is now the most recent
    memory.put("c", bytes("1234"));
    CHECK(memory.get("a"));
    CHECK_FALSE(memory.get("b"));
    CHECK(memory.get("c"));
    CHECK(memory.totalBytes() == 8);

    memory.put("too-big", bytes("12345678901"));
    CHECK_FALSE(memory.get("too-big"));
}

TEST_CASE("DiskCache evicts by size and keeps entries across instances")
{
    TempDir tmp;
    int64_t clock = 1000;
    auto now      = [&clock] { return clock; };
    {
        cache::DiskCache disk(tmp.path(), 10, now);
        CHECK(disk.put("a", "12345", {{"n", 1}}));
        clock++;
        CHECK(disk.put("b", "12345", {{"n", 2}}));
        clock++;
        CHECK(disk.get("a")); // a is now the most recent
        clock++;
        CHECK(disk.put("c", "123", {{"n", 3}}));
        CHECK_FALSE(disk.get("b"));
        CHECK_FALSE(disk.put("huge", "12345678901", {}));
    }

    cache::DiskCache reopened(tmp.path(), 10, now);
    CHECK(reopened.count() == 2);
    auto hit = reopened.get("a");
    REQUIRE(hit);
    CHECK(hit->body == "12345");
    CHECK(hit->meta["n"] == 1);
    CHECK(reopened.get("c"));
    CHECK_FALSE(std::filesystem::exists(tmp.path() + "b.body"));
}

TEST_CASE("DiskCache ignores bodies the index doesn't list")
{
    TempDir tmp;
    {
        FILE* f = std::fopen((tmp.path() + "orphan.body").c_str(), "wb");
        std::fputs("junk", f);
        std::fclose(f);
    }
    cache::DiskCache disk(tmp.path(), 100);
    CHECK(disk.count() == 0);
    CHECK_FALSE(std::filesystem::exists(tmp.path() + "orphan.body"));
}

TEST_CASE("HttpCache freshness follows max-age")
{
    TempDir tmp;
    int64_t clock = 0;
    net::HttpCache httpCache(tmp.path(), net::HttpCache::DEFAULT_MAX_BYTES, [&clock] { return clock; });

    net::HttpResult response;
    response.ok                       = true;
    response.status                   = 200;
    response.body                     = "{}";
    response.finalUrl                 = "https://addon/manifest.json";
    response.headers["cache-control"] = "max-age=600";
    REQUIRE(httpCache.store("GET", "https://addon/manifest.json", response));

    clock      = 300; // 5 minutes later
    auto fresh = httpCache.lookup("GET", "https://addon/manifest.json");
    REQUIRE(fresh);
    CHECK(fresh->fresh);
    CHECK(fresh->result.body == "{}");
    CHECK(fresh->result.fromCache);
    CHECK(fresh->result.headers["Cache-Control"] == "max-age=600");

    clock        = 660; // 11 minutes later
    auto expired = httpCache.lookup("GET", "https://addon/manifest.json");
    REQUIRE(expired);
    CHECK_FALSE(expired->fresh);
    CHECK(expired->result.stale);
}

TEST_CASE("HttpCache doesn't store errors or no-store responses")
{
    TempDir tmp;
    net::HttpCache httpCache(tmp.path());

    net::HttpResult error;
    error.status = 500;
    error.error  = net::HttpError::Http;
    CHECK_FALSE(httpCache.store("GET", "https://addon/a", error));

    net::HttpResult noStore;
    noStore.ok                       = true;
    noStore.status                   = 200;
    noStore.headers["Cache-Control"] = "no-store";
    CHECK_FALSE(httpCache.store("GET", "https://addon/b", noStore));

    CHECK_FALSE(httpCache.lookup("GET", "https://addon/a"));
    CHECK_FALSE(httpCache.lookup("GET", "https://addon/b"));
}

TEST_CASE("looksLikeImage recognises the formats borealis decodes")
{
    CHECK(image::looksLikeImage(std::string("\xFF\xD8\xFF\xE0", 4)));
    CHECK(image::looksLikeImage(std::string("\x89PNG\r\n\x1A\n....", 12)));
    CHECK(image::looksLikeImage("GIF89a..."));
    CHECK_FALSE(image::looksLikeImage("<html>Not Found</html>"));
    CHECK_FALSE(image::looksLikeImage(""));
}
