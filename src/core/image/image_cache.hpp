#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "core/cache/disk_cache.hpp"
#include "core/cache/lru_index.hpp"

namespace nuvio::core::image
{

using Bytes = std::shared_ptr<const std::string>;

// Encoded image bytes in memory, capped by total size, least recently used out first.
// Thread-safe.
class MemoryLru
{
  public:
    explicit MemoryLru(uint64_t maxBytes) : index(maxBytes) { }

    Bytes get(const std::string& key);
    void put(const std::string& key, Bytes bytes);

    uint64_t totalBytes();
    size_t count();

  private:
    std::mutex mutex;
    cache::LruIndex index;
    std::unordered_map<std::string, Bytes> items;
};

// The two cache levels behind ImageLoader, keyed by URL.
class ImageCache
{
  public:
    static constexpr uint64_t MEMORY_BYTES = 32ull * 1024 * 1024;
    static constexpr uint64_t DISK_BYTES   = 100ull * 1024 * 1024;

    ImageCache(const std::string& diskDir, uint64_t memoryBytes = MEMORY_BYTES, uint64_t diskBytes = DISK_BYTES);

    Bytes fromMemory(const std::string& url);

    // Reads the disk cache and promotes a hit to memory.
    Bytes fromDisk(const std::string& url);

    void put(const std::string& url, Bytes bytes);

    void flush() { disk.flush(); }

  private:
    MemoryLru memory;
    cache::DiskCache disk;
};

// Whether bytes start like a format the loader can show (JPEG, PNG, GIF, BMP
// through borealis, WebP through libwebp), so error pages and other junk aren't cached.
bool looksLikeImage(const std::string& bytes);

} // namespace nuvio::core::image
