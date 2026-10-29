#include "core/net/http_cache.hpp"

#include "core/cache/hash.hpp"
#include "core/net/cache_control.hpp"

namespace nuvio::core::net
{

HttpCache::HttpCache(const std::string& dir, uint64_t maxBytes, cache::DiskCache::Clock clock)
    : disk(dir, maxBytes, clock), now(clock)
{
}

std::string HttpCache::key(const std::string& method, const std::string& url)
{
    return cache::hashKey(method + " " + url);
}

std::optional<HttpCache::Lookup> HttpCache::lookup(const std::string& method, const std::string& url)
{
    auto hit = disk.get(key(method, url));
    if (!hit)
        return std::nullopt;

    const auto& meta = hit->meta;
    if (meta.value("url", std::string()) != url)
        return std::nullopt; // hash collision

    HttpResult result;
    result.ok        = true;
    result.status    = meta.value("status", 200L);
    result.finalUrl  = meta.value("finalUrl", url);
    result.body      = std::move(hit->body);
    result.fromCache = true;
    if (auto headers = meta.find("headers"); headers != meta.end() && headers->is_object())
        for (auto& [name, value] : headers->items())
            if (value.is_string())
                result.headers[name] = value.get<std::string>();

    int64_t age = now() - meta.value("storedAt", int64_t(0));
    bool fresh  = age >= 0 && age < meta.value("maxAge", int64_t(0));
    result.stale = !fresh;
    return Lookup{std::move(result), fresh};
}

bool HttpCache::store(const std::string& method, const std::string& url, const HttpResult& result)
{
    auto it           = result.headers.find("Cache-Control");
    CacheControl cc   = parseCacheControl(it == result.headers.end() ? "" : it->second);
    if (!result.ok || !isCacheable(method, result.status, cc))
        return false;

    // No max-age still gets stored, already stale, so StaleIfError can use it offline
    nlohmann::json headers = nlohmann::json::object();
    for (const auto& [name, value] : result.headers)
        headers[name] = value;
    nlohmann::json meta = {
        {"url", url},
        {"finalUrl", result.finalUrl},
        {"status", result.status},
        {"storedAt", now()},
        {"maxAge", cc.freshnessSeconds()},
        {"headers", headers},
    };
    return disk.put(key(method, url), result.body, std::move(meta));
}

} // namespace nuvio::core::net
