#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "core/storage/store.hpp"
#include "data/library/library.hpp"

namespace nuvio::data::library
{

// The active profile's saved items, kept in the store "library":
//   { "items": [SavedLibraryItem] in the order they were added, "sort": key, "filter": key }.
// An item is identified by id plus type. Every method must be called on the UI thread; listeners run
// synchronously after a change.
class LibraryRepository
{
  public:
    using Listener = std::function<void()>;

    struct Config
    {
        std::shared_ptr<core::storage::Store> store;
        std::function<int64_t()> nowMs; // epoch ms; defaults to the system clock
    };

    explicit LibraryRepository(Config config);

    // In stored (provider) order.
    const std::vector<SavedLibraryItem>& items() const { return list; }
    bool contains(const std::string& id, const std::string& type) const;

    // Adds the item at the end, stamped with the current time when it has no addedAt. An item already
    // saved is left as it is. Returns whether it was added.
    bool add(SavedLibraryItem item);
    bool remove(const std::string& id, const std::string& type);

    Sort sort() const { return sortOption; }
    void setSort(Sort sort);
    Filter filter() const { return filterOption; }
    void setFilter(Filter filter);

    int64_t now() const { return config.nowMs(); }

    int subscribe(Listener listener);
    void unsubscribe(int token);

    static LibraryRepository& shared();
    static void setShared(LibraryRepository* repository);
    static bool hasShared();

  private:
    void write();
    void notify();

    Config config;
    std::vector<SavedLibraryItem> list;
    Sort sortOption     = Sort::AddedNewest;
    Filter filterOption = Filter::All;
    std::map<int, Listener> listeners;
    int nextToken = 1;
};

} // namespace nuvio::data::library
