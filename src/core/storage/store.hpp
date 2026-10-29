#pragma once

#include <memory>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "core/storage/paths.hpp"

namespace nuvio::core::storage
{

// A named key-value store kept as one JSON object in one file, like NuvioTV's
// per-profile DataStore files (data/local/ProfileDataStoreFactory.kt).
// get/set work on the in-memory copy; save() writes it out on the storage thread.
// All methods are thread-safe.
class Store
{
  public:
    using Json = nlohmann::json;

    // The shared instance of a store; profile stores use ProfileContext::current().
    static std::shared_ptr<Store> open(const std::string& name, Scope scope = Scope::Profile);

    // Loads the store at path. A missing file is an empty store. If only
    // <path>.tmp is left from an interrupted save, it is recovered. A file that
    // doesn't parse is renamed to <path>.corrupt and the store starts empty.
    explicit Store(std::string path);

    // The value at key, or null.
    Json get(const std::string& key) const;

    // The value at key as T, or fallback if it is missing or of another type.
    template <typename T>
    T get(const std::string& key, T fallback) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = data.find(key);
        if (it == data.end())
            return fallback;
        try
        {
            return it->get<T>();
        }
        catch (const Json::exception&)
        {
            return fallback;
        }
    }

    bool contains(const std::string& key) const;
    void set(const std::string& key, Json value);
    void remove(const std::string& key);

    // Queues an atomic write of the current contents on the storage thread.
    // Saves are written in the order they were made.
    void save();

    const std::string& path() const { return filePath; }

    // Blocks until every queued save is written.
    static void flush();

    // Flushes and stops the storage thread; later saves are written synchronously.
    static void shutdown();

  private:
    void load();

    std::string filePath;
    mutable std::mutex mutex;
    Json data = Json::object();
};

} // namespace nuvio::core::storage
