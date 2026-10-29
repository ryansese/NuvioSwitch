#include "core/storage/store.hpp"

#include <cstdio>
#include <filesystem>
#include <map>
#include <optional>

#include "core/async/worker_pool.hpp"
#include "core/log.hpp"
#include "core/storage/files.hpp"
#include "core/storage/profile_context.hpp"

namespace nuvio::core::storage
{

namespace
{
// One thread, so saves are serialised in the order they were made.
std::mutex writerMutex;
std::unique_ptr<async::WorkerPool> writer;
bool writerStopped = false;

std::mutex registryMutex;
std::map<std::string, std::weak_ptr<Store>> registry;

void writeFile(const std::string& path, const std::string& contents)
{
    ensureDir(std::filesystem::path(path).parent_path().string());
    if (!atomicWrite(path, contents))
        log::error("Store: failed to save " + path);
}

std::optional<nlohmann::json> parseObject(const std::string& text)
{
    auto parsed = nlohmann::json::parse(text, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object())
        return std::nullopt;
    return parsed;
}
} // namespace

std::shared_ptr<Store> Store::open(const std::string& name, Scope scope)
{
    std::string path = Paths::storeFile(name, scope, ProfileContext::current());

    std::lock_guard<std::mutex> lock(registryMutex);
    if (auto existing = registry[path].lock())
        return existing;
    auto store     = std::make_shared<Store>(path);
    registry[path] = store;
    return store;
}

Store::Store(std::string path) : filePath(std::move(path))
{
    load();
}

void Store::load()
{
    std::string tmpPath = filePath + ".tmp";

    if (auto text = readFile(filePath))
    {
        if (auto parsed = parseObject(*text))
        {
            data = std::move(*parsed);
            std::remove(tmpPath.c_str()); // left over from a save that didn't finish
            return;
        }
        std::string corruptPath = filePath + ".corrupt";
        log::error("Store: " + filePath + " is not valid JSON, moved to " + corruptPath);
        std::remove(corruptPath.c_str());
        std::rename(filePath.c_str(), corruptPath.c_str());
    }

    // The FAT fallback in atomicWrite can leave only the .tmp file behind
    if (auto text = readFile(tmpPath))
    {
        if (auto parsed = parseObject(*text))
        {
            log::warning("Store: recovered " + filePath + " from " + tmpPath);
            data = std::move(*parsed);
            std::rename(tmpPath.c_str(), filePath.c_str());
            return;
        }
        std::remove(tmpPath.c_str());
    }
}

Store::Json Store::get(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = data.find(key);
    return it == data.end() ? Json() : *it;
}

bool Store::contains(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(mutex);
    return data.contains(key);
}

void Store::set(const std::string& key, Json value)
{
    std::lock_guard<std::mutex> lock(mutex);
    data[key] = std::move(value);
}

void Store::remove(const std::string& key)
{
    std::lock_guard<std::mutex> lock(mutex);
    data.erase(key);
}

void Store::save()
{
    std::string snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex);
        snapshot = data.dump();
    }

    std::lock_guard<std::mutex> lock(writerMutex);
    if (writerStopped)
    {
        writeFile(filePath, snapshot);
        return;
    }
    if (!writer)
        writer = std::make_unique<async::WorkerPool>(1);
    writer->post([path = filePath, snapshot = std::move(snapshot)] { writeFile(path, snapshot); });
}

void Store::flush()
{
    std::lock_guard<std::mutex> lock(writerMutex);
    if (writer)
        writer->waitIdle();
}

void Store::shutdown()
{
    std::lock_guard<std::mutex> lock(writerMutex);
    if (writer)
        writer->shutdown();
    writer.reset();
    writerStopped = true;
}

} // namespace nuvio::core::storage
