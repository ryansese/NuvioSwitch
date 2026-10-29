#pragma once

#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

namespace nuvio::core::cache
{

// Keys with byte sizes in least-recently-used order, capped at a total size.
// It only does the bookkeeping; callers drop whatever put() evicts.
// Not thread-safe.
class LruIndex
{
  public:
    explicit LruIndex(uint64_t maxBytes) : maxBytes(maxBytes) { }

    // Adds or replaces key as the most recent entry, then evicts the oldest
    // entries until the total fits. Returns the evicted keys (never key itself,
    // unless it alone is larger than the cap).
    std::vector<std::string> put(const std::string& key, uint64_t size);

    // Marks key as the most recent entry. Returns false if it isn't there.
    bool touch(const std::string& key);

    void remove(const std::string& key);
    bool contains(const std::string& key) const { return entries.count(key) != 0; }

    uint64_t totalBytes() const { return total; }
    uint64_t capacity() const { return maxBytes; }
    size_t count() const { return entries.size(); }

    // Keys from least to most recently used.
    std::vector<std::string> keysOldestFirst() const;

  private:
    struct Entry
    {
        uint64_t size;
        std::list<std::string>::iterator position;
    };

    uint64_t maxBytes;
    uint64_t total = 0;
    std::list<std::string> order; // front = most recent
    std::unordered_map<std::string, Entry> entries;
};

} // namespace nuvio::core::cache
