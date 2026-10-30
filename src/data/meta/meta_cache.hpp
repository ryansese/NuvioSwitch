#pragma once

#include <chrono>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

#include "core/net/http_types.hpp"
#include "domain/meta.hpp"

namespace nuvio::data::meta
{

using Clock = std::chrono::steady_clock;

constexpr std::chrono::seconds DEFAULT_TTL{6 * 60 * 60};
constexpr std::chrono::seconds MIN_TTL{5 * 60};

// How long a response may be reused (NuvioTV MetaRepositoryImpl.parseMaxAgeMs):
// no Cache-Control or no max-age gives 6 hours; no-cache and no-store give 5 minutes;
// a max-age (s-maxage first) is raised to at least 5 minutes.
std::chrono::seconds metaTtl(const core::net::Headers& responseHeaders);

// An in-memory LRU of capacity entries, each with an expiry. Not thread-safe:
// MetaRepository uses it on the UI thread only.
class MetaCache
{
  public:
    static constexpr size_t CAPACITY = 32;

    explicit MetaCache(size_t capacity = CAPACITY) : capacity(capacity) { }

    // The live entry for key, made the most recently used; an expired one is dropped.
    std::optional<domain::Meta> get(const std::string& key, Clock::time_point now);

    // Stores meta (replacing any entry), evicting the least recently used over capacity.
    void put(const std::string& key, domain::Meta meta, std::chrono::seconds ttl, Clock::time_point now);

    size_t size() const { return entries.size(); }

  private:
    struct Entry
    {
        domain::Meta meta;
        Clock::time_point expires;
        std::list<std::string>::iterator order;
    };

    size_t capacity;
    std::list<std::string> recent; // front is the most recently used
    std::unordered_map<std::string, Entry> entries;
};

} // namespace nuvio::data::meta
