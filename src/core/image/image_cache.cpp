#include "core/image/image_cache.hpp"

#include "core/cache/hash.hpp"
#include "core/image/decode.hpp"

namespace nuvio::core::image
{

Bytes MemoryLru::get(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = items.find(key);
    if (it == items.end())
        return nullptr;
    index.touch(key);
    return it->second;
}

void MemoryLru::put(const std::string& key, Bytes bytes)
{
    if (!bytes)
        return;
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& evicted : index.put(key, bytes->size()))
        items.erase(evicted);
    if (index.contains(key))
        items[key] = std::move(bytes);
}

uint64_t MemoryLru::totalBytes()
{
    std::lock_guard<std::mutex> lock(mutex);
    return index.totalBytes();
}

size_t MemoryLru::count()
{
    std::lock_guard<std::mutex> lock(mutex);
    return items.size();
}

ImageCache::ImageCache(const std::string& diskDir, uint64_t memoryBytes, uint64_t diskBytes)
    : memory(memoryBytes), disk(diskDir, diskBytes)
{
}

Bytes ImageCache::fromMemory(const std::string& url)
{
    return memory.get(url);
}

Bytes ImageCache::fromDisk(const std::string& url)
{
    auto hit = disk.get(cache::hashKey(url));
    if (!hit || hit->meta.value("url", std::string()) != url)
        return nullptr;
    auto bytes = std::make_shared<const std::string>(std::move(hit->body));
    memory.put(url, bytes);
    return bytes;
}

void ImageCache::put(const std::string& url, Bytes bytes)
{
    if (!bytes)
        return;
    memory.put(url, bytes);
    disk.put(cache::hashKey(url), *bytes, {{"url", url}});
}

bool looksLikeImage(const std::string& b)
{
    auto starts = [&b](const char* magic, size_t n) { return b.size() >= n && b.compare(0, n, magic, n) == 0; };
    return starts("\xFF\xD8\xFF", 3)            // JPEG
           || starts("\x89PNG\r\n\x1A\n", 8)    // PNG
           || starts("GIF87a", 6) || starts("GIF89a", 6)
           || starts("BM", 2)
           || isWebp(b);
}

} // namespace nuvio::core::image
