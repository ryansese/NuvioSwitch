#pragma once

#include <optional>
#include <string>

#include "core/cache/disk_cache.hpp"
#include "core/net/http_types.hpp"

namespace nuvio::core::net
{

// The on-disk HTTP response cache: 50 MB like NuvioTV's addon_http_cache,
// fresh for Cache-Control max-age / s-maxage.
class HttpCache
{
  public:
    static constexpr uint64_t DEFAULT_MAX_BYTES = 50ull * 1024 * 1024;

    HttpCache(const std::string& dir, uint64_t maxBytes = DEFAULT_MAX_BYTES,
        cache::DiskCache::Clock now = cache::unixNow);

    struct Lookup
    {
        HttpResult result; // ok, fromCache set; stale set when expired
        bool fresh;
    };

    // The cached response for method + url, fresh or not.
    std::optional<Lookup> lookup(const std::string& method, const std::string& url);

    // Stores result if its method, status and Cache-Control allow it.
    // Returns whether it was stored.
    bool store(const std::string& method, const std::string& url, const HttpResult& result);

    void flush() { disk.flush(); }

    static std::string key(const std::string& method, const std::string& url);

  private:
    cache::DiskCache disk;
    cache::DiskCache::Clock now;
};

} // namespace nuvio::core::net
