#include "data/meta/meta_cache.hpp"

#include <algorithm>

#include "core/net/cache_control.hpp"

namespace nuvio::data::meta
{

std::chrono::seconds metaTtl(const core::net::Headers& responseHeaders)
{
    auto header = responseHeaders.find("cache-control");
    if (header == responseHeaders.end())
        return DEFAULT_TTL;

    auto cc = core::net::parseCacheControl(header->second);
    if (cc.noCache || cc.noStore)
        return MIN_TTL;
    if (!cc.sMaxAge && !cc.maxAge)
        return DEFAULT_TTL;
    return std::max(MIN_TTL, std::chrono::seconds(cc.freshnessSeconds()));
}

std::optional<domain::Meta> MetaCache::get(const std::string& key, Clock::time_point now)
{
    auto it = entries.find(key);
    if (it == entries.end())
        return std::nullopt;
    if (now >= it->second.expires)
    {
        recent.erase(it->second.order);
        entries.erase(it);
        return std::nullopt;
    }
    recent.splice(recent.begin(), recent, it->second.order);
    return it->second.meta;
}

void MetaCache::put(const std::string& key, domain::Meta meta, std::chrono::seconds ttl, Clock::time_point now)
{
    auto it = entries.find(key);
    if (it != entries.end())
    {
        recent.erase(it->second.order);
        entries.erase(it);
    }
    recent.push_front(key);
    entries.emplace(key, Entry{std::move(meta), now + ttl, recent.begin()});

    while (entries.size() > capacity)
    {
        entries.erase(recent.back());
        recent.pop_back();
    }
}

} // namespace nuvio::data::meta
