#include "core/cache/disk_cache.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <vector>

#include "core/log.hpp"
#include "core/storage/files.hpp"

namespace nuvio::core::cache
{

namespace fs = std::filesystem;

int64_t unixNow()
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

DiskCache::DiskCache(std::string directory, uint64_t maxBytes, Clock clock)
    : dir(std::move(directory)), now(std::move(clock)), lru(maxBytes)
{
    if (!dir.empty() && dir.back() != '/')
        dir += '/';
    storage::ensureDir(dir);
    loadIndex();
}

DiskCache::~DiskCache()
{
    flush();
}

std::string DiskCache::bodyPath(const std::string& key) const
{
    return dir + key + ".body";
}

void DiskCache::loadIndex()
{
    struct Loaded
    {
        std::string key;
        Entry entry;
    };
    std::vector<Loaded> loaded;

    if (auto text = storage::readFile(dir + "index.json"))
    {
        auto index = Json::parse(*text, nullptr, false);
        if (index.is_object())
        {
            for (auto& [key, value] : index.items())
            {
                if (!value.is_object())
                    continue;
                uint64_t size = value.value("size", uint64_t(0));
                std::error_code ec;
                if (fs::file_size(bodyPath(key), ec) != size || ec)
                    continue; // body missing or half-written
                loaded.push_back({key, Entry{size, value.value("lastAccess", int64_t(0)), value.value("meta", Json::object())}});
            }
        }
        else
        {
            log::warning("DiskCache: unreadable index in " + dir + ", starting empty");
        }
    }

    // replay oldest first so the LRU order matches the stored access times
    std::sort(loaded.begin(), loaded.end(),
        [](const Loaded& a, const Loaded& b) { return a.entry.lastAccess < b.entry.lastAccess; });
    for (auto& item : loaded)
    {
        for (const auto& evicted : lru.put(item.key, item.entry.size))
        {
            entries.erase(evicted);
            std::remove(bodyPath(evicted).c_str());
        }
        entries[item.key] = std::move(item.entry);
    }

    // drop bodies the index doesn't know about
    std::error_code ec;
    for (const auto& file : fs::directory_iterator(dir, ec))
    {
        std::string name = file.path().filename().string();
        if (name == "index.json")
            continue;
        std::string suffix = ".body";
        bool known         = name.size() > suffix.size() &&
                     name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0 &&
                     entries.count(name.substr(0, name.size() - suffix.size())) != 0;
        if (!known)
            fs::remove(file.path(), ec);
    }

    saveIndexLocked();
}

void DiskCache::saveIndexLocked()
{
    Json index = Json::object();
    for (const auto& [key, entry] : entries)
        index[key] = {{"size", entry.size}, {"lastAccess", entry.lastAccess}, {"meta", entry.meta}};
    if (!storage::atomicWrite(dir + "index.json", index.dump()))
        log::warning("DiskCache: failed to write the index in " + dir);
    dirty = false;
}

std::optional<DiskCache::Hit> DiskCache::get(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = entries.find(key);
    if (it == entries.end())
        return std::nullopt;

    auto body = storage::readFile(bodyPath(key));
    if (!body || body->size() != it->second.size)
    {
        removeLocked(key);
        saveIndexLocked();
        return std::nullopt;
    }

    lru.touch(key);
    it->second.lastAccess = now();
    dirty                 = true;
    return Hit{std::move(*body), it->second.meta};
}

bool DiskCache::put(const std::string& key, const std::string& body, Json meta)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (body.size() > lru.capacity())
        return false;

    removeLocked(key);
    if (!storage::atomicWrite(bodyPath(key), body))
    {
        saveIndexLocked();
        return false;
    }

    for (const auto& evicted : lru.put(key, body.size()))
        removeLocked(evicted);
    entries[key] = Entry{body.size(), now(), std::move(meta)};
    saveIndexLocked();
    return true;
}

void DiskCache::remove(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (entries.count(key) == 0)
        return;
    removeLocked(key);
    saveIndexLocked();
}

void DiskCache::removeLocked(const std::string& key)
{
    lru.remove(key);
    entries.erase(key);
    std::remove(bodyPath(key).c_str());
}

void DiskCache::flush()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (dirty)
        saveIndexLocked();
}

uint64_t DiskCache::totalBytes()
{
    std::lock_guard<std::mutex> lock(mutex);
    return lru.totalBytes();
}

size_t DiskCache::count()
{
    std::lock_guard<std::mutex> lock(mutex);
    return entries.size();
}

} // namespace nuvio::core::cache
