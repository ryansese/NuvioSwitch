#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "core/cache/lru_index.hpp"

namespace nuvio::core::cache
{

// Seconds since the Unix epoch.
int64_t unixNow();

// Bodies on disk, one file per key (<dir>/<key>.body), with an index.json that
// holds each entry's size, last access and caller metadata. Capped at maxBytes
// with least-recently-used eviction. Thread-safe.
class DiskCache
{
  public:
    using Json  = nlohmann::json;
    using Clock = std::function<int64_t()>;

    struct Hit
    {
        std::string body;
        Json meta;
    };

    DiskCache(std::string dir, uint64_t maxBytes, Clock now = unixNow);
    ~DiskCache(); // flush()

    // The body and metadata for key, marking it recently used.
    std::optional<Hit> get(const std::string& key);

    // Stores body under key, replacing any earlier entry, and evicts old entries
    // to stay under the cap. Bodies larger than the whole cap are not stored.
    bool put(const std::string& key, const std::string& body, Json meta);

    void remove(const std::string& key);

    // Writes the index if access times changed since the last write.
    void flush();

    uint64_t totalBytes();
    size_t count();

  private:
    struct Entry
    {
        uint64_t size;
        int64_t lastAccess;
        Json meta;
    };

    void loadIndex();
    void saveIndexLocked();
    void removeLocked(const std::string& key);
    std::string bodyPath(const std::string& key) const;

    std::string dir;
    Clock now;
    std::mutex mutex;
    LruIndex lru;
    std::unordered_map<std::string, Entry> entries;
    bool dirty = false;
};

} // namespace nuvio::core::cache
